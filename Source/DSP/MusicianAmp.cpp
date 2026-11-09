// MusicianAmp.cpp
// Component values are from the H/H drawing C/AC00014 (VS Musician (Reverb))
// and the AS00014/4 output driver sheet. Reference designators in comments.

#include "MusicianAmp.h"

#include <cmath>

namespace hhvs
{

namespace
{
    const int kPotUpdateInterval = 16;

    // 741 op-amp
    const double kGbw741   = 1.0e6;    // Hz
    const double kSlew741  = 0.5;      // V/us
    const double kSwing741 = 14.0;     // V on +/-16 V rails (PL16Z zeners)
    const double kKnee741  = 0.8;      // V
    const double kRout741  = 75.0;     // ohms, open-loop output resistance

    const double kPickupR  = 6000.0;   // guitar pickup DC resistance
    const double kPotMin   = 10.0;     // wiper contact / end resistance

    // Mixer: Ch 1 and Ch 2 via 10K, reverb via 8K2, into the 25K Master Volume
    const double kMixG     = 1.0 / 10000.0 + 1.0 / 10000.0 + 1.0 / 8200.0 + 1.0 / 25000.0;
    const double kMixCh    = (1.0 / 10000.0) / kMixG;
    const double kMixRev   = (1.0 / 8200.0) / kMixG * (1000.0 / 1100.0);   // R9 100R into 1K pot

    // Power amp (AS00014/4)
    const double kPaGain   = 1.0 + 14700.0 / 390.0;   // R5 10K + R33 4K7 over R3 390R
    const double kRailNoLoad = 50.0;   // V, +/-48 V nominal, a little higher at idle
    const double kRailC    = 4700.0e-6;// C1, C2 reservoirs
    const double kRailRs   = 3.0;      // transformer + bridge source resistance (80 VA)
    const double kRailDrop = 2.5;      // V lost in drivers, outputs and 0R33 emitter resistors
    const double kRailKnee = 1.5;      // V
    const double kPaSlew   = 2.0;      // V/us at the output
    const double kIMax     = 7.5;      // A, Q4/Q5 V-I limiter, roughly
    const double kIQuiet   = 0.06;     // A per rail at idle

    const double kVFullScale = 40.0;   // V peak at the speaker = 1.0 out

    inline double clamp01 (double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
    inline double dbToGain (double dB) { return std::pow (10.0, dB / 20.0); }
    inline double potR (double r) { return r < kPotMin ? kPotMin : r; }
}

double MusicianAmp::audioTaper (double x)
{
    const double k = 4.394449154672439;   // 10% of the track at mid-rotation
    return (std::exp (k * x) - 1.0) / (std::exp (k) - 1.0);
}

MusicianAmp::MusicianAmp()
    : fsBase (44100.0), fsOs (176400.0),
      railPos (kRailNoLoad), railNeg (kRailNoLoad), vOutPrev (0.0), lastVspk (0.0),
      peakAmps (0.0), slewStep (1.0), rsCharge (0.0), climHit (false),
      coneVPrev (0.0), cabNorm (1.0), spkMix (1.0),
      ch2Rev (false), ch1Rev (true), heavy (false), valve (false), speakerOn (true),
      channel (kChannel1),
      inGainT (1.0), inGainC (1.0), outGainT (1.0), outGainC (1.0),
      smoothCoeff (0.001), potCountdown (0), heavyApplied (false),
      ch1Active (true), ch2Active (false)
{
    const double defaults[kNumKnobs] = { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.3, 0.35 };
    for (int i = 0; i < kNumKnobs; ++i)
        knobTarget[i] = knobCur[i] = defaults[i];
    for (int k = 0; k < Oversampler4x::kFactor; ++k)
        revUp[k] = 0.0;
    buildCircuits();
}

//==============================================================================
void MusicianAmp::buildCircuits()
{
    // ---- Ch 1 gain stage (AS00016: Q1/Q2 2N5089 long-tail pair + IC1 741) ----
    // Treated as one op-amp: the pair is IC1's input stage. Q1 base is +, Q2 base is -.
    {
        Netlist& n = ch1a; n.clear();
        const int n1 = n.addNode(), n2 = n.addNode(), n3 = n.addNode(), n4 = n.addNode(),
                  n5 = n.addNode(), o = n.addNode();
        n.addInput (n1, kPickupR + 10000.0);   // pickup + R1 10K
        n.addC (n1, n2, 0.1e-6);               // C1
        n.addR (n2, 0, 100000.0);              // R3
        n.addR (o, n3, 22000.0);               // R9
        n.addC (o, n3, 68.0e-12);              // C4
        n.addR (n3, n4, 2700.0);               // R8
        n.addC (n4, n5, 6.8e-6);               // C3
        eGainFb = n.addR (n5, 0, 12500.0);     // VR2 Gain, CW end (pin 25) to wiper (24, ground)
        eHeavy  = n.addR (n3, n4, 1.0e9);      // R22 1K, switched in by S1B on Heavy
        n.setOpAmp (n2, n3, o, kGbw741, kSlew741, kSwing741, kKnee741);
        ch1aOut = o;
    }

    // ---- Ch 1 tone stack and IC2 (1M5 feedback) ----
    {
        Netlist& n = ch1b; n.clear();
        const int s = n.addNode(), a = n.addNode(), t = n.addNode(), b18 = n.addNode(),
                  b17 = n.addNode(), b16 = n.addNode(), t21 = n.addNode(), t20 = n.addNode(),
                  t19 = n.addNode(), N = n.addNode(), m = n.addNode(), o = n.addNode();
        n.addInput (s, kRout741);
        n.addC (s, a, 6.8e-6);                 // C5
        n.addR (a, t, 470.0);                  // R10
        eGainOut = n.addR (t, 0, 12500.0);     // VR2 Gain, pin 23 to wiper
        n.addR (t, b18, 10000.0);              // R11
        n.addR (t, b18, 1500.0);               // R21 via LINK 22-23
        eB1Top = n.addR (b18, b17, 50000.0);   // VR5 Bass 100K lin
        eB1Bot = n.addR (b17, b16, 50000.0);
        n.addC (b18, b17, 0.047e-6);           // C6
        n.addC (b17, b16, 0.047e-6);           // C7
        n.addR (b16, 0, 10000.0);              // R12
        n.addR (t, t21, 10000.0);              // R14
        eT1Top = n.addR (t21, t20, 50000.0);   // VR6 Treble 100K lin
        eT1Bot = n.addR (t20, t19, 50000.0);
        n.addR (t19, 0, 10000.0);              // R15
        n.addC (t20, N, 4700.0e-12);           // C8
        n.addR (b17, N, 100000.0);             // R13
        n.addC (N, m, 0.1e-6);                 // C9
        n.addR (m, o, 1.5e6);                  // R16
        n.addC (m, o, 330.0e-12);              // across R16
        n.setOpAmp (0, m, o, kGbw741, kSlew741, kSwing741, kKnee741);
        ch1bOut = o;
    }

    // ---- Ch 2 gain stage (AS00017 IC1) ----
    {
        Netlist& n = ch2a; n.clear();
        const int n1 = n.addNode(), n2 = n.addNode(), m = n.addNode(), n4 = n.addNode(),
                  n5 = n.addNode(), o = n.addNode();
        n.addInput (n1, kPickupR + 10000.0);   // pickup + R1 10K (Input 1)
        n.addR (n1, 0, 10000.0);               // R2 10K, Input 2 jack shorted when empty
        n.addC (n1, n2, 0.1e-6);               // C1
        n.addR (n2, 0, 100000.0);              // R3
        n.addR (o, m, 47000.0);                // R5
        n.addR (m, n4, 2200.0);                // R4
        n.addC (n4, n5, 6.8e-6);               // C2
        eVolFb = n.addR (n5, 0, 12500.0);      // VR1 Volume, CW end (pin 6) to wiper (7)
        n.setOpAmp (n2, m, o, kGbw741, kSlew741, kSwing741, kKnee741);
        ch2aOut = o;
    }

    // ---- Ch 2 tone stack (passive Baxandall) and IC2 (1M5 feedback) ----
    {
        Netlist& n = ch2b; n.clear();
        const int s = n.addNode(), a = n.addNode(), t = n.addNode(), b9 = n.addNode(),
                  b10 = n.addNode(), b11 = n.addNode(), N = n.addNode(), t12 = n.addNode(),
                  t13 = n.addNode(), t14 = n.addNode(), m = n.addNode(), o = n.addNode();
        n.addInput (s, kRout741);
        n.addC (s, a, 6.8e-6);                 // C3
        n.addR (a, t, 470.0);                  // R6
        eVolOut = n.addR (t, 0, 12500.0);      // VR1, pin 8 to wiper
        n.addR (t, b9, 10000.0);               // R7
        eB2Top = n.addR (b9, b10, 50000.0);    // VR3 Bass 100K lin
        eB2Bot = n.addR (b10, b11, 50000.0);
        n.addC (b9, b10, 0.047e-6);            // C4
        n.addC (b10, b11, 0.047e-6);           // C5
        n.addR (b11, 0, 10000.0);              // R8
        n.addR (t, t12, 6800.0);               // R11
        eT2Top = n.addR (t12, t13, 50000.0);   // VR4 Treble 100K lin
        eT2Bot = n.addR (t13, t14, 50000.0);
        n.addR (t14, 0, 10000.0);              // R12
        n.addC (t13, N, 1000.0e-12);           // C6
        n.addR (b10, N, 100000.0);             // R5
        n.addC (N, m, 2200.0e-12);             // C7: 1,000 pF on the drawing; 2,200 pF matches
                                               // the owner's amp by ear (Ch 2 was a little thin)
        n.addR (m, o, 1.5e6);                  // R10
        n.setOpAmp (0, m, o, kGbw741, kSlew741, kSwing741, kKnee741);
        ch2bOut = o;
    }

    // ---- Reverb driver (AS00018 IC1) ----
    {
        Netlist& n = revDrv; n.clear();
        const int p = n.addNode(), m = n.addNode(), x = n.addNode(), o = n.addNode();
        n.addInput (p, 10000.0);               // R2 10K from Ch 2
        n.addInput (p, 47000.0);               // R1 47K from Ch 1
        n.addR (p, 0, 2200.0);                 // R3
        n.addR (o, m, 33000.0);                // R5
        n.addC (m, x, 0.68e-6);                // C1
        n.addR (x, 0, 180.0);                  // R4
        n.setOpAmp (p, m, o, kGbw741, kSlew741, kSwing741, kKnee741);
        revDrvOut = o;
    }

    // ---- Reverb recovery (AS00018 IC2), base rate ----
    {
        Netlist& n = revRec; n.clear();
        const int p = n.addNode(), m = n.addNode(), x = n.addNode(), o = n.addNode();
        n.addInput (p, 200.0);                 // tank output transducer
        n.addR (p, 0, 4700.0);                 // R7
        n.addR (o, m, 270000.0);               // R8
        n.addC (o, m, 330.0e-12);              // C3
        n.addC (m, x, 0.1e-6);                 // C2
        n.addR (x, 0, 1000.0);                 // R6
        n.setOpAmp (p, m, o, kGbw741, kSlew741, kSwing741, kKnee741);
        revRecOut = o;
    }

    // ---- Output network and loudspeaker load ----
    // Two 1977 H/H 12" drivers wired as an 8 ohm pair. Thiele-Small-style
    // electrical equivalent: Re, lossy voice-coil inductance, and the motional
    // impedance as a parallel R-L-C at the in-cab resonance.
    {
        Netlist& n = speaker; n.clear();
        const int a = n.addNode(), o = n.addNode(), z = n.addNode(), p = n.addNode(), q = n.addNode();
        n.addInput (a, 0.01);                  // power amp output (feedback holds this node)
        n.addL (a, o, 3.0e-6);                 // L1 (value not marked; typical)
        n.addR (a, o, 15.0);                   // R29
        n.addR (o, z, 15.0);                   // R30 Zobel
        n.addC (z, 0, 0.047e-6);               // C14
        const double re = 6.4, fs = 90.0, qms = 3.5, qes = 0.55;
        const double ws = 2.0 * kPi * fs;
        const double cmes = qes / (ws * re);
        const double lces = 1.0 / (ws * ws * cmes);
        const double res = re * qms / qes;
        n.addR (o, p, re);                     // voice coil resistance
        n.addL (p, q, 0.5e-3);                 // voice coil inductance
        n.addR (p, q, 20.0);                   // eddy-current losses
        n.addR (q, 0, res);                    // motional: mechanical losses
        n.addL (q, 0, lces);                   //           suspension compliance
        n.addC (q, 0, cmes);                   //           moving mass
        spkOut = o;
        spkCone = q;
    }
}

//==============================================================================
void MusicianAmp::prepare (double sr)
{
    fsBase = sr;
    fsOs = sr * Oversampler4x::kFactor;
    smoothCoeff = 1.0 - std::exp (-1.0 / (0.02 * sr));

    ch1a.prepare (fsOs);
    ch1b.prepare (fsOs);
    ch2a.prepare (fsOs);
    ch2b.prepare (fsOs);
    revDrv.prepare (fsOs);
    revRec.prepare (fsBase);
    speaker.prepare (fsOs);
    tank.prepare (fsBase);

    // Power amp: input coupling C1 0.22u into R1 1K + R2 15K; feedback leg R3 390R + C2 6u8;
    // 741 gain-bandwidth over the closed-loop gain.
    paInHP.setHighPass1 (fsOs, 1.0 / (2.0 * kPi * 0.22e-6 * 16000.0));
    paFbHP.setHighPass1 (fsOs, 1.0 / (2.0 * kPi * 390.0 * 6.8e-6));
    paGbwLP.setLowPass1 (fsOs, kGbw741 / kPaGain);
    slewStep = kPaSlew * 1.0e6 / fsOs;

    // Valve Sound section filters
    valvePre.setLowPass (fsOs, 2800.0, 0.6);    // round off the 741's clipped edges
    valvePost.setLowPass1 (fsOs, 4500.0);
    valveDc.setHighPass1 (fsOs, 10.0);

    // Loudspeaker cone breakup and cabinet (base rate, after the physical low end)
    cab[0].setPeak (fsBase, 180.0, 1.0, 2.0);     // cabinet low-mid
    cab[1].setPeak (fsBase, 1500.0, 1.0, 2.5);    // cone breakup region
    cab[2].setPeak (fsBase, 2700.0, 1.1, 8.0);
    cab[3].setPeak (fsBase, 3800.0, 1.8, 5.0);
    const double lpF = fsBase * 0.45 < 5000.0 ? fsBase * 0.45 : 5000.0;
    cab[4].setLowPass (fsBase, lpF, 0.7);
    cab[5].setLowPass (fsBase, lpF * 1.15, 1.0);

    applyPots (true);
    applyModuleEq();

    // Normalise the miked speaker so 1 kHz matches the speaker-terminal level.
    {
        speaker.reset();
        for (int i = 0; i < 6; ++i) cab[i].reset();
        double prev = 0.0, peak = 0.0;
        const int nBase = (int) (0.12 * fsBase);
        const int F = Oversampler4x::kFactor;
        for (int i = 0; i < nBase; ++i)
        {
            double last = 0.0;
            for (int k = 0; k < F; ++k)
            {
                const double tt = (double) (i * F + k) / fsOs;
                const double vin = std::sin (2.0 * kPi * 1000.0 * tt);
                speaker.process (&vin);
                const double vc = speaker.v (spkCone);
                last = (vc - prev) * fsOs;
                prev = vc;
            }
            double y = last;
            for (int b = 0; b < 6; ++b) y = cab[b].process (y);
            if (i > nBase / 2 && std::fabs (y) > peak) peak = std::fabs (y);
        }
        cabNorm = peak > 0.0 ? 1.0 / peak : 1.0;
    }

    reset();
}

void MusicianAmp::reset()
{
    for (int i = 0; i < kNumKnobs; ++i) knobCur[i] = knobTarget[i];
    inGainC = inGainT;
    outGainC = outGainT;
    applyPots (true);
    applyModuleEq();

    ch1a.reset(); ch1b.reset(); ch2a.reset(); ch2b.reset();
    revDrv.reset(); revRec.reset(); speaker.reset();
    tank.reset();
    osMain.reset(); osSpkDown.reset(); osDrvDown.reset(); osRevUp.reset();
    spkMix = speakerOn ? 1.0 : 0.0;
    for (int k = 0; k < Oversampler4x::kFactor; ++k) revUp[k] = 0.0;

    voiceLow.reset(); voiceMid.reset(); voiceHigh.reset(); midEq.reset(); presEq.reset();
    valveDc.reset(); valvePre.reset(); valvePost.reset();
    paInHP.reset(); paFbHP.reset(); paGbwLP.reset();
    for (int i = 0; i < 6; ++i) cab[i].reset();

    railPos = railNeg = kRailNoLoad;
    vOutPrev = 0.0; lastVspk = 0.0; coneVPrev = 0.0;
    climHit = false;
    potCountdown = 0;
    ch1Active = (channel != kChannel2);
    ch2Active = (channel != kChannel1);
}

//==============================================================================
// Pots

void MusicianAmp::applyPots (bool force)
{
    (void) force;
    const double R25 = 25000.0, R100 = 100000.0;

    // VR2 Gain, 25K reverse-log, dual action: the CW leg shunts the feedback
    // (more gain clockwise), the other leg shunts the stage output to ground.
    {
        const double rfb = R25 * audioTaper (1.0 - knobCur[kCh1Gain]);
        ch1a.setR (eGainFb, potR (rfb));
        ch1b.setR (eGainOut, potR (R25 - rfb));
    }
    ch1a.setR (eHeavy, heavy ? 1000.0 : 1.0e9);
    heavyApplied = heavy;
    {
        const double b = knobCur[kCh1Bass], t = knobCur[kCh1Treb];
        ch1b.setR (eB1Top, potR (R100 * (1.0 - b)));
        ch1b.setR (eB1Bot, potR (R100 * b));
        ch1b.setR (eT1Top, potR (R100 * (1.0 - t)));
        ch1b.setR (eT1Bot, potR (R100 * t));
    }
    // VR1 Volume, same arrangement as Ch 1 Gain
    {
        const double rfb = R25 * audioTaper (1.0 - knobCur[kCh2Vol]);
        ch2a.setR (eVolFb, potR (rfb));
        ch2b.setR (eVolOut, potR (R25 - rfb));
    }
    {
        const double b = knobCur[kCh2Bass], t = knobCur[kCh2Treb];
        ch2b.setR (eB2Top, potR (R100 * (1.0 - b)));
        ch2b.setR (eB2Bot, potR (R100 * b));
        ch2b.setR (eT2Top, potR (R100 * (1.0 - t)));
        ch2b.setR (eT2Bot, potR (R100 * t));
    }
    ch1a.rebuild(); ch1b.rebuild(); ch2a.rebuild(); ch2b.rebuild();
}

// V-S tone correction module (AS00015/A). Its insides are not on the drawing;
// these sections reproduce what its controls are wired to do.
void MusicianAmp::applyModuleEq()
{
    if (heavy)
    {
        voiceLow.setLowShelf (fsOs, 120.0, 3.0);
        voiceMid.setPeak (fsOs, 750.0, 0.8, -4.0);
        voiceHigh.setIdentity();
    }
    else
    {
        voiceLow.setIdentity();
        voiceMid.setIdentity();
        voiceHigh.setHighShelf (fsOs, 2500.0, 2.5);
    }
    midEq.setPeak (fsOs, 700.0, 0.7, (knobCur[kMid] - 0.5) * 24.0);       // +/-12 dB
    presEq.setHighShelf (fsOs, 3000.0, (knobCur[kPres] - 0.5) * 20.0);    // +/-10 dB
}

//==============================================================================
// Setters

void MusicianAmp::setCh2Volume (double v)    { knobTarget[kCh2Vol]  = clamp01 (v); }
void MusicianAmp::setCh2Bass (double v)      { knobTarget[kCh2Bass] = clamp01 (v); }
void MusicianAmp::setCh2Treble (double v)    { knobTarget[kCh2Treb] = clamp01 (v); }
void MusicianAmp::setCh2Reverb (bool on)     { ch2Rev = on; }
void MusicianAmp::setCh1Gain (double v)      { knobTarget[kCh1Gain] = clamp01 (v); }
void MusicianAmp::setCh1Bass (double v)      { knobTarget[kCh1Bass] = clamp01 (v); }
void MusicianAmp::setMiddle (double v)       { knobTarget[kMid]     = clamp01 (v); }
void MusicianAmp::setCh1Treble (double v)    { knobTarget[kCh1Treb] = clamp01 (v); }
void MusicianAmp::setPresence (double v)     { knobTarget[kPres]    = clamp01 (v); }
void MusicianAmp::setCh1Reverb (bool on)     { ch1Rev = on; }
void MusicianAmp::setVoiceHeavy (bool h)     { heavy = h; }
void MusicianAmp::setValveSound (bool on)    { valve = on; }
void MusicianAmp::setMasterReverb (double v) { knobTarget[kMRev]    = clamp01 (v); }
void MusicianAmp::setMasterVolume (double v) { knobTarget[kMVol]    = clamp01 (v); }
void MusicianAmp::setChannel (int ch)        { channel = (ch < 0 || ch > 2) ? 0 : ch; }
void MusicianAmp::setSpeaker (bool on)       { speakerOn = on; }
void MusicianAmp::setInputGainDb (double dB) { inGainT = dbToGain (dB); }
void MusicianAmp::setOutputGainDb (double dB){ outGainT = dbToGain (dB); }

//==============================================================================
// Signal path

double MusicianAmp::processCh1 (double g)
{
    ch1a.process (&g);
    double v = ch1a.v (ch1aOut);
    ch1b.process (&v);
    double x = ch1b.v (ch1bOut);      // into the module via R17 1K (C10 47u blocks DC)

    // V-S module
    x = voiceHigh.process (voiceMid.process (voiceLow.process (x)));
    if (valve)
    {
        // Asymmetric soft saturation and smoothing in place of the 741's hard edge.
        double u = valvePre.process (x) * (1.0 / kSwing741);
        const double bias = 0.3, drive = 1.8;
        const double t0 = std::tanh (bias);
        const double slope = drive * (1.0 - t0 * t0);
        u = (std::tanh (drive * u + bias) - t0) / slope;
        x = valvePost.process (valveDc.process (u)) * kSwing741 * 1.6;
    }
    x = presEq.process (midEq.process (x));
    return x;
}

double MusicianAmp::processCh2 (double g)
{
    ch2a.process (&g);
    double v = ch2a.v (ch2aOut);
    ch2b.process (&v);
    return ch2b.v (ch2bOut);          // C8 6u8, R13 10K to the mix
}

double MusicianAmp::powerAmp (double vIn)
{
    // Closed-loop response: (1 + Rf/R3 * HP(R3, C2)) with the input coupling
    // high-pass and the 741's bandwidth.
    const double x = paInHP.process (vIn);
    double v = x + (kPaGain - 1.0) * paFbHP.process (x);
    v = paGbwLP.process (v);

    // Slew rate
    double d = v - vOutPrev;
    if (d > slewStep) d = slewStep; else if (d < -slewStep) d = -slewStep;
    v = vOutPrev + d;

    // Clipping against the sagging rails
    const double lim = (v >= 0.0 ? railPos : railNeg) - kRailDrop;
    const double lin = lim - kRailKnee;
    const double av = std::fabs (v);
    if (av > lin)
    {
        const double y = lin + kRailKnee * (1.0 - std::exp (-(av - lin) / kRailKnee));
        v = v < 0.0 ? -y : y;
    }

    // V-I current limiting into the actual speaker load
    double i0, gIn;
    speaker.inputCurrentTerms (i0, gIn);
    double i = i0 + gIn * v;
    climHit = false;
    if (i > kIMax)       { v = (kIMax - i0) / gIn;  i = kIMax;  climHit = true; }
    else if (i < -kIMax) { v = (-kIMax - i0) / gIn; i = -kIMax; climHit = true; }

    speaker.process (&v);
    vOutPrev = v;
    if (std::fabs (i) > peakAmps) peakAmps = std::fabs (i);

    // Reservoir capacitors: each rail supplies its half of the load and is
    // recharged through the transformer and bridge.
    const double dt = 1.0 / fsOs;
    const double iPos = kIQuiet + (i > 0.0 ? i : 0.0);
    const double iNeg = kIQuiet + (i < 0.0 ? -i : 0.0);
    const double chP = railPos < kRailNoLoad ? (kRailNoLoad - railPos) / kRailRs : 0.0;
    const double chN = railNeg < kRailNoLoad ? (kRailNoLoad - railNeg) / kRailRs : 0.0;
    railPos += (chP - iPos) * dt / kRailC;
    railNeg += (chN - iNeg) * dt / kRailC;
    if (railPos < 5.0) railPos = 5.0;
    if (railNeg < 5.0) railNeg = 5.0;

    lastVspk = speaker.v (spkOut);
    return lastVspk;
}

void MusicianAmp::process (const float* in, float* out, int numSamples)
{
    const int F = Oversampler4x::kFactor;
    const double a = smoothCoeff;
    const double aBlock = 1.0 - std::pow (1.0 - a, (double) kPotUpdateInterval);

    // Channel selection: an unused channel is reset when it comes back.
    const bool want1 = (channel != kChannel2), want2 = (channel != kChannel1);
    if (want1 && ! ch1Active) { ch1a.reset(); ch1b.reset(); }
    if (want2 && ! ch2Active) { ch2a.reset(); ch2b.reset(); }
    ch1Active = want1; ch2Active = want2;

    double up[F], drv[F], term[F], spk[F];

    for (int n = 0; n < numSamples; ++n)
    {
        if (--potCountdown <= 0)
        {
            potCountdown = kPotUpdateInterval;
            bool moved = (heavy != heavyApplied);
            bool modMoved = moved;
            for (int k = 0; k < kNumKnobs; ++k)
            {
                const double d = knobTarget[k] - knobCur[k];
                if (std::fabs (d) > 1.0e-5)
                {
                    knobCur[k] += d * aBlock;
                    if (std::fabs (knobTarget[k] - knobCur[k]) < 1.0e-5) knobCur[k] = knobTarget[k];
                    if (k == kMid || k == kPres) modMoved = true;
                    else if (k != kMRev && k != kMVol) moved = true;
                }
            }
            if (moved) applyPots (false);
            if (modMoved) applyModuleEq();
        }

        inGainC  += (inGainT - inGainC) * a;
        outGainC += (outGainT - outGainC) * a;
        const double mVol = audioTaper (knobCur[kMVol]);
        const double mRev = knobCur[kMRev];   // 1K linear

        const double x = (double) in[n] * inGainC;
        osMain.upsample (x, up);

        for (int k = 0; k < F; ++k)
        {
            const double v1 = ch1Active ? processCh1 (up[k]) : 0.0;
            const double v2 = ch2Active ? processCh2 (up[k]) : 0.0;

            const double sends[2] = { ch2Rev ? v2 : 0.0, ch1Rev ? v1 : 0.0 };
            revDrv.process (sends);
            drv[k] = revDrv.v (revDrvOut);

            const double mix = kMixCh * (v1 + v2) + kMixRev * mRev * revUp[k];
            const double vs = powerAmp (mix * mVol);

            // Both outputs run all the time so the Speaker switch can crossfade cleanly.
            const double vc = speaker.v (spkCone);
            spk[k] = (vc - coneVPrev) * fsOs;   // cone acceleration ~ radiated pressure
            coneVPrev = vc;
            term[k] = vs;
        }

        // Spring reverb at the base rate; its return feeds the next sample's mix.
        const double drvBase = osDrvDown.downsample (drv);
        const double tankOut = tank.process (drvBase);
        revRec.process (&tankOut);
        osRevUp.upsample (revRec.v (revRecOut), revUp);

        const double yTerm = osMain.downsample (term);
        double yAc = osSpkDown.downsample (spk);
        for (int b = 0; b < 6; ++b) yAc = cab[b].process (yAc);
        yAc *= cabNorm;
        spkMix += ((speakerOn ? 1.0 : 0.0) - spkMix) * a;
        const double y = yTerm + (yAc - yTerm) * spkMix;

        double o = y * (outGainC / kVFullScale);
        if (! (o == o)) { reset(); o = 0.0; }
        out[n] = (float) o;
    }
}

} // namespace hhvs
