// SpringTank.h
// The "4C" spring reverb unit. A tank is a mechanical part, not a circuit, so
// this is an algorithmic model of a two-spring medium-decay tank: each spring is
// a delay loop whose round trip passes through a chain of all-pass filters,
// which gives the dispersive "boing"/chirp of a real spring, band-limited the
// way the transducers are (roughly 100 Hz to 4.5 kHz).
//
// Input is the driver voltage across the input transducer, output is the
// pickup transducer voltage (a few millivolts), which the recovery stage then
// amplifies. Runs at the base sample rate. Fixed storage, no allocation.

#ifndef TBVS_SPRINGTANK_H
#define TBVS_SPRINGTANK_H

#include "Biquad.h"

namespace hhvs
{

class SpringTank
{
public:
    enum { kMaxDelay = 16384, kNumAP = 12, kNumSprings = 2 };

    SpringTank();
    void prepare (double sampleRate);
    void reset();
    double process (double vDrive);

    static const double kTransferGain;   // pickup volts per drive volt, midband

private:
    struct Spring
    {
        double buf[kMaxDelay];
        int len, pos;
        double feedback;
        double apState[kNumAP];
        double apCoeff;
        double lpState;
    };

    double processSpring (Spring& s, double in);

    Spring springs[kNumSprings];
    Biquad inHP, inLP, outLP;
    double lpCoeff;
    double fs;
};

} // namespace hhvs

#endif
