// MusicianAmp.h
// TB VS Musician: model of the H/H VS Musician (Reverb), the whole amplifier, one mono signal path.
//
//   guitar -> [Ch 1: 2N5089/741 gain stage -> Baxandall -> 741 (1M5)] -> V-S module --+
//          -> [Ch 2: 741 gain stage -> tone network -> 741 (1M5)] ---------------------+-> mix
//   reverb sends -> 741 tank driver -> 4C spring tank -> 741 recovery -> Master Reverb -+
//   mix -> Master Volume -> AS00014/4 100 W power amp -> 8 ohm 2x12 (two 1977 H/H 12")
//
// Everything from the schematic is modelled from its component values (the
// Netlist class, 4x oversampled). Three parts are not circuits on the sheet and
// are modelled from their behaviour instead: the inside of the AS00015/A
// tone correction module (Middle, Presence, Voice and Valve Sound), the spring
// tank, and the loudspeaker cones.
//
// Calibration: 1.0 at the input is 1 V peak from the guitar. Output 1.0 is
// 40 V peak at the speaker terminals (about 100 W into 8 ohms); with Speaker on,
// the cab-miked signal is scaled to match at 1 kHz.
// Plain C++98, no allocation or locks in process(). One instance per channel.

#ifndef TBVS_MUSICIANAMP_H
#define TBVS_MUSICIANAMP_H

#include "Netlist.h"
#include "Oversampler4x.h"
#include "SpringTank.h"
#include "Biquad.h"

namespace hhvs
{

class MusicianAmp
{
public:
    enum Channel { kChannel1 = 0, kChannel2, kChannelBoth };

    MusicianAmp();

    void prepare (double sampleRate);
    void reset();

    // Front panel, left to right. Knobs are 0..1 (0 to 10 on the dial; tone
    // knobs 0.5 = the centre "0" mark). Changes are smoothed.
    void setCh2Volume (double v);
    void setCh2Bass (double v);
    void setCh2Treble (double v);
    void setCh2Reverb (bool on);
    void setCh1Gain (double v);
    void setCh1Bass (double v);
    void setMiddle (double v);
    void setCh1Treble (double v);
    void setPresence (double v);
    void setCh1Reverb (bool on);
    void setVoiceHeavy (bool heavy);
    void setValveSound (bool on);
    void setMasterReverb (double v);
    void setMasterVolume (double v);

    // Plugin-only controls
    void setChannel (int ch);
    void setSpeaker (bool on);
    void setInputGainDb (double dB);
    void setOutputGainDb (double dB);

    void process (const float* in, float* out, int numSamples);

    static int latencySamples() { return Oversampler4x::latencySamples(); }

    // For tests: the last power-amp state
    double railPositive() const { return railPos; }
    double railNegative() const { return railNeg; }
    double lastSpeakerVolts() const { return lastVspk; }
    double peakSpeakerAmps() const { return peakAmps; }
    void   clearPeakAmps() { peakAmps = 0.0; }
    bool   currentLimitHit() const { return climHit; }

    static double audioTaper (double x);

private:
    // Knob indices for smoothing
    enum Knob { kCh2Vol, kCh2Bass, kCh2Treb, kCh1Gain, kCh1Bass, kMid, kCh1Treb, kPres,
                kMRev, kMVol, kNumKnobs };

    void buildCircuits();
    void applyPots (bool force);
    void applyModuleEq();
    double processCh1 (double g);
    double processCh2 (double g);
    double powerAmp (double vIn);

    double fsBase, fsOs;

    // Circuits
    Netlist ch1a, ch1b, ch2a, ch2b, revDrv, revRec, speaker;
    int ch1aOut, ch1bOut, ch2aOut, ch2bOut, revDrvOut, revRecOut;
    int eGainFb, eGainOut, eHeavy;                    // Ch 1 gain pot halves, Voice switch R22
    int eB1Top, eB1Bot, eT1Top, eT1Bot;               // Ch 1 Bass / Treble halves
    int eVolFb, eVolOut;                              // Ch 2 Volume pot halves
    int eB2Top, eB2Bot, eT2Top, eT2Bot;               // Ch 2 Bass / Treble halves
    int spkOut, spkCone;

    // V-S tone correction module
    Biquad voiceLow, voiceMid, voiceHigh, midEq, presEq, valveDc, valvePre, valvePost;

    // Power amp
    Biquad paInHP, paFbHP, paGbwLP;
    double railPos, railNeg, vOutPrev, lastVspk, peakAmps, slewStep;
    bool climHit;

    // Spring reverb
    SpringTank tank;
    Oversampler4x osMain, osSpkDown, osDrvDown, osRevUp;
    double revUp[Oversampler4x::kFactor];

    // Speaker acoustics
    double coneVPrev;
    Biquad cab[6];
    double cabNorm;
    double spkMix;       // 0 = speaker terminals, 1 = miked speaker (crossfaded)

    // Controls
    double knobTarget[kNumKnobs], knobCur[kNumKnobs];
    bool ch2Rev, ch1Rev, heavy, valve, speakerOn;
    int channel;
    double inGainT, inGainC, outGainT, outGainC;
    double smoothCoeff;
    int potCountdown;
    bool heavyApplied;
    bool ch1Active, ch2Active;
};

} // namespace hhvs

#endif
