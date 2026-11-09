// Biquad.h
// RBJ-cookbook biquads (transposed direct form II) plus first-order sections.
// Used only where the schematic does not give components: the inside of the
// V-S tone correction module, the spring tank, and the loudspeaker cone.

#ifndef TBVS_BIQUAD_H
#define TBVS_BIQUAD_H

#include <cmath>

namespace hhvs
{

const double kPi = 3.14159265358979323846;

class Biquad
{
public:
    Biquad() : b0 (1.0), b1 (0.0), b2 (0.0), a1 (0.0), a2 (0.0), z1 (0.0), z2 (0.0) {}

    void reset() { z1 = z2 = 0.0; }

    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setIdentity() { b0 = 1.0; b1 = b2 = a1 = a2 = 0.0; }

    void setPeak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
    }

    void setLowShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }

    void setHighShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }

    void setLowPass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void setHighPass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    // First-order sections via the bilinear transform (b2 = a2 = 0).
    void setLowPass1 (double fs, double f)
    {
        const double k = std::tan (kPi * f / fs);
        set (k, k, 0.0, k + 1.0, k - 1.0, 0.0);
    }

    void setHighPass1 (double fs, double f)
    {
        const double k = std::tan (kPi * f / fs);
        set (1.0, -1.0, 0.0, k + 1.0, k - 1.0, 0.0);
    }

private:
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    double b0, b1, b2, a1, a2, z1, z2;
};

} // namespace hhvs

#endif
