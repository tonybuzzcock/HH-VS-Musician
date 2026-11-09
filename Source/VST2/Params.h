// Params.h
// Parameter table shared by the VST wrapper and the editor.
//
// Order follows the amp's front panel left to right, then the plugin-only
// controls. Labels follow the owner's naming: the Volume channel is Channel 1
// and the Gain/V-S channel is Channel 2 (the service drawing numbers them the
// other way round, which is what the DSP code uses).

#ifndef TBVS_PARAMS_H
#define TBVS_PARAMS_H

namespace tbvs
{

enum Param
{
    // Front panel, left to right
    kCh1Volume = 0, kCh1Bass, kCh1Treble, kCh1Reverb,
    kCh2Gain, kCh2Bass, kMiddle, kCh2Treble, kPresence, kCh2Reverb,
    kVoice, kValveSound, kMasterReverb, kMasterVolume,
    // Plugin only (bottom strip of the editor)
    kChannel, kSpeaker, kInput, kOutput,
    kNumParams
};

enum Kind { kKnob, kTone, kSwitch, kVoiceSw, kChannelSel, kDb12, kDb24 };

const char* const kParamNames[kNumParams] = {
    "1 Volume", "1 Bass", "1 Treble", "1 Reverb",
    "2 Gain", "2 Bass", "Middle", "2 Treble", "Presence", "2 Reverb",
    "Voice", "ValveSnd", "M Reverb", "M Volume",
    "Channel", "Speaker", "Input", "Output"
};

const int kParamKind[kNumParams] = {
    kKnob, kTone, kTone, kSwitch,
    kKnob, kTone, kTone, kTone, kTone, kSwitch,
    kVoiceSw, kSwitch, kKnob, kKnob,
    kChannelSel, kSwitch, kDb12, kDb24
};

const float kDefaults[kNumParams] = {
    0.5f, 0.5f, 0.5f, 0.0f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f,
    0.0f, 0.0f, 0.3f, 0.35f,
    0.5f, 1.0f, 0.5f, 0.5f
};

const double kInputRangeDb  = 12.0;
const double kOutputRangeDb = 24.0;

inline bool isOn (float v) { return v >= 0.5f; }

// Channel selector positions read 1 / 2 / 1+2.
inline int channelSlot (float v) { return v < (1.0f / 3.0f) ? 0 : (v < (2.0f / 3.0f) ? 1 : 2); }
inline float channelSlotValue (int slot) { return slot <= 0 ? 0.0f : (slot == 1 ? 0.5f : 1.0f); }

} // namespace tbvs

#endif
