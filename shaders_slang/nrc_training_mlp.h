// NRC uses FP16 matrix multiplication (supported by Ada) and independently
// laid-out FP32 gradient accumulators. Keep this explicit backward pass in
// sync with the ReLU hidden layers and linear output used by inference.
CoopVec<half, K> nrcBackwardLinear<let M : int, let K : int>(
    CoopVec<half, K> input, CoopVec<half, M> gradient, uint layer, uint bucket)
{
    var gradient32 = CoopVec<float, M>(0.f);
    [ForceUnroll] for (int i = 0; i < M; ++i) gradient32[i] = float(gradient[i]);
    // NetworkUtilities rounds the allocation after the final FP32 bias to 64
    // bytes. Each scale bucket contains one complete such matrix/bias layout.
    uint stride = (gConst.gradientBiasOffsets[NUM_HIDDEN_LAYERS / 4][NUM_HIDDEN_LAYERS % 4] + OUTPUT_NEURONS * 4 + 63) & ~63u;
    uint weightOffset = gConst.weightOffsets[layer / 4][layer % 4];
    uint gradientWeightOffset = bucket * stride + gConst.gradientWeightOffsets[layer / 4][layer % 4];
    uint gradientBiasOffset = bucket * stride + gConst.gradientBiasOffsets[layer / 4][layer % 4];
    // Vulkan requires FP16 A/B operands even for an FP32 destination matrix.
    coopVecOuterProductAccumulate(gradient, input, gMLPParamsGradients,
        gradientWeightOffset, 0, CoopVecMatrixLayout::TrainingOptimal, CoopVecComponentType::Float32);
    coopVecReduceSumAccumulate(gradient32, gMLPParamsGradients, gradientBiasOffset);
    return coopVecMatMul<half, K>(gradient, CoopVecComponentType::Float16, gMLPParams,
        weightOffset, CoopVecComponentType::Float16, CoopVecMatrixLayout::TrainingOptimal, true, 0);
}

void nrcTrainSample(CoopVec<half, INPUT_NEURONS> input, float3 target)
{
    uint[NUM_TRANSITIONS] weights = rtxns::UnpackArray<NUM_TRANSITIONS_ALIGN4, NUM_TRANSITIONS>(gConst.weightOffsets);
    uint[NUM_TRANSITIONS] biases = rtxns::UnpackArray<NUM_TRANSITIONS_ALIGN4, NUM_TRANSITIONS>(gConst.biasOffsets);
    CoopVec<half, HIDDEN_NEURONS> activations[NUM_HIDDEN_LAYERS];
    activations[0] = rtxns::relu(rtxns::LinearOp<half, HIDDEN_NEURONS, INPUT_NEURONS>(
        input, gMLPParams, weights[0], biases[0], CoopVecMatrixLayout::TrainingOptimal, CoopVecComponentType::Float16));
    [ForceUnroll] for (uint layer = 1; layer < NUM_HIDDEN_LAYERS; ++layer)
        activations[layer] = rtxns::relu(rtxns::LinearOp<half, HIDDEN_NEURONS, HIDDEN_NEURONS>(
            activations[layer - 1], gMLPParams, weights[layer], biases[layer],
            CoopVecMatrixLayout::TrainingOptimal, CoopVecComponentType::Float16));
    var output = rtxns::LinearOp<half, OUTPUT_NEURONS, HIDDEN_NEURONS>(
        activations[NUM_HIDDEN_LAYERS - 1], gMLPParams, weights[NUM_HIDDEN_LAYERS], biases[NUM_HIDDEN_LAYERS],
        CoopVecMatrixLayout::TrainingOptimal, CoopVecComponentType::Float16);
    float3 prediction = float3(output[0], output[1], output[2]);
    if (!all(isfinite(prediction))) return;

    float scale = LOSS_SCALE / (float(gConst.batchSize) * 3.f);
    float3 lossGradient = 2.f * scale * (prediction - target) / (prediction * prediction + 1.f);
    // Preserve bright paths without overflowing FP16 operands: gradients are
    // scaled by powers of 16 and accumulated in separate FP32 buckets. Adam
    // restores each bucket's scale AFTER summation; no sample is clipped.
    float magnitude = max(max(abs(lossGradient.x), abs(lossGradient.y)), abs(lossGradient.z));
    uint bucket = uint(clamp(ceil(log2(max(magnitude * 8.f, 1.f)) / float(NRC_GRADIENT_BUCKET_BITS)),
        0.f, float(NRC_GRADIENT_BUCKET_COUNT - 1)));
    float backwardScale = exp2(-float(bucket * NRC_GRADIENT_BUCKET_BITS));
    lossGradient *= backwardScale;
    var gradient = nrcBackwardLinear<OUTPUT_NEURONS, HIDDEN_NEURONS>(activations[NUM_HIDDEN_LAYERS - 1],
        CoopVec<half, OUTPUT_NEURONS>(half(lossGradient.x), half(lossGradient.y), half(lossGradient.z), half(0.f)),
        NUM_HIDDEN_LAYERS, bucket);
    [ForceUnroll] for (int layer = NUM_HIDDEN_LAYERS - 1; layer > 0; --layer)
    {
        [ForceUnroll] for (int i = 0; i < HIDDEN_NEURONS; ++i)
            gradient[i] = activations[layer][i] > half(0.f) ? gradient[i] : half(0.f);
        gradient = nrcBackwardLinear<HIDDEN_NEURONS, HIDDEN_NEURONS>(activations[layer - 1], gradient, layer, bucket);
    }
    [ForceUnroll] for (int i = 0; i < HIDDEN_NEURONS; ++i)
        gradient[i] = activations[0][i] > half(0.f) ? gradient[i] : half(0.f);
    nrcBackwardLinear<HIDDEN_NEURONS, INPUT_NEURONS>(input, gradient, 0, bucket);
}
