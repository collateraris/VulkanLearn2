#pragma once

#define INPUT_FEATURES 14
// EncodeFrequency expands the input by 6 per input feature
#define FREQUENCY_EXPANSION 6
#define INPUT_NEURONS (INPUT_FEATURES * FREQUENCY_EXPANSION) // 6* from Frequency Encoding
#define OUTPUT_NEURONS 4


#define HIDDEN_NEURONS 32
#define NUM_HIDDEN_LAYERS 3
#define BATCH_SIZE (1 << 16)
#define BATCH_COUNT 1

#define LEARNING_RATE 1e-4f
#define COMPONENT_WEIGHTS float4(1.f, 1.f, 1.f, 0.f)

#define NUM_TRANSITIONS (NUM_HIDDEN_LAYERS + 1)
#define NUM_TRANSITIONS_ALIGN4 ((NUM_TRANSITIONS + 3) / 4)
#define LOSS_SCALE 128.0
#define NRC_GRADIENT_BUCKET_COUNT 32
#define NRC_GRADIENT_BUCKET_BITS 4
#define NRC_CACHE_BLEND_MAX 0.25f

struct TrainingConstantBufferEntry
{
    uint4 weightOffsets[NUM_TRANSITIONS_ALIGN4];
    uint4 biasOffsets[NUM_TRANSITIONS_ALIGN4];
    uint4 gradientWeightOffsets[NUM_TRANSITIONS_ALIGN4];
    uint4 gradientBiasOffsets[NUM_TRANSITIONS_ALIGN4];
    uint32_t maxParamSize;
    float learningRate;
    float currentStep;
    uint32_t batchSize;
    uint64_t seed;
};
