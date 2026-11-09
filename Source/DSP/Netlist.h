// Netlist.h
// Small linear circuit solver used for every op-amp stage and for the speaker.
//
// Modified nodal analysis with trapezoidal companions for capacitors and
// inductors. At most one op-amp per netlist, modelled as a 741: a single-pole
// integrator (gain-bandwidth product) whose output is limited by slew rate and
// by a soft-kneed swing limit. When the op-amp's unconstrained answer would
// break either limit, the sample is solved again with the output forced to the
// limited value, so the rest of the circuit (feedback caps, loading) sees what
// the real chip does while it is clipping or slewing.
//
// Matrices are inverted when values change (rebuild()), never in process().
// Plain C++98, fixed-size storage, no allocation.

#ifndef TBVS_NETLIST_H
#define TBVS_NETLIST_H

namespace hhvs
{

class Netlist
{
public:
    enum { kMaxNodes = 15, kMaxDim = kMaxNodes + 1, kMaxElems = 24, kMaxInputs = 2 };

    Netlist();

    void clear();
    int  addNode();                                    // returns node id (1..); ground is 0
    int  addR (int a, int b, double ohms);             // returns element index
    int  addC (int a, int b, double farads);
    int  addL (int a, int b, double henries);
    int  addInput (int node, double sourceOhms);       // Norton source; returns input index
    void setOpAmp (int plus, int minus, int out,
                   double gbwHz, double slewVPerUs, double swing, double knee);

    void setR (int elem, double ohms);                 // call rebuild() afterwards
    void prepare (double sampleRate);                  // builds and inverts
    void rebuild();                                    // after setR, keeps state
    void reset();                                      // zero state (all-AC circuit: zero is the DC solution)

    // One sample. inputs[k] are the open-circuit source voltages.
    void process (const double* inputs);

    double v (int node) const { return node > 0 ? x[node - 1] : 0.0; }
    bool   opAmpLimited() const { return limited; }

    // Current delivered by input 0's source into the circuit, as I = i0 + g * Vs,
    // where g is the small-signal input conductance and i0 the part due to state.
    // Used by the power amp's current limiter. Only valid for netlists without an op-amp.
    void inputCurrentTerms (double& i0, double& g) const;

private:
    enum Kind { kRes, kCap, kInd };
    struct Elem { int kind; int a, b; double value; double g; double J; };

    void build (double A[kMaxDim][kMaxDim], bool forced) const;
    void stamp (double A[kMaxDim][kMaxDim], int a, int b, double g) const;
    void buildRhs (double* rhs, const double* inputs, bool forced, double vForced) const;
    void solveWith (const double Ainv[kMaxDim][kMaxDim], const double* rhs, double* out) const;
    double limitOut (double vCand) const;
    void updateState();

    static bool invert (double A[kMaxDim][kMaxDim], double Ainv[kMaxDim][kMaxDim], int n);

    int nNodes, nElems, nInputs, dim;
    Elem elems[kMaxElems];
    int inNode[kMaxInputs];
    double inG[kMaxInputs];

    bool hasOp;
    int opP, opM, opOut;
    double opGbw, opSlew, opSwing, opKnee;
    double kInt;        // backward-Euler integrator gain, wt * T
    double slewStep;    // max output change per sample
    double vOutPrev;
    bool limited;

    double T;
    double Ainv[kMaxDim][kMaxDim];
    double AinvF[kMaxDim][kMaxDim];
    double x[kMaxDim];

    // For inputCurrentTerms
    double inV0;        // last input voltage applied
};

} // namespace hhvs

#endif
