#pragma once

#include <vk_types.h>

#define INPUT_FEATURES 9
// EncodeFrequency expands the input by 6 per input feature
#define FREQUENCY_EXPANSION 6
#define INPUT_NEURONS (INPUT_FEATURES * FREQUENCY_EXPANSION) // 6* from Frequency Encoding
#define OUTPUT_NEURONS 4


#define HIDDEN_NEURONS 64
#define NUM_HIDDEN_LAYERS 7
#define BATCH_SIZE (1 << 16)
#define BATCH_COUNT 1

#define LEARNING_RATE 1e-4f
#define COMPONENT_WEIGHTS float4(1.f, 1.f, 1.f, 1.0)

#define NUM_TRANSITIONS (NUM_HIDDEN_LAYERS + 1)
#define NUM_TRANSITIONS_ALIGN4 ((NUM_TRANSITIONS + 3) / 4)
#define LOSS_SCALE 128.0

struct TrainingConstantBufferEntry
{
    glm::uvec4 weightOffsets[NUM_TRANSITIONS_ALIGN4];
    glm::uvec4 biasOffsets[NUM_TRANSITIONS_ALIGN4];
    uint32_t maxParamSize;
    float learningRate;
    float currentStep;
    uint32_t batchSize;
    uint64_t seed;
};