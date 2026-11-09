// SpringTank.cpp

#include "SpringTank.h"

#include <cmath>
#include <cstring>

namespace hhvs
{

const double SpringTank::kTransferGain = 0.004;

SpringTank::SpringTank() : lpCoeff (0.5), fs (44100.0)
{
    std::memset (springs, 0, sizeof (springs));
}

void SpringTank::prepare (double sampleRate)
{
    fs = sampleRate;

    // Two springs of slightly different length; medium decay (~2.5 s).
    const double delaySec[kNumSprings] = { 0.0331, 0.0413 };
    const double t60 = 2.5;
    for (int i = 0; i < kNumSprings; ++i)
    {
        Spring& s = springs[i];
        int len = (int) (delaySec[i] * fs + 0.5);
        if (len >= kMaxDelay) len = kMaxDelay - 1;
        if (len < 8) len = 8;
        s.len = len;
        s.feedback = -std::pow (10.0, -3.0 * delaySec[i] / t60);
        // Dispersion: the all-pass coefficient sets how strongly highs lag lows.
        s.apCoeff = (i == 0) ? 0.62 : 0.58;
    }

    // Transducer bandwidth
    inHP.setHighPass (fs, 110.0, 0.707);
    inLP.setLowPass (fs, 4500.0, 0.707);
    outLP.setLowPass (fs, 4200.0, 0.6);
    lpCoeff = 1.0 - std::exp (-2.0 * kPi * 3500.0 / fs);   // per-trip high-frequency loss

    reset();
}

void SpringTank::reset()
{
    for (int i = 0; i < kNumSprings; ++i)
    {
        Spring& s = springs[i];
        std::memset (s.buf, 0, sizeof (s.buf));
        std::memset (s.apState, 0, sizeof (s.apState));
        s.pos = 0;
        s.lpState = 0.0;
    }
    inHP.reset(); inLP.reset(); outLP.reset();
}

double SpringTank::processSpring (Spring& s, double in)
{
    int rp = s.pos - s.len;
    if (rp < 0) rp += kMaxDelay;
    double y = s.buf[rp];

    // All-pass chain: first-order sections, y = -a x + xprev + a yprev (lattice form)
    const double a = s.apCoeff;
    double v = y;
    for (int k = 0; k < kNumAP; ++k)
    {
        const double w = v - a * s.apState[k];
        const double o = a * w + s.apState[k];
        s.apState[k] = w;
        v = o;
    }

    s.lpState += (v - s.lpState) * lpCoeff;
    const double fb = s.lpState * s.feedback;

    s.buf[s.pos] = in + fb;
    if (++s.pos >= kMaxDelay) s.pos = 0;
    return v;
}

double SpringTank::process (double vDrive)
{
    double x = inLP.process (inHP.process (vDrive));
    double out = 0.0;
    for (int i = 0; i < kNumSprings; ++i)
        out += processSpring (springs[i], x);
    out = outLP.process (out) * (kTransferGain * 0.5);
    if (! (out == out)) { reset(); return 0.0; }
    return out;
}

} // namespace hhvs
