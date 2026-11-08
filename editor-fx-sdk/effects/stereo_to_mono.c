// PTFX_NAME: Stereo to Mono
// PTFX_AUTHOR: Polyend
// PTFX_FLAGS: STEREO, CONVERT_TO_MONO
// PTFX_SCRATCH: 0

#include "tracker_plugin.h"
#include "../../core/effector/sampleProcessing/PcmEffects.h"

// Knobs: Source Channel (String List) & Gain Trim
const PluginParam PLUGIN_PARAMS[2] = {
    PTFX_PARAM_LIST("Source", 0, "Merge L+R|Left Only|Right Only|Side L-R"),
    PTFX_PARAM_FLOAT("Gain", -12.0f, 6.0f, 0.0f, 0.5f, "dB")
};

static const PtfxHostApi* s_host = 0;
static uint8_t s_mode = 0;
static float   s_gain = 1.0f;

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    (void)scratch_mem;
    (void)scratch_size;
    s_host = host;
    s_mode = 0;
    s_gain = 1.0f;
}

void plugin_set_param(uint8_t index, float value) {
    if (index == 0) {
        s_mode = (uint8_t)value;
    } else if (index == 1) {
        if (value == 0.0f) {
            s_gain = 1.0f;
        } else if (s_host && s_host->powf) {
            s_gain = s_host->powf(10.0f, value / 20.0f);
        }
    }
}

// Processes 1 channel on MK66, or converts 2 channels on Tracker Mini / Tracker+
int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    if (!in || !out || !in[0] || !out[0]) {
        return 0;
    }

    const int stereo=channels>=2&&in[1]&&in[0]!=in[1];
    for(uint32_t i=0;i<samples;++i){
        const int16_t pcm=ce_pcm_mono(in[0][i],stereo?in[1][i]:in[0][i],s_mode,s_gain,stereo);
        out[0][i]=pcm;if(channels>1&&out[1])out[1][i]=pcm;
    }
    return samples;
}
