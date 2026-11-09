// vst2_abi.h
// Minimal declarations of the VST 2.4 binary interface, written for
// interoperability. Only what this plugin uses is declared. This is not the
// Steinberg SDK; the names follow the conventional ones so the code reads
// naturally to anyone who knows the format.
//
// "VST" is a trademark of Steinberg Media Technologies GmbH.

#ifndef TBVS_VST2_ABI_H
#define TBVS_VST2_ABI_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
  #define VST_CALL __cdecl
  #define VST_EXPORT extern "C" __declspec(dllexport)
#else
  #define VST_CALL
  #define VST_EXPORT extern "C" __attribute__((visibility("default")))
#endif

typedef int32_t  VstInt32;
typedef intptr_t VstIntPtr;

struct AEffect;

typedef VstIntPtr (VST_CALL *audioMasterCallback) (AEffect* effect, VstInt32 opcode, VstInt32 index,
                                                   VstIntPtr value, void* ptr, float opt);
typedef VstIntPtr (VST_CALL *AEffectDispatcherProc) (AEffect* effect, VstInt32 opcode, VstInt32 index,
                                                     VstIntPtr value, void* ptr, float opt);
typedef void  (VST_CALL *AEffectProcessProc) (AEffect* effect, float** inputs, float** outputs, VstInt32 sampleFrames);
typedef void  (VST_CALL *AEffectProcessDoubleProc) (AEffect* effect, double** inputs, double** outputs, VstInt32 sampleFrames);
typedef void  (VST_CALL *AEffectSetParameterProc) (AEffect* effect, VstInt32 index, float parameter);
typedef float (VST_CALL *AEffectGetParameterProc) (AEffect* effect, VstInt32 index);

struct AEffect
{
    VstInt32 magic;                                 // kEffectMagic
    AEffectDispatcherProc dispatcher;
    AEffectProcessProc process;                     // deprecated accumulating process
    AEffectSetParameterProc setParameter;
    AEffectGetParameterProc getParameter;
    VstInt32 numPrograms;
    VstInt32 numParams;
    VstInt32 numInputs;
    VstInt32 numOutputs;
    VstInt32 flags;
    VstIntPtr resvd1;
    VstIntPtr resvd2;
    VstInt32 initialDelay;                          // latency in samples
    VstInt32 realQualities;
    VstInt32 offQualities;
    float ioRatio;
    void* object;                                   // plugin instance
    void* user;                                     // free for the host
    VstInt32 uniqueID;
    VstInt32 version;
    AEffectProcessProc processReplacing;
    AEffectProcessDoubleProc processDoubleReplacing;
    char future[56];
};

#define VST_FOURCC(a, b, c, d) ((VstInt32) (((uint32_t) (a) << 24) | ((uint32_t) (b) << 16) | ((uint32_t) (c) << 8) | (uint32_t) (d)))

enum
{
    kEffectMagic = VST_FOURCC ('V', 's', 't', 'P')
};

// Plugin flags
enum
{
    effFlagsHasEditor          = 1 << 0,
    effFlagsCanReplacing       = 1 << 4,
    effFlagsProgramChunks      = 1 << 5,
    effFlagsIsSynth            = 1 << 8,
    effFlagsNoSoundInStop      = 1 << 9,
    effFlagsCanDoubleReplacing = 1 << 12
};

// Host -> plugin opcodes
enum
{
    effOpen = 0,
    effClose = 1,
    effSetProgram = 2,
    effGetProgram = 3,
    effSetProgramName = 4,
    effGetProgramName = 5,
    effGetParamLabel = 6,
    effGetParamDisplay = 7,
    effGetParamName = 8,
    effSetSampleRate = 10,
    effSetBlockSize = 11,
    effMainsChanged = 12,
    effEditGetRect = 13,
    effEditOpen = 14,
    effEditClose = 15,
    effEditIdle = 19,
    effGetChunk = 23,
    effSetChunk = 24,
    effProcessEvents = 25,
    effCanBeAutomated = 26,
    effString2Parameter = 27,
    effGetProgramNameIndexed = 29,
    effGetInputProperties = 33,
    effGetOutputProperties = 34,
    effGetPlugCategory = 35,
    effSetBypass = 44,
    effGetEffectName = 45,
    effGetVendorString = 47,
    effGetProductString = 48,
    effGetVendorVersion = 49,
    effVendorSpecific = 50,
    effCanDo = 51,
    effGetTailSize = 52,
    effGetParameterProperties = 56,
    effGetVstVersion = 58,
    effStartProcess = 71,
    effStopProcess = 72
};

// Plugin -> host opcodes
enum
{
    audioMasterAutomate = 0,
    audioMasterVersion = 1,
    audioMasterCurrentId = 2,
    audioMasterIdle = 3,
    audioMasterIOChanged = 13
};

enum
{
    kPlugCategUnknown = 0,
    kPlugCategEffect = 1
};

enum
{
    kVstMaxProgNameLen   = 24,
    kVstMaxParamStrLen   = 8,
    kVstMaxVendorStrLen  = 64,
    kVstMaxProductStrLen = 64,
    kVstMaxEffectNameLen = 32
};

#endif
