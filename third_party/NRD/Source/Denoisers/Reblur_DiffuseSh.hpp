/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

void nrd::InstanceImpl::Add_ReblurDiffuseSh(DenoiserData& denoiserData)
{
    #define DENOISER_NAME REBLUR_DiffuseSh
    #define DIFF_TEMP1 AsUint(Transient::DIFF_TMP1)
    #define DIFF_TEMP2 AsUint(Transient::DIFF_TMP2)
    #define DIFF_SH_TEMP1 AsUint(Transient::DIFF_SH_TMP1)
    #define DIFF_SH_TEMP2 AsUint(Transient::DIFF_SH_TMP2)

    denoiserData.settings.reblur = ReblurSettings();
    denoiserData.settingsSize = sizeof(denoiserData.settings.reblur);

    uint16_t w = denoiserData.desc.renderWidth;
    uint16_t h = denoiserData.desc.renderHeight;
    uint16_t tilesW = DivideUp(w, 16);
    uint16_t tilesH = DivideUp(h, 16);

    enum class Permanent
    {
        PREV_VIEWZ = PERMANENT_POOL_START,
        PREV_NORMAL_ROUGHNESS,
        PREV_INTERNAL_DATA,
        DIFF_HISTORY,
        DIFF_FAST_HISTORY,
        DIFF_SH_HISTORY,
    };

    AddTextureToPermanentPool( {REBLUR_FORMAT_PREV_VIEWZ, w, h, 1} );
    AddTextureToPermanentPool( {REBLUR_FORMAT_PREV_NORMAL_ROUGHNESS, w, h, 1} );
    AddTextureToPermanentPool( {REBLUR_FORMAT_PREV_INTERNAL_DATA, w, h, 1} );
    AddTextureToPermanentPool( {REBLUR_FORMAT, w, h, 1} );
    AddTextureToPermanentPool( {REBLUR_FORMAT_FAST_HISTORY, w, h, 1} );
    AddTextureToPermanentPool( {REBLUR_FORMAT, w, h, 1} );

    enum class Transient
    {
        DATA1 = TRANSIENT_POOL_START,
        DATA2,
        DIFF_TMP1,
        DIFF_TMP2,
        DIFF_FAST_HISTORY,
        DIFF_SH_TMP1,
        DIFF_SH_TMP2,
        TILES,
    };

    AddTextureToTransientPool( {Format::RG8_UNORM, w, h, 1} );
    AddTextureToTransientPool( {Format::R8_UINT, w, h, 1} );
    AddTextureToTransientPool( {REBLUR_FORMAT, w, h, 1} );
    AddTextureToTransientPool( {REBLUR_FORMAT, w, h, 1} );
    AddTextureToTransientPool( {REBLUR_FORMAT_FAST_HISTORY, w, h, 1} );
    AddTextureToTransientPool( {REBLUR_FORMAT, w, h, 1} );
    AddTextureToTransientPool( {REBLUR_FORMAT, w, h, 1} );
    AddTextureToTransientPool( {Format::R8_UNORM, tilesW, tilesH, 1} );

    REBLUR_SET_SHARED_CONSTANTS;

    for (int i = 0; i < REBLUR_CLASSIFY_TILES_PERMUTATION_NUM; i++)
    {
        PushPass("Classify tiles");
        {
            // Inputs
            PushInput( AsUint(ResourceType::IN_VIEWZ) );

            // Outputs
            PushOutput( AsUint(Transient::TILES) );

            // Shaders
            AddDispatch( REBLUR_ClassifyTiles, REBLUR_CLASSIFY_TILES_CONSTANT_NUM, REBLUR_CLASSIFY_TILES_NUM_THREADS, 1 );
        }
    }

    for (int i = 0; i < REBLUR_HITDIST_RECONSTRUCTION_PERMUTATION_NUM; i++)
    {
        bool is5x5 = ( ( ( i >> 1 ) & 0x1 ) != 0 );
        bool isPrepassEnabled = ( ( ( i >> 0 ) & 0x1 ) != 0 );

        PushPass("Hit distance reconstruction");
        {
            // Inputs
            PushInput( AsUint(Transient::TILES) );
            PushInput( AsUint(ResourceType::IN_NORMAL_ROUGHNESS) );
            PushInput( AsUint(ResourceType::IN_VIEWZ) );
            PushInput( AsUint(ResourceType::IN_DIFF_SH0) );

            // Outputs
            PushOutput( isPrepassEnabled ? DIFF_TEMP2 : DIFF_TEMP1 );

            // Shaders
            if (is5x5)
            {
                AddDispatch( REBLUR_Diffuse_HitDistReconstruction_5x5, REBLUR_HITDIST_RECONSTRUCTION_CONSTANT_NUM, REBLUR_HITDIST_RECONSTRUCTION_NUM_THREADS, 1 );
                AddDispatch( REBLUR_Perf_Diffuse_HitDistReconstruction_5x5, REBLUR_HITDIST_RECONSTRUCTION_CONSTANT_NUM, REBLUR_HITDIST_RECONSTRUCTION_NUM_THREADS, 1 );
            }
            else
            {
                AddDispatch( REBLUR_Diffuse_HitDistReconstruction, REBLUR_HITDIST_RECONSTRUCTION_CONSTANT_NUM, REBLUR_HITDIST_RECONSTRUCTION_NUM_THREADS, 1 );
                AddDispatch( REBLUR_Perf_Diffuse_HitDistReconstruction, REBLUR_HITDIST_RECONSTRUCTION_CONSTANT_NUM, REBLUR_HITDIST_RECONSTRUCTION_NUM_THREADS, 1 );
            }
        }
    }

    for (int i = 0; i < REBLUR_PREPASS_PERMUTATION_NUM; i++)
    {
        bool isAfterReconstruction = ( ( ( i >> 0 ) & 0x1 ) != 0 );

        PushPass("Pre-pass");
        {
            // Inputs
            PushInput( AsUint(Transient::TILES) );
            PushInput( AsUint(ResourceType::IN_NORMAL_ROUGHNESS) );
            PushInput( AsUint(ResourceType::IN_VIEWZ) );
            PushInput( isAfterReconstruction ? DIFF_TEMP2 : AsUint(ResourceType::IN_DIFF_SH0) );
            PushInput( AsUint(ResourceType::IN_DIFF_SH1) );

            // Outputs
            PushOutput( DIFF_TEMP1 );
            PushOutput( DIFF_SH_TEMP1 );

            // Shaders
            AddDispatch( REBLUR_DiffuseSh_PrePass, REBLUR_PREPASS_CONSTANT_NUM, REBLUR_PREPASS_NUM_THREADS, 1 );
            AddDispatch( REBLUR_Perf_DiffuseSh_PrePass, REBLUR_PREPASS_CONSTANT_NUM, REBLUR_PREPASS_NUM_THREADS, 1 );
        }
    }

    for (int i = 0; i < REBLUR_TEMPORAL_ACCUMULATION_PERMUTATION_NUM; i++)
    {
        bool hasDisocclusionThresholdMix = ( ( ( i >> 3 ) & 0x1 ) != 0 );
        bool isTemporalStabilization = ( ( ( i >> 2 ) & 0x1 ) != 0 );
        bool hasConfidenceInputs = ( ( ( i >> 1 ) & 0x1 ) != 0 );
        bool isAfterPrepass = ( ( ( i >> 0 ) & 0x1 ) != 0 );

        PushPass("Temporal accumulation");
        {
            // Inputs
            PushInput( AsUint(Transient::TILES) );
            PushInput( AsUint(ResourceType::IN_NORMAL_ROUGHNESS) );
            PushInput( AsUint(ResourceType::IN_VIEWZ) );
            PushInput( AsUint(ResourceType::IN_MV) );
            PushInput( AsUint(Permanent::PREV_VIEWZ) );
            PushInput( AsUint(Permanent::PREV_NORMAL_ROUGHNESS) );
            PushInput( AsUint(Permanent::PREV_INTERNAL_DATA) );
            PushInput( hasDisocclusionThresholdMix ? AsUint(ResourceType::IN_DISOCCLUSION_THRESHOLD_MIX) : REBLUR_DUMMY );
            PushInput( hasConfidenceInputs ? AsUint(ResourceType::IN_DIFF_CONFIDENCE) : REBLUR_DUMMY );
            PushInput( isAfterPrepass ? DIFF_TEMP1 : AsUint(ResourceType::IN_DIFF_SH0) );
            PushInput( isTemporalStabilization ? AsUint(Permanent::DIFF_HISTORY) : AsUint(ResourceType::OUT_DIFF_SH0) );
            PushInput( AsUint(Permanent::DIFF_FAST_HISTORY) );
            PushInput( isAfterPrepass ? DIFF_SH_TEMP1 : AsUint(ResourceType::IN_DIFF_SH1) );
            PushInput( isTemporalStabilization ? AsUint(Permanent::DIFF_SH_HISTORY) : AsUint(ResourceType::OUT_DIFF_SH1) );

            // Outputs
            PushOutput( DIFF_TEMP2 );
            PushOutput( AsUint(Transient::DIFF_FAST_HISTORY) );
            PushOutput( AsUint(Transient::DATA1) );
            PushOutput( AsUint(Transient::DATA2) );
            PushOutput( DIFF_SH_TEMP2 );

            // Shaders
            AddDispatch( REBLUR_DiffuseSh_TemporalAccumulation, REBLUR_TEMPORAL_ACCUMULATION_CONSTANT_NUM, REBLUR_TEMPORAL_ACCUMULATION_NUM_THREADS, 1 );
            AddDispatch( REBLUR_Perf_DiffuseSh_TemporalAccumulation, REBLUR_TEMPORAL_ACCUMULATION_CONSTANT_NUM, REBLUR_TEMPORAL_ACCUMULATION_NUM_THREADS, 1 );
        }
    }

    for (int i = 0; i < REBLUR_HISTORY_FIX_PERMUTATION_NUM; i++)
    {
        PushPass("History fix");
        {
            // Inputs
            PushInput( AsUint(Transient::TILES) );
            PushInput( AsUint(ResourceType::IN_NORMAL_ROUGHNESS) );
            PushInput( AsUint(Transient::DATA1) );
            PushInput( AsUint(ResourceType::IN_VIEWZ) );
            PushInput( DIFF_TEMP2 );
            PushInput( AsUint(Transient::DIFF_FAST_HISTORY) );
            PushInput( DIFF_SH_TEMP2 );

            // Outputs
            PushOutput( DIFF_TEMP1 );
            PushOutput( AsUint(Permanent::DIFF_FAST_HISTORY) );
            PushOutput( DIFF_SH_TEMP1 );

            // Shaders
            AddDispatch( REBLUR_DiffuseSh_HistoryFix, REBLUR_HISTORY_FIX_CONSTANT_NUM, REBLUR_HISTORY_FIX_NUM_THREADS, 1 );
            AddDispatch( REBLUR_Perf_DiffuseSh_HistoryFix, REBLUR_HISTORY_FIX_CONSTANT_NUM, REBLUR_HISTORY_FIX_NUM_THREADS, 1 );
        }
    }

    for (int i = 0; i < REBLUR_BLUR_PERMUTATION_NUM; i++)
    {
        PushPass("Blur");
        {
            // Inputs
            PushInput( AsUint(Transient::TILES) );
            PushInput( AsUint(ResourceType::IN_NORMAL_ROUGHNESS) );
            PushInput( AsUint(Transient::DATA1) );
            PushInput( DIFF_TEMP1 );
            PushInput( AsUint(ResourceType::IN_VIEWZ) );
            PushInput( DIFF_SH_TEMP1 );

            // Outputs
            PushOutput( DIFF_TEMP2 );
            PushOutput( AsUint(Permanent::PREV_VIEWZ) );
            PushOutput( DIFF_SH_TEMP2 );

            // Shaders
            AddDispatch( REBLUR_DiffuseSh_Blur, REBLUR_BLUR_CONSTANT_NUM, REBLUR_BLUR_NUM_THREADS, 1 );
            AddDispatch( REBLUR_Perf_DiffuseSh_Blur, REBLUR_BLUR_CONSTANT_NUM, REBLUR_BLUR_NUM_THREADS, 1 );
        }
    }

    for (int i = 0; i < REBLUR_POST_BLUR_PERMUTATION_NUM; i++)
    {
        bool isTemporalStabilization = ( ( ( i >> 0 ) & 0x1 ) != 0 );

        PushPass("Post-blur");
        {
            // Inputs
            PushInput( AsUint(Transient::TILES) );
            PushInput( AsUint(ResourceType::IN_NORMAL_ROUGHNESS) );
            PushInput( AsUint(Transient::DATA1) );
            PushInput( DIFF_TEMP2 );
            PushInput( AsUint(Permanent::PREV_VIEWZ) );
            PushInput( DIFF_SH_TEMP2 );

            // Outputs
            PushOutput( AsUint(Permanent::PREV_NORMAL_ROUGHNESS) );

            if (isTemporalStabilization)
            {
                PushOutput( AsUint(Permanent::DIFF_HISTORY) );
                PushOutput( AsUint(Permanent::DIFF_SH_HISTORY) );
            }
            else
            {
                PushOutput( AsUint(ResourceType::OUT_DIFF_SH0) );
                PushOutput( AsUint(Permanent::PREV_INTERNAL_DATA) );
                PushOutput( AsUint(ResourceType::OUT_DIFF_SH1) );
            }

            // Shaders
            if (isTemporalStabilization)
            {
                AddDispatch( REBLUR_DiffuseSh_PostBlur, REBLUR_POST_BLUR_CONSTANT_NUM, REBLUR_POST_BLUR_NUM_THREADS, 1 );
                AddDispatch( REBLUR_Perf_DiffuseSh_PostBlur, REBLUR_POST_BLUR_CONSTANT_NUM, REBLUR_POST_BLUR_NUM_THREADS, 1 );
            }
            else
            {
                AddDispatch( REBLUR_DiffuseSh_PostBlur_NoTemporalStabilization, REBLUR_POST_BLUR_CONSTANT_NUM, REBLUR_POST_BLUR_NUM_THREADS, 1 );
                AddDispatch( REBLUR_Perf_DiffuseSh_PostBlur_NoTemporalStabilization, REBLUR_POST_BLUR_CONSTANT_NUM, REBLUR_POST_BLUR_NUM_THREADS, 1 );
            }
        }
    }

    for (int i = 0; i < REBLUR_COPY_STABILIZED_HISTORY_PERMUTATION_NUM; i++)
    {
        PushPass("Copy stabilized history");
        {
            // Inputs
            PushInput( AsUint(Transient::TILES) );
            PushInput( AsUint(ResourceType::OUT_DIFF_SH0) );
            PushInput( AsUint(ResourceType::OUT_DIFF_SH1) );

            // Outputs
            PushOutput( DIFF_TEMP2 );
            PushOutput( DIFF_SH_TEMP2 );

            // Shaders
            AddDispatch( REBLUR_DiffuseSh_CopyStabilizedHistory, REBLUR_COPY_STABILIZED_HISTORY_CONSTANT_NUM, REBLUR_COPY_STABILIZED_HISTORY_NUM_THREADS, USE_MAX_DIMS );
        }
    }

    for (int i = 0; i < REBLUR_TEMPORAL_STABILIZATION_PERMUTATION_NUM; i++)
    {
        PushPass("Temporal stabilization");
        {
            // Inputs
            PushInput( AsUint(Transient::TILES) );
            PushInput( AsUint(ResourceType::IN_NORMAL_ROUGHNESS) );
            PushInput( AsUint(Permanent::PREV_VIEWZ) );
            PushInput( AsUint(Transient::DATA1) );
            PushInput( AsUint(Transient::DATA2) );
            PushInput( AsUint(Permanent::DIFF_HISTORY) );
            PushInput( DIFF_TEMP2 );
            PushInput( AsUint(Permanent::DIFF_SH_HISTORY) );
            PushInput( DIFF_SH_TEMP2 );

            // Outputs
            PushOutput( AsUint(ResourceType::IN_MV) );
            PushOutput( AsUint(Permanent::PREV_INTERNAL_DATA) );
            PushOutput( AsUint(ResourceType::OUT_DIFF_SH0) );
            PushOutput( AsUint(ResourceType::OUT_DIFF_SH1) );

            // Shaders
            AddDispatch( REBLUR_DiffuseSh_TemporalStabilization, REBLUR_TEMPORAL_STABILIZATION_CONSTANT_NUM, REBLUR_TEMPORAL_STABILIZATION_NUM_THREADS, 1 );
            AddDispatch( REBLUR_Perf_DiffuseSh_TemporalStabilization, REBLUR_TEMPORAL_STABILIZATION_CONSTANT_NUM, REBLUR_TEMPORAL_STABILIZATION_NUM_THREADS, 1 );
        }
    }

    for (int i = 0; i < REBLUR_SPLIT_SCREEN_PERMUTATION_NUM; i++)
    {
        PushPass("Split screen");
        {
            // Inputs
            PushInput( AsUint(ResourceType::IN_VIEWZ) );
            PushInput( AsUint(ResourceType::IN_DIFF_SH0) );
            PushInput( AsUint(ResourceType::IN_DIFF_SH1) );

            // Outputs
            PushOutput( AsUint(ResourceType::OUT_DIFF_SH0) );
            PushOutput( AsUint(ResourceType::OUT_DIFF_SH1) );

            // Shaders
            AddDispatch( REBLUR_DiffuseSh_SplitScreen, REBLUR_SPLIT_SCREEN_CONSTANT_NUM, REBLUR_SPLIT_SCREEN_NUM_THREADS, 1 );
        }
    }

    REBLUR_ADD_VALIDATION_DISPATCH( Transient::DATA2, ResourceType::IN_DIFF_SH0, ResourceType::IN_DIFF_SH0 );

    #undef DENOISER_NAME
    #undef DIFF_TEMP1
    #undef DIFF_TEMP2
    #undef DIFF_SH_TEMP1
    #undef DIFF_SH_TEMP2
}
