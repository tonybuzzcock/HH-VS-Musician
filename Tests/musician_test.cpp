// musician_test.cpp
// Offline checks for the HH VS Musician (Reverb) DSP. Builds on Linux or macOS:
//   make -C Tests run      (writes demo WAVs to Tests/renders)
//
// 1. Small-signal behaviour: every tone control moves the right band the right
//    way, Gain/Volume are monotonic, Voice Heavy adds gain
// 2. Power amp: clipping level, current limit, supply sag and recovery
// 3. Stability sweep: control settings x 10 mV-4 V x 41 Hz-15 kHz, plus loud noise
// 4. Features: channel select, reverb switches and tail, Valve Sound, Speaker
// 5. Automation of every control while playing
// 6. CPU per second of audio
// 7. Demo renders (32-bit float WAV)
//
// Channel names here follow the service drawing (Ch 1 = Gain board AS00016,
// Ch 2 = Volume board AS00017); the plugin labels them the other way round.

#include "../Source/DSP/MusicianAmp.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <string>
#include <vector>

using namespace hhvs;

static const double kPiT = 3.14159265358979323846;
static const int kSr = 44100;
static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

static bool allFinite (const std::vector<float>& x, double limit)
{
    for (size_t i = 0; i < x.size(); ++i)
        if (! (x[i] == x[i]) || std::fabs (x[i]) > limit) return false;
    return true;
}

static double rmsFrom (const std::vector<float>& x, size_t from)
{
    double s = 0.0;
    for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (x.size() - from));
}

static double peakFrom (const std::vector<float>& x, size_t from)
{
    double p = 0.0;
    for (size_t i = from; i < x.size(); ++i) p = std::max (p, (double) std::fabs (x[i]));
    return p;
}

static std::vector<float> sine (double f, double amp, double sec)
{
    std::vector<float> v ((size_t) (sec * kSr));
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (amp * std::sin (2.0 * kPiT * f * i / kSr));
    return v;
}

static std::vector<float> run (MusicianAmp& a, const std::vector<float>& in)
{
    std::vector<float> out (in.size());
    const int block = 256;
    for (size_t pos = 0; pos < in.size(); pos += block)
    {
        const int n = (int) ((in.size() - pos) < (size_t) block ? (in.size() - pos) : block);
        a.process (&in[pos], &out[pos], n);
    }
    return out;
}

// Small-signal gain in dB (speaker volts per guitar volt) at f.
static double gainDb (MusicianAmp& a, double f)
{
    const double amp = 1.0e-5;
    a.reset();
    std::vector<float> o = run (a, sine (f, amp, 0.4));
    return 20.0 * std::log10 (peakFrom (o, o.size() / 2) * 40.0 / amp);
}

// Fraction of output energy above f (single-bin-free crude spectral measure via first difference)
static double brightness (const std::vector<float>& x, size_t from)
{
    double d = 0.0, s = 0.0;
    for (size_t i = from + 1; i < x.size(); ++i) { const double e = x[i] - x[i - 1]; d += e * e; s += (double) x[i] * x[i]; }
    return s > 0.0 ? d / s : 0.0;
}

static void writeWav (const std::string& path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::printf ("  could not write %s\n", path.c_str()); return; }
    const unsigned int dataBytes = (unsigned int) (x.size() * 4);
    const unsigned int riff = 36 + dataBytes;
    const unsigned short fmt = 3, ch = 1, bits = 32, align = 4;
    const unsigned int rate = (unsigned int) sr, bps = rate * 4, fmtLen = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f);
    std::fwrite ("WAVEfmt ", 1, 8, f); std::fwrite (&fmtLen, 4, 1, f);
    std::fwrite (&fmt, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f); std::fwrite (&bps, 4, 1, f);
    std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    std::fwrite (&x[0], 4, x.size(), f);
    std::fclose (f);
}

// Karplus-Strong plucked string as a stand-in for a DI guitar track.
static void pluck (std::vector<float>& buf, size_t start, double freq, double amp, double dur)
{
    const int period = (int) (kSr / freq + 0.5);
    std::vector<double> line (period);
    unsigned int seed = (unsigned int) (freq * 1000.0);
    for (int i = 0; i < period; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        line[i] = ((seed >> 8) / 8388608.0 - 1.0);
    }
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 1; i < period; ++i) line[i] = 0.5 * (line[i] + line[i - 1]);
    const size_t n = (size_t) (dur * kSr);
    int idx = 0;
    for (size_t i = 0; i < n && start + i < buf.size(); ++i)
    {
        const int next = (idx + 1) % period;
        const double y = line[idx];
        line[idx] = 0.996 * 0.5 * (line[idx] + line[next]);
        idx = next;
        const double env = (i > n - 2000) ? (double) (n - i) / 2000.0 : 1.0;
        buf[start + i] += (float) (amp * y * env);
    }
}

static std::vector<float> makeRiff()
{
    std::vector<float> g ((size_t) (kSr * 10.0), 0.0f);
    const double E2 = 82.41, G2 = 98.0, A2 = 110.0, B2 = 123.47, E3 = 164.81;
    struct N { double t, f, a, d; };
    const N notes[] = {
        { 0.00, E2, 0.25, 0.45 }, { 0.00, E2 * 1.5, 0.20, 0.45 }, { 0.00, E3, 0.15, 0.45 },
        { 0.50, E2, 0.25, 0.45 }, { 0.50, E2 * 1.5, 0.20, 0.45 }, { 0.50, E3, 0.15, 0.45 },
        { 1.00, G2, 0.25, 0.45 }, { 1.00, G2 * 1.5, 0.20, 0.45 }, { 1.00, G2 * 2.0, 0.15, 0.45 },
        { 1.50, A2, 0.25, 0.90 }, { 1.50, A2 * 1.5, 0.20, 0.90 }, { 1.50, A2 * 2.0, 0.15, 0.90 },
        { 2.75, E3, 0.30, 0.25 }, { 3.00, 196.0, 0.30, 0.25 }, { 3.25, 220.0, 0.30, 0.25 },
        { 3.50, 246.94, 0.30, 0.25 }, { 3.75, 293.66, 0.30, 0.50 }, { 4.25, 246.94, 0.30, 0.25 },
        { 4.50, 220.0, 0.30, 0.75 },
        { 5.50, E2, 0.30, 3.0 }, { 5.50, B2, 0.25, 3.0 }, { 5.50, E3, 0.20, 3.0 },
        { 5.50, 207.65, 0.15, 3.0 }, { 5.50, 246.94, 0.15, 3.0 }, { 5.50, 329.63, 0.12, 3.0 },
    };
    for (size_t i = 0; i < sizeof (notes) / sizeof (notes[0]); ++i)
        pluck (g, (size_t) (notes[i].t * kSr), notes[i].f, notes[i].a, notes[i].d);
    return g;
}

static void cleanSetup (MusicianAmp& a)
{
    a.setCh1Reverb (false); a.setCh2Reverb (false); a.setMasterReverb (0.0);
    a.setSpeaker (false); a.setValveSound (false); a.setVoiceHeavy (false);
    a.setCh1Gain (0.5); a.setCh1Bass (0.5); a.setMiddle (0.5); a.setCh1Treble (0.5); a.setPresence (0.5);
    a.setCh2Volume (0.5); a.setCh2Bass (0.5); a.setCh2Treble (0.5);
    a.setMasterVolume (1.0); a.setChannel (MusicianAmp::kChannel1);
    a.setInputGainDb (0.0); a.setOutputGainDb (0.0);
}

int main (int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : "renders";
    MusicianAmp a;
    a.prepare (kSr);

    //--------------------------------------------------------------------------
    std::printf ("1. Small-signal behaviour (gain in dB, speaker V per guitar V)\n");
    {
        cleanSetup (a);
        struct T { const char* name; void (MusicianAmp::*set) (double); int ch; double f; };
        const T tone[] = {
            { "Ch 1 Bass @ 80 Hz",     &MusicianAmp::setCh1Bass,   0, 80.0 },
            { "Ch 1 Treble @ 4 kHz",   &MusicianAmp::setCh1Treble, 0, 4000.0 },
            { "Middle @ 700 Hz",       &MusicianAmp::setMiddle,    0, 700.0 },
            { "Presence @ 5 kHz",      &MusicianAmp::setPresence,  0, 5000.0 },
            { "Ch 2 Bass @ 80 Hz",     &MusicianAmp::setCh2Bass,   1, 80.0 },
            { "Ch 2 Treble @ 6 kHz",   &MusicianAmp::setCh2Treble, 1, 6000.0 },
        };
        for (size_t i = 0; i < sizeof (tone) / sizeof (tone[0]); ++i)
        {
            a.setChannel (tone[i].ch);
            (a.*tone[i].set) (0.0); const double lo = gainDb (a, tone[i].f);
            (a.*tone[i].set) (0.5); const double mid = gainDb (a, tone[i].f);
            (a.*tone[i].set) (1.0); const double hi = gainDb (a, tone[i].f);
            (a.*tone[i].set) (0.5);
            std::printf ("     %-22s  -5: %5.1f   0: %5.1f   +5: %5.1f\n", tone[i].name, lo, mid, hi);
            char what[96]; std::sprintf (what, "%s cuts and boosts (range > 8 dB)", tone[i].name);
            check (lo < mid && mid < hi && hi - lo > 8.0, what);
        }

        a.setChannel (0);
        double prev = -1.0e9; bool mono = true;
        std::printf ("     Ch 1 Gain @ 330 Hz:");
        for (int k = 1; k <= 10; ++k)
        {
            a.setCh1Gain (k / 10.0); const double g = gainDb (a, 330.0);
            std::printf (" %.0f", g);
            if (g < prev - 0.01) mono = false;
            prev = g;
        }
        std::printf ("\n");
        check (mono, "Ch 1 Gain is monotonic");
        a.setCh1Gain (1.0);
        const double light = gainDb (a, 330.0);
        a.setCh1Gain (0.0);
        const double g0 = gainDb (a, 330.0);
        std::printf ("     Gain 0: %.1f dB\n", g0);
        check (g0 < light - 35.0, "Ch 1 Gain at 0 is nearly silent (dual-action pot shunts the output)");
        a.setCh1Gain (1.0);
        a.setVoiceHeavy (true); const double hvy = gainDb (a, 330.0); a.setVoiceHeavy (false);
        std::printf ("     Gain 10, Light %.1f dB, Heavy %.1f dB\n", light, hvy);
        check (hvy > light + 5.0, "Voice Heavy adds preamp gain (S1B switches R22 1K)");
        a.setCh1Gain (0.5);

        a.setChannel (1);
        prev = -1.0e9; mono = true;
        for (int k = 1; k <= 10; ++k)
        {
            a.setCh2Volume (k / 10.0); const double g = gainDb (a, 1000.0);
            if (g < prev - 0.01) mono = false;
            prev = g;
        }
        check (mono, "Ch 2 Volume is monotonic");
        a.setCh2Volume (0.5);
        a.setChannel (0);
    }

    //--------------------------------------------------------------------------
    std::printf ("\n2. Power amp into the 8 ohm 2x12\n");
    {
        cleanSetup (a);
        a.setCh1Gain (0.3);
        a.setMasterVolume (0.45);
        a.reset(); a.clearPeakAmps();
        std::vector<float> o = run (a, sine (1000.0, 0.05, 0.5));
        const double vClean = peakFrom (o, o.size() / 2) * 40.0;
        std::printf ("     moderate level: %.1f V peak at the speaker\n", vClean);

        a.setCh1Gain (1.0); a.setMasterVolume (1.0); a.setVoiceHeavy (true);
        a.reset(); a.clearPeakAmps();
        o = run (a, sine (1000.0, 0.5, 1.5));
        const double vMax = peakFrom (o, o.size() / 2) * 40.0;
        const double sag = a.railPositive();
        std::printf ("     flat out: %.1f V peak (%.0f W into 8 ohms), peak current %.2f A, rail +%.1f V\n",
                     vMax, vMax * vMax / 16.0, a.peakSpeakerAmps(), sag);
        check (vMax > 38.0 && vMax < 48.0, "clips at roughly 100 W into 8 ohms");
        check (a.peakSpeakerAmps() <= 7.5 + 1e-6, "output current never exceeds the V-I limit");
        check (sag < 48.0, "supply rails sag under sustained full power");
        std::vector<float> z ((size_t) (kSr * 1.5), 0.0f);
        run (a, z);
        std::printf ("     after 1.5 s of silence: rail +%.1f V\n", a.railPositive());
        check (a.railPositive() > 49.0, "rails recover");
        a.setVoiceHeavy (false);
    }

    //--------------------------------------------------------------------------
    std::printf ("\n3. Stability sweep\n");
    {
        const double levels[] = { 0.01, 0.1, 0.5, 1.0, 2.0, 4.0 };
        const double freqs[] = { 41.0, 110.0, 440.0, 2000.0, 7000.0, 15000.0 };
        unsigned int seed = 12345u;
        int runs = 0; bool ok = true;
        for (int combo = 0; combo < 24 && ok; ++combo)
        {
            // Random panel settings, with corners forced on some passes
            double knob[14];
            for (int k = 0; k < 14; ++k)
            {
                seed = seed * 1664525u + 1013904223u;
                const double r = (seed >> 8) / 16777216.0;
                knob[k] = (combo % 3 == 0) ? (r < 0.5 ? 0.0 : 1.0) : r;
            }
            a.setCh2Volume (knob[0]); a.setCh2Bass (knob[1]); a.setCh2Treble (knob[2]); a.setCh2Reverb (knob[3] > 0.5);
            a.setCh1Gain (knob[4]); a.setCh1Bass (knob[5]); a.setMiddle (knob[6]); a.setCh1Treble (knob[7]);
            a.setPresence (knob[8]); a.setCh1Reverb (knob[9] > 0.5); a.setVoiceHeavy (knob[10] > 0.5);
            a.setValveSound (knob[11] > 0.5); a.setMasterReverb (knob[12]); a.setMasterVolume (knob[13]);
            a.setChannel (combo % 3); a.setSpeaker ((combo & 1) != 0);
            a.reset();
            for (size_t li = 0; li < sizeof (levels) / sizeof (levels[0]); ++li)
                for (size_t fi = 0; fi < sizeof (freqs) / sizeof (freqs[0]); ++fi)
                {
                    std::vector<float> o = run (a, sine (freqs[fi], levels[li], 0.15));
                    ++runs;
                    if (! allFinite (o, 8.0)) { ok = false; std::printf ("     unstable: combo %d, %.2f V, %.0f Hz\n", combo, levels[li], freqs[fi]); }
                }
            std::vector<float> noise ((size_t) (kSr * 0.3));
            for (size_t i = 0; i < noise.size(); ++i)
            {
                seed = seed * 1664525u + 1013904223u;
                noise[i] = (float) (4.0 * ((seed >> 8) / 8388608.0 - 1.0));
            }
            if (! allFinite (run (a, noise), 8.0)) { ok = false; std::printf ("     noise burst unstable, combo %d\n", combo); }
        }
        std::printf ("     %d tone runs plus noise bursts\n", runs);
        check (ok, "finite and bounded at every setting, level and frequency");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n4. Features\n");
    {
        cleanSetup (a);
        a.setMasterVolume (0.5);
        std::vector<float> in = sine (330.0, 0.2, 0.5);

        a.setChannel (0); a.reset();
        const double c1 = rmsFrom (run (a, in), kSr / 4);
        a.setChannel (1); a.reset();
        const double c2 = rmsFrom (run (a, in), kSr / 4);
        a.setChannel (2); a.reset();
        const double cb = rmsFrom (run (a, in), kSr / 4);
        std::printf ("     rms out: Ch 1 %.3f, Ch 2 %.3f, both %.3f\n", c1, c2, cb);
        check (c1 > 0.01 && c2 > 0.001 && cb > std::max (c1, c2) * 0.9, "channel select routes each channel");

        // Reverb tail: play a burst, then silence
        std::vector<float> burst = sine (440.0, 0.2, 0.3);
        burst.resize ((size_t) (kSr * 1.5), 0.0f);
        a.setChannel (0); a.setCh1Reverb (true); a.setMasterReverb (0.7); a.reset();
        std::vector<float> o = run (a, burst);
        const double tailOn = rmsFrom (o, (size_t) (kSr * 0.6));
        a.setCh1Reverb (false); a.reset();
        o = run (a, burst);
        const double tailOff = rmsFrom (o, (size_t) (kSr * 0.6));
        std::printf ("     tail after the note: reverb on %.2g, off %.2g\n", tailOn, tailOff);
        check (tailOn > 20.0 * tailOff && tailOn > 1e-4, "Ch 1 Reverb switch sends to the spring tank");
        a.setChannel (1); a.setCh2Reverb (true); a.reset();
        o = run (a, burst);
        check (rmsFrom (o, (size_t) (kSr * 0.6)) > 1e-5, "Ch 2 Reverb switch sends to the spring tank");
        a.setCh2Reverb (false); a.setMasterReverb (0.0); a.setChannel (0);

        // Valve Sound smooths the clipped edge
        a.setCh1Gain (1.0); a.setVoiceHeavy (true); a.setMasterVolume (0.3);
        std::vector<float> hard = sine (330.0, 0.5, 0.5);
        a.setValveSound (false); a.reset();
        const double bOff = brightness (run (a, hard), kSr / 4);
        a.setValveSound (true); a.reset();
        const double bOn = brightness (run (a, hard), kSr / 4);
        a.setVoiceHeavy (false);
        std::printf ("     brightness index: Valve Sound off %.4f, on %.4f\n", bOff, bOn);
        check (bOn < bOff * 0.8, "Valve Sound softens the high harmonics");
        a.setValveSound (false);

        // Speaker rolls off the top
        a.setCh1Gain (0.5); a.setMasterVolume (1.0);
        a.setSpeaker (false); const double t8 = gainDb (a, 8000.0) - gainDb (a, 1000.0);
        a.setSpeaker (true);  const double s8 = gainDb (a, 8000.0) - gainDb (a, 1000.0);
        const double s80 = gainDb (a, 80.0) - gainDb (a, 1000.0);
        std::printf ("     8 kHz re 1 kHz: terminals %.1f dB, miked speaker %.1f dB; 80 Hz miked %.1f dB\n", t8, s8, s80);
        check (s8 < t8 - 15.0, "12\" speakers roll off above 5 kHz");
        a.setSpeaker (false);

        // Silence settles to silence (reverb off)
        cleanSetup (a); a.setMasterVolume (0.7); a.reset();
        run (a, sine (220.0, 0.5, 0.5));
        std::vector<float> z ((size_t) kSr, 0.0f);
        o = run (a, z);
        std::printf ("     residual after 0.5 s of silence: %.2g\n", peakFrom (o, kSr / 2));
        check (peakFrom (o, kSr / 2) < 1e-4, "silence settles to silence");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n5. Automation while playing\n");
    {
        cleanSetup (a); a.setChannel (2); a.setSpeaker (true); a.reset();
        std::vector<float> in = makeRiff();
        in.resize ((size_t) (kSr * 4));
        std::vector<float> out (in.size());
        bool ok = true; double maxStep = 0.0; float prev = 0.0f;
        for (size_t pos = 0; pos < in.size(); pos += 64)
        {
            const double t = (double) pos / in.size();
            const double s[8] = { std::sin (6.28 * t), std::sin (6.28 * 2 * t), std::sin (6.28 * 3 * t), std::sin (6.28 * 5 * t),
                                  std::sin (6.28 * 7 * t), std::sin (6.28 * 4 * t), std::sin (6.28 * 6 * t), std::sin (6.28 * 8 * t) };
            a.setCh2Volume (0.5 + 0.5 * s[0]); a.setCh2Bass (0.5 + 0.5 * s[1]); a.setCh2Treble (0.5 + 0.5 * s[2]);
            a.setCh1Gain (0.5 + 0.5 * s[3]); a.setCh1Bass (0.5 + 0.5 * s[4]); a.setMiddle (0.5 + 0.5 * s[5]);
            a.setCh1Treble (0.5 + 0.5 * s[6]); a.setPresence (0.5 + 0.5 * s[7]);
            a.setMasterReverb (0.5 + 0.5 * s[1]); a.setMasterVolume (0.4 + 0.3 * s[0]);
            a.setCh1Reverb (s[2] > 0); a.setCh2Reverb (s[3] > 0); a.setVoiceHeavy (s[4] > 0); a.setValveSound (s[5] > 0);
            a.setSpeaker (s[6] > 0); a.setChannel (s[7] > 0.5 ? 1 : (s[7] < -0.5 ? 0 : 2));
            const int n = (int) std::min ((size_t) 64, in.size() - pos);
            a.process (&in[pos], &out[pos], n);
            for (int i = 0; i < n; ++i) { maxStep = std::max (maxStep, (double) std::fabs (out[pos + i] - prev)); prev = out[pos + i]; }
        }
        ok = allFinite (out, 3.0);
        std::printf ("     peak %.3f, largest sample-to-sample step %.3f\n", peakFrom (out, 0), maxStep);
        check (ok, "every control and switch automated together stays finite and bounded");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n6. CPU\n");
    {
        cleanSetup (a); a.setChannel (2); a.setCh1Reverb (true); a.setCh2Reverb (true);
        a.setMasterReverb (0.4); a.setSpeaker (true); a.reset();
        std::vector<float> in = makeRiff();
        const std::clock_t t0 = std::clock();
        run (a, in);
        const double secs = (double) (std::clock() - t0) / CLOCKS_PER_SEC;
        std::printf ("     both channels + reverb + speaker: %.1f%% of one core (mono)\n", 100.0 * secs / (in.size() / (double) kSr));
        a.setChannel (0); a.setCh2Reverb (false); a.reset();
        const std::clock_t t1 = std::clock();
        run (a, in);
        const double secs1 = (double) (std::clock() - t1) / CLOCKS_PER_SEC;
        std::printf ("     Ch 1 only: %.1f%% of one core (mono)\n", 100.0 * secs1 / (in.size() / (double) kSr));
    }

    //--------------------------------------------------------------------------
    std::printf ("\n7. Demo renders in %s/\n", outDir.c_str());
    {
        const std::vector<float> riff = makeRiff();
        struct Demo { const char* file; int ch; double gain, vol, mvol, mrev; bool heavy, valve, rev1, rev2, spk; double out; };
        const Demo demos[] = {
            { "ch2_gain_clean_reverb.wav",    0, 0.25, 0.5, 0.45, 0.40, false, false, true,  false, true,  0.0 },
            { "ch2_gain_vs_sustain.wav",      0, 0.85, 0.5, 0.30, 0.25, false, false, true,  false, true,  0.0 },
            { "ch2_gain_heavy_valve.wav",     0, 1.00, 0.5, 0.30, 0.25, true,  true,  true,  false, true,  0.0 },
            { "ch1_volume_channel.wav",          1, 0.50, 0.6, 0.50, 0.30, false, false, false, true,  true,  0.0 },
            { "power_amp_cranked.wav",   0, 0.50, 0.5, 1.00, 0.20, false, false, true,  false, true, -10.0 },
            { "ch2_gain_no_speaker.wav",   0, 0.85, 0.5, 0.30, 0.25, false, false, true,  false, false, -6.0 },
        };
        for (size_t i = 0; i < sizeof (demos) / sizeof (demos[0]); ++i)
        {
            const Demo& d = demos[i];
            cleanSetup (a);
            a.setChannel (d.ch); a.setCh1Gain (d.gain); a.setCh2Volume (d.vol); a.setMasterVolume (d.mvol);
            a.setMasterReverb (d.mrev); a.setVoiceHeavy (d.heavy); a.setValveSound (d.valve);
            a.setCh1Reverb (d.rev1); a.setCh2Reverb (d.rev2); a.setSpeaker (d.spk); a.setOutputGainDb (d.out);
            a.reset();
            std::vector<float> o = run (a, riff);
            const double pk = peakFrom (o, 0);
            std::printf ("     %-26s peak %.2f\n", d.file, pk);
            // Normalise renders to -1 dBFS so they are easy to compare by ear
            const double g = pk > 0.0 ? 0.89 / pk : 1.0;
            for (size_t k = 0; k < o.size(); ++k) o[k] = (float) (o[k] * g);
            writeWav (outDir + "/" + d.file, o, kSr);
        }
        std::vector<float> dry (riff);
        for (size_t k = 0; k < dry.size(); ++k) dry[k] *= 1.5f;
        writeWav (outDir + "/dry_input.wav", dry, kSr);
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL DSP CHECKS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
