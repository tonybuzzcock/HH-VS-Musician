// VSMusicianVST.cpp
// VST 2.4 wrapper for HH VS Musician (Reverb).
//
// - Parameters follow the amp's front panel left to right, then four
//   plugin-only controls (Channel, Speaker, Input, Output).
// - Stereo in/out; each side runs its own amp. A mono source on a stereo insert
//   runs once and is copied, halving CPU.
// - No custom editor yet: Blue Cat PatchWork shows its generic controls.
// - Reports the oversampler latency and the spring reverb tail.
// - Plain C++98, no frameworks: builds for Mac OS X 10.8.

#include "vst2_abi.h"
#include "../DSP/MusicianAmp.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64)
  #include <xmmintrin.h>
  #define TBVS_HAVE_SSE 1
#endif

namespace
{

const VstInt32 kUniqueID    = VST_FOURCC ('T', 'B', 'v', 's');
const VstInt32 kVersion     = 1000;       // 1.0.0
const VstInt32 kNumChannels = 2;
const char* const kPluginName = "HH VS Musician (Reverb)";

enum Param
{
    // Front panel, left to right
    kCh2Volume = 0, kCh2Bass, kCh2Treble, kCh2Reverb,
    kCh1Gain, kCh1Bass, kMiddle, kCh1Treble, kPresence, kCh1Reverb,
    kVoice, kValveSound, kMasterReverb, kMasterVolume,
    // Plugin only
    kChannel, kSpeaker, kInput, kOutput,
    kNumParams
};

enum Kind { kKnob, kTone, kSwitch, kVoiceSw, kChannelSel, kDb12, kDb24 };

const char* const kParamNames[kNumParams] = {
    "2 Volume", "2 Bass", "2 Treble", "2 Reverb",
    "1 Gain", "1 Bass", "Middle", "1 Treble", "Presence", "1 Reverb",
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
    0.0f, 1.0f, 0.5f, 0.5f
};

const double kInputRangeDb  = 12.0;
const double kOutputRangeDb = 24.0;

inline bool isOn (float v) { return v >= 0.5f; }
inline int channelOf (float v) { return v < (1.0f / 3.0f) ? 0 : (v < (2.0f / 3.0f) ? 1 : 2); }

void copyString (char* dst, const char* src, size_t maxLen)
{
    std::strncpy (dst, src, maxLen);
    dst[maxLen] = 0;
}

class VSMusicianVST
{
public:
    explicit VSMusicianVST (audioMasterCallback)
        : sampleRate (44100.0), channel1Stale (false)
    {
        std::memset (&effect, 0, sizeof (effect));
        effect.magic = kEffectMagic;
        effect.dispatcher = &dispatcherProc;
        effect.process = &processAccumulateProc;
        effect.setParameter = &setParameterProc;
        effect.getParameter = &getParameterProc;
        effect.processReplacing = &processReplacingProc;
        effect.processDoubleReplacing = 0;
        effect.numPrograms = 1;
        effect.numParams = kNumParams;
        effect.numInputs = kNumChannels;
        effect.numOutputs = kNumChannels;
        effect.flags = effFlagsCanReplacing;
        effect.initialDelay = hhvs::MusicianAmp::latencySamples();
        effect.ioRatio = 1.0f;
        effect.object = this;
        effect.uniqueID = kUniqueID;
        effect.version = kVersion;

        for (int i = 0; i < kNumParams; ++i)
            params[i] = kDefaults[i];

        copyString (programName, "Default", kVstMaxProgNameLen);
        applyAllParams();
        prepare();
    }

    AEffect* getAEffect() { return &effect; }

private:
    //==========================================================================
    static VSMusicianVST* self (AEffect* e) { return static_cast<VSMusicianVST*> (e->object); }

    static VstIntPtr VST_CALL dispatcherProc (AEffect* e, VstInt32 opcode, VstInt32 index,
                                              VstIntPtr value, void* ptr, float opt)
    {
        VSMusicianVST* p = self (e);
        if (opcode == effClose)
        {
            delete p;
            return 1;
        }
        return p->dispatch (opcode, index, value, ptr, opt);
    }

    static void VST_CALL setParameterProc (AEffect* e, VstInt32 index, float value)
    {
        self (e)->setParameter (index, value);
    }

    static float VST_CALL getParameterProc (AEffect* e, VstInt32 index)
    {
        VSMusicianVST* p = self (e);
        return (index >= 0 && index < kNumParams) ? p->params[index] : 0.0f;
    }

    static void VST_CALL processReplacingProc (AEffect* e, float** in, float** out, VstInt32 n)
    {
        self (e)->processReplacing (in, out, n);
    }

    static void VST_CALL processAccumulateProc (AEffect* e, float** in, float** out, VstInt32 n)
    {
        VSMusicianVST* p = self (e);
        float* tmp[kNumChannels];
        for (int c = 0; c < kNumChannels; ++c)
            tmp[c] = p->scratch[c];

        VstInt32 done = 0;
        while (done < n)
        {
            const VstInt32 chunk = (n - done) < kScratch ? (n - done) : kScratch;
            float* ins[kNumChannels];
            for (int c = 0; c < kNumChannels; ++c)
                ins[c] = in[c] + done;
            p->processReplacing (ins, tmp, chunk);
            for (int c = 0; c < kNumChannels; ++c)
                for (VstInt32 i = 0; i < chunk; ++i)
                    out[c][done + i] += tmp[c][i];
            done += chunk;
        }
    }

    //==========================================================================
    VstIntPtr dispatch (VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
    {
        switch (opcode)
        {
            case effOpen:              return 0;
            case effSetProgram:        return 0;
            case effGetProgram:        return 0;
            case effSetProgramName:
                if (ptr) copyString (programName, (const char*) ptr, kVstMaxProgNameLen);
                return 0;
            case effGetProgramName:
                if (ptr) copyString ((char*) ptr, programName, kVstMaxProgNameLen);
                return 0;
            case effGetProgramNameIndexed:
                if (ptr && index == 0) { copyString ((char*) ptr, programName, kVstMaxProgNameLen); return 1; }
                return 0;

            case effGetParamLabel:
                if (ptr && index >= 0 && index < kNumParams)
                {
                    const int k = kParamKind[index];
                    copyString ((char*) ptr, (k == kDb12 || k == kDb24) ? "dB" : "", kVstMaxParamStrLen);
                }
                return 0;
            case effGetParamDisplay:
                if (ptr && index >= 0 && index < kNumParams)
                    formatParam (index, (char*) ptr);
                return 0;
            case effGetParamName:
                if (ptr && index >= 0 && index < kNumParams)
                    copyString ((char*) ptr, kParamNames[index], kVstMaxParamStrLen);
                return 0;
            case effCanBeAutomated:
                return (index >= 0 && index < kNumParams) ? 1 : 0;

            case effSetSampleRate:
                if (opt > 0.0f) sampleRate = opt;
                return 0;
            case effSetBlockSize:
                return 0;
            case effMainsChanged:
                if (value) prepare();
                return 0;

            case effGetPlugCategory:   return kPlugCategEffect;
            case effGetEffectName:
                if (ptr) copyString ((char*) ptr, kPluginName, kVstMaxEffectNameLen);
                return 1;
            case effGetProductString:
                if (ptr) copyString ((char*) ptr, kPluginName, kVstMaxProductStrLen);
                return 1;
            case effGetVendorString:
                if (ptr) copyString ((char*) ptr, "TB", kVstMaxVendorStrLen);
                return 1;
            case effGetVendorVersion:  return kVersion;
            case effGetVstVersion:     return 2400;

            case effCanDo:
                if (ptr)
                {
                    const char* s = (const char*) ptr;
                    if (! std::strcmp (s, "plugAsChannelInsert") || ! std::strcmp (s, "plugAsSend"))
                        return 1;
                    if (! std::strcmp (s, "receiveVstEvents") || ! std::strcmp (s, "receiveVstMidiEvent")
                        || ! std::strcmp (s, "sendVstEvents") || ! std::strcmp (s, "sendVstMidiEvent"))
                        return -1;
                }
                return 0;

            case effGetTailSize:       return (VstIntPtr) (sampleRate * 3.0);   // spring reverb decay
            case effStartProcess:      return 0;
            case effStopProcess:       return 0;
            default:                   return 0;
        }
    }

    void formatParam (int index, char* text) const
    {
        char buf[32];
        const float v = params[index];
        switch (kParamKind[index])
        {
            case kKnob:       std::sprintf (buf, "%.1f", v * 10.0); break;
            case kTone:
            {
                double t = (v - 0.5) * 10.0;
                if (std::fabs (t) < 0.05) t = 0.0;
                std::sprintf (buf, t == 0.0 ? "0" : "%+.1f", t);
                break;
            }
            case kSwitch:     std::strcpy (buf, isOn (v) ? "On" : "Off"); break;
            case kVoiceSw:    std::strcpy (buf, isOn (v) ? "Heavy" : "Light"); break;
            case kChannelSel:
            {
                const int c = channelOf (v);
                std::strcpy (buf, c == 0 ? "1" : (c == 1 ? "2" : "1+2"));
                break;
            }
            case kDb12:       std::sprintf (buf, "%+.1f", (v * 2.0 - 1.0) * kInputRangeDb); break;
            case kDb24:       std::sprintf (buf, "%+.1f", (v * 2.0 - 1.0) * kOutputRangeDb); break;
            default:          buf[0] = 0; break;
        }
        copyString (text, buf, kVstMaxParamStrLen);
    }

    void setParameter (VstInt32 index, float value)
    {
        if (index < 0 || index >= kNumParams) return;
        if (! (value == value)) return;   // NaN guard
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        params[index] = value;
        applyParam (index);
    }

    void applyParam (int index)
    {
        const double v = params[index];
        const bool on = isOn (params[index]);
        for (int c = 0; c < kNumChannels; ++c)
        {
            hhvs::MusicianAmp& a = amp[c];
            switch (index)
            {
                case kCh2Volume:    a.setCh2Volume (v); break;
                case kCh2Bass:      a.setCh2Bass (v); break;
                case kCh2Treble:    a.setCh2Treble (v); break;
                case kCh2Reverb:    a.setCh2Reverb (on); break;
                case kCh1Gain:      a.setCh1Gain (v); break;
                case kCh1Bass:      a.setCh1Bass (v); break;
                case kMiddle:       a.setMiddle (v); break;
                case kCh1Treble:    a.setCh1Treble (v); break;
                case kPresence:     a.setPresence (v); break;
                case kCh1Reverb:    a.setCh1Reverb (on); break;
                case kVoice:        a.setVoiceHeavy (on); break;
                case kValveSound:   a.setValveSound (on); break;
                case kMasterReverb: a.setMasterReverb (v); break;
                case kMasterVolume: a.setMasterVolume (v); break;
                case kChannel:      a.setChannel (channelOf (params[index])); break;
                case kSpeaker:      a.setSpeaker (on); break;
                case kInput:        a.setInputGainDb ((v * 2.0 - 1.0) * kInputRangeDb); break;
                case kOutput:       a.setOutputGainDb ((v * 2.0 - 1.0) * kOutputRangeDb); break;
                default: break;
            }
        }
    }

    void applyAllParams()
    {
        for (int i = 0; i < kNumParams; ++i)
            applyParam (i);
    }

    void prepare()
    {
        for (int c = 0; c < kNumChannels; ++c)
            amp[c].prepare (sampleRate);
    }

    void processReplacing (float** in, float** out, VstInt32 n)
    {
        if (n <= 0 || ! in || ! out) return;

#if TBVS_HAVE_SSE
        const unsigned int oldCsr = _mm_getcsr();
        _mm_setcsr (oldCsr | 0x8040);   // flush denormals to zero
#endif
        const bool sameInput = (in[0] == in[1])
                            || std::memcmp (in[0], in[1], (size_t) n * sizeof (float)) == 0;

        if (sameInput)
        {
            amp[0].process (in[0], out[0], (int) n);
            channel1Stale = true;
            if (out[1] != out[0])
                std::memcpy (out[1], out[0], (size_t) n * sizeof (float));
        }
        else
        {
            if (channel1Stale)
            {
                amp[1] = amp[0];
                channel1Stale = false;
            }
            amp[0].process (in[0], out[0], (int) n);
            amp[1].process (in[1], out[1], (int) n);
        }

#if TBVS_HAVE_SSE
        _mm_setcsr (oldCsr);
#endif
    }

    //==========================================================================
    enum { kScratch = 512 };

    AEffect effect;
    double sampleRate;
    float params[kNumParams];
    char programName[kVstMaxProgNameLen + 1];
    hhvs::MusicianAmp amp[kNumChannels];
    bool channel1Stale;
    float scratch[kNumChannels][kScratch];
};

} // namespace

//==============================================================================
VST_EXPORT AEffect* VSTPluginMain (audioMasterCallback host)
{
    if (! host)
        return 0;
    VSMusicianVST* plugin = new VSMusicianVST (host);
    return plugin->getAEffect();
}

#if defined(__APPLE__)
VST_EXPORT AEffect* main_macho (audioMasterCallback host)
{
    return VSTPluginMain (host);
}
#endif
