// Oversampler4x.h
// 4x polyphase FIR up/down sampler (linear phase, Kaiser-windowed sinc).
// Fixed latency, reported by latencySamples(). Plain C++03, no allocation.

#ifndef TBVS_OVERSAMPLER4X_H
#define TBVS_OVERSAMPLER4X_H

#include <cmath>
#include <cstring>

namespace hhvs
{

class Oversampler4x
{
public:
    enum { kFactor = 4, kTaps = 161, kPhaseLen = (kTaps + kFactor - 1) / kFactor };

    Oversampler4x()
    {
        design();
        reset();
    }

    void reset()
    {
        std::memset (upHist, 0, sizeof (upHist));
        std::memset (downHist, 0, sizeof (downHist));
        upPos = 0;
        downPos = 0;
    }

    // Base-rate latency: each filter delays (kTaps - 1) / 2 oversampled samples.
    static int latencySamples() { return (kTaps - 1) / kFactor; }

    // One base-rate sample in, kFactor oversampled samples out.
    void upsample (double x, double out[kFactor])
    {
        upPos = (upPos == 0) ? kPhaseLen - 1 : upPos - 1;
        upHist[upPos] = x;
        upHist[upPos + kPhaseLen] = x;
        const double* h = &upHist[upPos];

        for (int ph = 0; ph < kFactor; ++ph)
        {
            double acc = 0.0;
            const double* c = poly[ph];
            for (int k = 0; k < kPhaseLen; ++k)
                acc += c[k] * h[k];
            out[ph] = acc;
        }
    }

    // kFactor oversampled samples in, one base-rate sample out.
    double downsample (const double in[kFactor])
    {
        for (int k = 0; k < kFactor; ++k)
        {
            downPos = (downPos == 0) ? kTaps - 1 : downPos - 1;
            downHist[downPos] = in[k];
            downHist[downPos + kTaps] = in[k];
        }
        const double* h = &downHist[downPos];
        double acc = 0.0;
        for (int k = 0; k < kTaps; ++k)
            acc += coeffs[k] * h[k];
        return acc;
    }

private:
    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        const double q = x * x * 0.25;
        for (int k = 1; k < 60; ++k)
        {
            term *= q / ((double) k * (double) k);
            sum += term;
            if (term < sum * 1.0e-17) break;
        }
        return sum;
    }

    void design()
    {
        const double pi = 3.14159265358979323846;
        const double fc = 0.5 / kFactor;   // cutoff at the base-rate Nyquist, cycles per sample
        const double beta = 7.5;           // ~75 dB stopband
        const double mid = (kTaps - 1) * 0.5;
        const double i0b = besselI0 (beta);

        double sum = 0.0;
        for (int n = 0; n < kTaps; ++n)
        {
            const double t = n - mid;
            const double sinc = (t == 0.0) ? 2.0 * fc : std::sin (2.0 * pi * fc * t) / (pi * t);
            const double r = t / mid;
            const double w = besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
            coeffs[n] = sinc * w;
            sum += coeffs[n];
        }
        for (int n = 0; n < kTaps; ++n)
            coeffs[n] /= sum;  // unity DC gain for the decimator

        // Interpolator phases: zero-stuffing loses a factor of kFactor, so put it back.
        for (int ph = 0; ph < kFactor; ++ph)
            for (int k = 0; k < kPhaseLen; ++k)
            {
                const int n = ph + k * kFactor;
                poly[ph][k] = (n < kTaps) ? coeffs[n] * kFactor : 0.0;
            }
    }

    double coeffs[kTaps];
    double poly[kFactor][kPhaseLen];
    double upHist[2 * kPhaseLen];
    double downHist[2 * kTaps];
    int upPos, downPos;
};

} // namespace hhvs

#endif
