#define INPUT_FEATURES 9
#define INPUT_NEURONS (INPUT_FEATURES * 6) // 6* from Frequency Encoding
#define OUTPUT_NEURONS 4

#define HIDDEN_NEURONS 32
#define NUM_HIDDEN_LAYERS 3
#define BATCH_SIZE (1 << 16)
#define BATCH_COUNT 100

#define LEARNING_RATE 1e-2f
#define COMPONENT_WEIGHTS float4(1.f, 10.f, 1.f, 5.f)

#define NUM_TRANSITIONS (NUM_HIDDEN_LAYERS + 1)
#define NUM_TRANSITIONS_ALIGN4 ((NUM_TRANSITIONS + 3) / 4)
#define LOSS_SCALE 128.0

struct TrainingConstantBufferEntry
{
    uint4 weightOffsets[NUM_TRANSITIONS_ALIGN4];
    uint4 biasOffsets[NUM_TRANSITIONS_ALIGN4];
    uint32_t maxParamSize;
    float learningRate;
    float currentStep;
    uint32_t batchSize;
    uint64_t seed;
};