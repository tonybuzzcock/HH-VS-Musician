// vst_host_test.cpp
// A minimal VST 2.4 host that loads the built plugin binary through its entry
// point and checks it behaves: opcodes, parameters, processing, latency,
// sample-rate changes, in-place buffers and shutdown.
//
//   vst_host_test <path to plugin binary>
// On macOS pass the binary inside the bundle:
//   "HH VS Musician (Reverb).vst/Contents/MacOS/HHVSMusicianReverb"

#include "../Source/VST2/vst2_abi.h"
#include "../Source/DSP/MusicianAmp.h"

#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

static int automateCalls = 0;
static VstIntPtr VST_CALL hostCallback (AEffect*, VstInt32 opcode, VstInt32, VstIntPtr, void*, float)
{
    switch (opcode)
    {
        case audioMasterVersion:   return 2400;
        case audioMasterAutomate:  ++automateCalls; return 0;
        default:                   return 0;
    }
}

typedef AEffect* (*EntryProc) (audioMasterCallback);

static VstIntPtr call (AEffect* e, VstInt32 op, VstInt32 idx = 0, VstIntPtr val = 0, void* ptr = 0, float opt = 0.0f)
{
    return e->dispatcher (e, op, idx, val, ptr, opt);
}

static bool finite (const std::vector<float>& x)
{
    for (size_t i = 0; i < x.size(); ++i)
        if (! (x[i] == x[i]) || std::fabs (x[i]) > 100.0f) return false;
    return true;
}

static void runBlocks (AEffect* e, std::vector<float>& l, std::vector<float>& r,
                       std::vector<float>& ol, std::vector<float>& orr, int block)
{
    for (size_t pos = 0; pos < l.size(); pos += block)
    {
        const int n = (int) ((l.size() - pos) < (size_t) block ? l.size() - pos : block);
        float* in[2]  = { &l[pos], &r[pos] };
        float* out[2] = { &ol[pos], &orr[pos] };
        e->processReplacing (e, in, out, n);
    }
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: %s <plugin binary>\n", argv[0]); return 2; }

    std::printf ("Loading %s\n", argv[1]);
    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (! lib) { std::printf ("dlopen failed: %s\n", dlerror()); return 1; }

    EntryProc entry = (EntryProc) dlsym (lib, "VSTPluginMain");
    check (entry != 0, "exports VSTPluginMain");
#if defined(__APPLE__)
    check (dlsym (lib, "main_macho") != 0, "exports main_macho (older Mac hosts)");
#endif
    if (! entry) return 1;

    AEffect* e = entry (hostCallback);
    check (e != 0, "entry point returns an effect");
    if (! e) return 1;

    std::printf ("\nIdentity\n");
    check (e->magic == kEffectMagic, "magic is 'VstP'");
    check (e->numInputs == 2 && e->numOutputs == 2, "stereo in/out");
    check (e->numParams == 18, "18 parameters (14 panel + 4 plugin)");
    check ((e->flags & effFlagsCanReplacing) != 0, "processReplacing supported");
    check ((e->flags & effFlagsIsSynth) == 0, "not flagged as an instrument");
    check (e->initialDelay == hhvs::MusicianAmp::latencySamples(), "reports oversampler latency");
    check (e->uniqueID == VST_FOURCC ('T', 'B', 'v', 's'), "unique ID 'TBvs'");
    char buf[256];
    std::memset (buf, 0, sizeof (buf));
    call (e, effGetEffectName, 0, 0, buf);
    std::printf ("  name: %s, latency %d samples\n", buf, e->initialDelay);
    check (std::strcmp (buf, "HH VS Musician (Reverb)") == 0, "effect name");
    check (call (e, effGetPlugCategory) == kPlugCategEffect, "category: effect");
    check (call (e, effGetVstVersion) == 2400, "VST 2.4");
    check (call (e, effCanDo, 0, 0, (void*) "receiveVstMidiEvent") == -1, "declines MIDI");

    call (e, effOpen);
    call (e, effSetSampleRate, 0, 0, 0, 44100.0f);
    call (e, effSetBlockSize, 0, 512);

    std::printf ("\nParameters\n");
    const char* expectNames[18] = { "2 Volume", "2 Bass", "2 Treble", "2 Reverb",
                                    "1 Gain", "1 Bass", "Middle", "1 Treble", "Presence", "1 Reverb",
                                    "Voice", "ValveSnd", "M Reverb", "M Volume",
                                    "Channel", "Speaker", "Input", "Output" };
    const char* expectDisp[18]  = { "5.0", "0", "0", "Off", "5.0", "0", "0", "0", "0", "On",
                                    "Light", "Off", "3.0", "3.5", "1", "On", "+0.0", "+0.0" };
    for (int i = 0; i < 18; ++i)
    {
        char name[64] = { 0 }, disp[64] = { 0 }, label[64] = { 0 };
        call (e, effGetParamName, i, 0, name);
        call (e, effGetParamDisplay, i, 0, disp);
        call (e, effGetParamLabel, i, 0, label);
        std::printf ("  %-8s = %-6s %-3s (%.2f)\n", name, disp, label, e->getParameter (e, i));
        check (std::strcmp (name, expectNames[i]) == 0 && std::strlen (name) <= 8 && std::strlen (disp) <= 8,
               "name and display string length");
        check (std::strcmp (disp, expectDisp[i]) == 0, "default value as on the panel");
    }
    e->setParameter (e, 0, 0.25f);
    check (std::fabs (e->getParameter (e, 0) - 0.25f) < 1e-6f, "set/get round trip");
    e->setParameter (e, 0, 7.0f);
    check (e->getParameter (e, 0) == 1.0f, "out-of-range values are clamped");
    e->setParameter (e, 0, 0.5f);
    e->setParameter (e, 16, 1.0f);
    std::memset (buf, 0, sizeof (buf));
    call (e, effGetParamDisplay, 16, 0, buf);
    check (std::strcmp (buf, "+12.0") == 0, "Input shows +12.0 dB at max");
    e->setParameter (e, 16, 0.5f);
    e->setParameter (e, 1, 0.0f);
    std::memset (buf, 0, sizeof (buf));
    call (e, effGetParamDisplay, 1, 0, buf);
    check (std::strcmp (buf, "-5.0") == 0, "tone knobs read -5 .. 0 .. +5 like the panel");
    e->setParameter (e, 1, 0.5f);
    e->setParameter (e, 10, 1.0f);
    std::memset (buf, 0, sizeof (buf));
    call (e, effGetParamDisplay, 10, 0, buf);
    check (std::strcmp (buf, "Heavy") == 0, "Voice switch reads Light / Heavy");
    e->setParameter (e, 10, 0.0f);
    e->setParameter (e, 14, 1.0f);
    std::memset (buf, 0, sizeof (buf));
    call (e, effGetParamDisplay, 14, 0, buf);
    check (std::strcmp (buf, "1+2") == 0, "Channel selector reads 1 / 2 / 1+2");
    e->setParameter (e, 14, 0.0f);
    check (call (e, effGetTailSize) > 0, "reports a reverb tail");
    e->setParameter (e, 99, 0.5f);   // must be ignored, not crash
    check (e->getParameter (e, 99) == 0.0f, "invalid parameter index is harmless");

    std::printf ("\nProcessing\n");
    call (e, effMainsChanged, 0, 1);
    const int sr = 44100;
    std::vector<float> l (sr), r (sr), ol (sr), orr (sr);
    for (int i = 0; i < sr; ++i)
        l[i] = r[i] = (float) (0.2 * std::sin (2.0 * 3.14159265358979 * 196.0 * i / sr));

    // Reference: the DSP core directly, same settings as the plugin defaults
    hhvs::MusicianAmp ref;
    ref.setCh2Volume (0.5); ref.setCh2Bass (0.5); ref.setCh2Treble (0.5); ref.setCh2Reverb (false);
    ref.setCh1Gain (0.5); ref.setCh1Bass (0.5); ref.setMiddle (0.5); ref.setCh1Treble (0.5); ref.setPresence (0.5);
    ref.setCh1Reverb (true); ref.setVoiceHeavy (false); ref.setValveSound (false);
    ref.setMasterReverb (0.3); ref.setMasterVolume (0.35);
    ref.setChannel (0); ref.setSpeaker (true); ref.setInputGainDb (0.0); ref.setOutputGainDb (0.0);
    ref.prepare (sr);
    std::vector<float> refOut (sr);
    ref.process (&l[0], &refOut[0], sr);

    runBlocks (e, l, r, ol, orr, 512);
    double maxDiff = 0.0, maxLR = 0.0, pk = 0.0;
    for (int i = 0; i < sr; ++i)
    {
        maxDiff = std::max (maxDiff, (double) std::fabs (ol[i] - refOut[i]));
        maxLR = std::max (maxLR, (double) std::fabs (ol[i] - orr[i]));
        pk = std::max (pk, (double) std::fabs (ol[i]));
    }
    std::printf ("  output peak %.3f, max diff vs DSP core %.2g\n", pk, maxDiff);
    check (pk > 0.02, "produces amp output");
    check (maxDiff < 1e-6, "matches the DSP core sample for sample");
    check (maxLR == 0.0, "mono source gives identical L/R");
    check (finite (ol) && finite (orr), "output finite");

    // True stereo: different signals, switching from mono mid-stream
    for (int i = 0; i < sr; ++i)
        r[i] = (float) (0.2 * std::sin (2.0 * 3.14159265358979 * 247.0 * i / sr));
    runBlocks (e, l, r, ol, orr, 256);
    double lrDiff = 0.0;
    for (int i = sr / 2; i < sr; ++i) lrDiff = std::max (lrDiff, (double) std::fabs (ol[i] - orr[i]));
    check (lrDiff > 0.01 && finite (ol) && finite (orr), "independent stereo channels");

    // In-place processing (output buffers are the input buffers)
    {
        std::vector<float> a (l), b (r);
        for (size_t pos = 0; pos < a.size(); pos += 333)
        {
            const int n = (int) ((a.size() - pos) < 333 ? a.size() - pos : 333);
            float* io[2] = { &a[pos], &b[pos] };
            e->processReplacing (e, io, io, n);
        }
        check (finite (a) && finite (b), "in-place processing, odd block size");
    }

    // Tiny and huge blocks
    {
        std::vector<float> a (l), b (r), oa (sr), ob (sr);
        runBlocks (e, a, b, oa, ob, 1);
        check (finite (oa) && finite (ob), "1-sample blocks");
        runBlocks (e, a, b, oa, ob, 8192);
        check (finite (oa) && finite (ob), "8192-sample blocks");
    }

    // Silence in -> settles to silence out (no DC, no oscillation)
    {
        e->setParameter (e, 9, 0.0f);    // Ch 1 Reverb off so the spring tail is not measured
        std::vector<float> z (sr * 4, 0.0f), oa (sr * 4), ob (sr * 4);
        std::vector<float> z2 (sr * 4, 0.0f);
        runBlocks (e, z, z2, oa, ob, 512);
        double tail = 0.0;
        for (int i = sr * 3; i < sr * 4; ++i) tail = std::max (tail, (double) std::fabs (oa[i]));
        std::printf ("  residual after 3 s of silence: %.2g\n", tail);
        e->setParameter (e, 9, 1.0f);
        check (tail < 1e-3, "silence settles to silence");
    }

    std::printf ("\nSample-rate change to 96 kHz\n");
    call (e, effMainsChanged, 0, 0);
    call (e, effSetSampleRate, 0, 0, 0, 96000.0f);
    call (e, effMainsChanged, 0, 1);
    {
        const int sr2 = 96000;
        std::vector<float> a (sr2), b (sr2), oa (sr2), ob (sr2);
        for (int i = 0; i < sr2; ++i) a[i] = b[i] = (float) (0.2 * std::sin (2.0 * 3.14159265358979 * 196.0 * i / sr2));
        std::clock_t t0 = std::clock();
        runBlocks (e, a, b, oa, ob, 512);
        const double secs = (double) (std::clock() - t0) / CLOCKS_PER_SEC;
        check (finite (oa), "runs at 96 kHz");
        std::printf ("  CPU at 96 kHz (mono source): %.1f%% of one core\n", 100.0 * secs);
    }

    std::printf ("\nAutomation while processing\n");
    {
        std::vector<float> a (sr), b (sr), oa (sr), ob (sr);
        for (int i = 0; i < sr; ++i) a[i] = b[i] = (float) (0.3 * std::sin (2.0 * 3.14159265358979 * 110.0 * i / sr));
        for (size_t pos = 0; pos < a.size(); pos += 64)
        {
            const float t = (float) pos / a.size();
            for (int p = 0; p < 18; ++p) e->setParameter (e, p, 0.5f + 0.5f * std::sin (6.2831853f * (p + 1) * t));
            const int n = (int) ((a.size() - pos) < 64 ? a.size() - pos : 64);
            float* in[2]  = { &a[pos], &b[pos] };
            float* out[2] = { &oa[pos], &ob[pos] };
            e->processReplacing (e, in, out, n);
        }
        double amax = 0.0; int nans = 0;
        for (size_t i = 0; i < oa.size(); ++i) { if (! (oa[i] == oa[i])) ++nans; else amax = std::max (amax, (double) std::fabs (oa[i])); }
        std::printf ("  automation: peak %.2f, NaNs %d\n", amax, nans);
        check (finite (oa), "all parameters automated together");
    }

    call (e, effMainsChanged, 0, 0);
    call (e, effClose);
    check (true, "effClose");
    dlclose (lib);

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL HOST CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
