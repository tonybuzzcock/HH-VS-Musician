// Netlist.cpp

#include "Netlist.h"

#include <cmath>
#include <cstring>

namespace hhvs
{

Netlist::Netlist()
{
    clear();
}

void Netlist::clear()
{
    nNodes = 0;
    nElems = 0;
    nInputs = 0;
    dim = 0;
    hasOp = false;
    opP = opM = opOut = 0;
    opGbw = 1.0e6; opSlew = 0.5; opSwing = 14.0; opKnee = 1.0;
    kInt = 0.0; slewStep = 1.0e9;
    vOutPrev = 0.0;
    limited = false;
    T = 1.0 / 176400.0;
    inV0 = 0.0;
    std::memset (x, 0, sizeof (x));
    std::memset (Ainv, 0, sizeof (Ainv));
    std::memset (AinvF, 0, sizeof (AinvF));
}

int Netlist::addNode()
{
    if (nNodes >= kMaxNodes) return 0;
    return ++nNodes;
}

int Netlist::addR (int a, int b, double ohms)
{
    if (nElems >= kMaxElems) return -1;
    Elem& e = elems[nElems];
    e.kind = kRes; e.a = a; e.b = b; e.value = ohms; e.g = 0.0; e.J = 0.0;
    return nElems++;
}

int Netlist::addC (int a, int b, double farads)
{
    if (nElems >= kMaxElems) return -1;
    Elem& e = elems[nElems];
    e.kind = kCap; e.a = a; e.b = b; e.value = farads; e.g = 0.0; e.J = 0.0;
    return nElems++;
}

int Netlist::addL (int a, int b, double henries)
{
    if (nElems >= kMaxElems) return -1;
    Elem& e = elems[nElems];
    e.kind = kInd; e.a = a; e.b = b; e.value = henries; e.g = 0.0; e.J = 0.0;
    return nElems++;
}

int Netlist::addInput (int node, double sourceOhms)
{
    if (nInputs >= kMaxInputs) return -1;
    inNode[nInputs] = node;
    inG[nInputs] = 1.0 / sourceOhms;
    return nInputs++;
}

void Netlist::setOpAmp (int plus, int minus, int out, double gbwHz, double slewVPerUs,
                        double swing, double knee)
{
    hasOp = true;
    opP = plus; opM = minus; opOut = out;
    opGbw = gbwHz; opSlew = slewVPerUs; opSwing = swing; opKnee = knee;
}

void Netlist::setR (int elem, double ohms)
{
    if (elem < 0 || elem >= nElems) return;
    if (ohms < 1.0e-3) ohms = 1.0e-3;
    elems[elem].value = ohms;
}

void Netlist::prepare (double sampleRate)
{
    T = 1.0 / sampleRate;
    rebuild();
    reset();
}

void Netlist::stamp (double A[kMaxDim][kMaxDim], int a, int b, double g) const
{
    if (a > 0) A[a - 1][a - 1] += g;
    if (b > 0) A[b - 1][b - 1] += g;
    if (a > 0 && b > 0)
    {
        A[a - 1][b - 1] -= g;
        A[b - 1][a - 1] -= g;
    }
}

void Netlist::build (double A[kMaxDim][kMaxDim], bool forced) const
{
    for (int r = 0; r < kMaxDim; ++r)
        for (int c = 0; c < kMaxDim; ++c)
            A[r][c] = 0.0;

    for (int i = 0; i < nElems; ++i)
        stamp (A, elems[i].a, elems[i].b, elems[i].g);

    for (int k = 0; k < nInputs; ++k)
        stamp (A, inNode[k], 0, inG[k]);

    if (hasOp)
    {
        const int iRow = nNodes;               // extra unknown: op-amp output current
        A[opOut - 1][iRow] -= 1.0;             // current injected into the output node
        A[iRow][opOut - 1] = 1.0;
        if (! forced)
        {
            // Backward-Euler integrator: Vout(n) - k (Vp - Vm) = Vout(n-1)
            if (opP > 0) A[iRow][opP - 1] -= kInt;
            if (opM > 0) A[iRow][opM - 1] += kInt;
        }
    }
}

bool Netlist::invert (double A[kMaxDim][kMaxDim], double Ai[kMaxDim][kMaxDim], int n)
{
    double M[kMaxDim][2 * kMaxDim];
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
        {
            M[r][c] = A[r][c];
            M[r][c + n] = (r == c) ? 1.0 : 0.0;
        }

    for (int col = 0; col < n; ++col)
    {
        int piv = col;
        double best = std::fabs (M[col][col]);
        for (int r = col + 1; r < n; ++r)
            if (std::fabs (M[r][col]) > best) { best = std::fabs (M[r][col]); piv = r; }
        if (best < 1.0e-300) return false;
        if (piv != col)
            for (int c = 0; c < 2 * n; ++c)
            {
                const double t = M[col][c]; M[col][c] = M[piv][c]; M[piv][c] = t;
            }
        const double inv = 1.0 / M[col][col];
        for (int c = 0; c < 2 * n; ++c) M[col][c] *= inv;
        for (int r = 0; r < n; ++r)
        {
            if (r == col) continue;
            const double f = M[r][col];
            if (f == 0.0) continue;
            for (int c = 0; c < 2 * n; ++c) M[r][c] -= f * M[col][c];
        }
    }
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            Ai[r][c] = M[r][c + n];
    return true;
}

void Netlist::rebuild()
{
    dim = nNodes + (hasOp ? 1 : 0);

    for (int i = 0; i < nElems; ++i)
    {
        Elem& e = elems[i];
        if (e.kind == kRes)      e.g = 1.0 / e.value;
        else if (e.kind == kCap) e.g = 2.0 * e.value / T;
        else                     e.g = T / (2.0 * e.value);
    }

    const double pi = 3.14159265358979323846;
    kInt = 2.0 * pi * opGbw * T;
    slewStep = opSlew * 1.0e6 * T;

    double A[kMaxDim][kMaxDim];
    build (A, false);
    if (! invert (A, Ainv, dim))
        std::memset (Ainv, 0, sizeof (Ainv));
    if (hasOp)
    {
        build (A, true);
        if (! invert (A, AinvF, dim))
            std::memset (AinvF, 0, sizeof (AinvF));
    }
}

void Netlist::reset()
{
    for (int i = 0; i < nElems; ++i)
        elems[i].J = 0.0;
    std::memset (x, 0, sizeof (x));
    vOutPrev = 0.0;
    limited = false;
    inV0 = 0.0;
}

void Netlist::buildRhs (double* rhs, const double* inputs, bool forced, double vForced) const
{
    for (int i = 0; i < dim; ++i) rhs[i] = 0.0;

    for (int i = 0; i < nElems; ++i)
    {
        const Elem& e = elems[i];
        if (e.kind == kRes) continue;
        // Capacitor: i(a->b) = g v - J   -> inject +J into a, -J into b
        // Inductor:  i(a->b) = g v + J   -> inject -J into a, +J into b
        const double inj = (e.kind == kCap) ? e.J : -e.J;
        if (e.a > 0) rhs[e.a - 1] += inj;
        if (e.b > 0) rhs[e.b - 1] -= inj;
    }

    for (int k = 0; k < nInputs; ++k)
        if (inNode[k] > 0)
            rhs[inNode[k] - 1] += inputs[k] * inG[k];

    if (hasOp)
        rhs[nNodes] = forced ? vForced : vOutPrev;
}

void Netlist::solveWith (const double A[kMaxDim][kMaxDim], const double* rhs, double* out) const
{
    for (int r = 0; r < dim; ++r)
    {
        double s = 0.0;
        const double* row = A[r];
        for (int c = 0; c < dim; ++c)
            s += row[c] * rhs[c];
        out[r] = s;
    }
}

// Slew limit, then a soft knee into the output swing limit.
double Netlist::limitOut (double vCand) const
{
    double d = vCand - vOutPrev;
    if (d > slewStep) d = slewStep;
    else if (d < -slewStep) d = -slewStep;
    double v = vOutPrev + d;

    const double lin = opSwing - opKnee;
    const double a = std::fabs (v);
    if (a > lin)
    {
        const double over = (a - lin) / opKnee;
        const double y = lin + opKnee * (1.0 - std::exp (-over));
        v = v < 0.0 ? -y : y;
    }
    return v;
}

void Netlist::updateState()
{
    for (int i = 0; i < nElems; ++i)
    {
        Elem& e = elems[i];
        if (e.kind == kRes) continue;
        const double va = e.a > 0 ? x[e.a - 1] : 0.0;
        const double vb = e.b > 0 ? x[e.b - 1] : 0.0;
        const double vv = va - vb;
        if (e.kind == kCap)
            e.J = 2.0 * e.g * vv - e.J;
        else
            e.J = 2.0 * e.g * vv + e.J;
    }
    if (hasOp)
        vOutPrev = x[opOut - 1];
}

void Netlist::process (const double* inputs)
{
    double rhs[kMaxDim];
    buildRhs (rhs, inputs, false, 0.0);
    solveWith (Ainv, rhs, x);
    limited = false;

    if (hasOp)
    {
        const double vc = x[opOut - 1];
        const double vl = limitOut (vc);
        if (std::fabs (vl - vc) > 1.0e-9)
        {
            buildRhs (rhs, inputs, true, vl);
            solveWith (AinvF, rhs, x);
            limited = true;
        }
    }

    if (nInputs > 0) inV0 = inputs[0];

    // NaN/runaway guard: a bad sample resets the stage rather than poisoning it.
    for (int i = 0; i < dim; ++i)
        if (! (x[i] == x[i]) || std::fabs (x[i]) > 1.0e6)
        {
            reset();
            return;
        }

    updateState();
}

void Netlist::inputCurrentTerms (double& i0, double& g) const
{
    // Response to the state alone (source shorted) and to a unit source with zero state.
    double rhs[kMaxDim], sol[kMaxDim];
    const double zero[kMaxInputs] = { 0.0, 0.0 };
    buildRhs (rhs, zero, false, 0.0);
    solveWith (Ainv, rhs, sol);
    const int n = inNode[0];
    // I = (Vs - Vn) * G ;  with Vs = 0: I0 = -Vn0 * G
    i0 = -sol[n - 1] * inG[0];
    // Unit source, zero state: Vn = Ainv[n][n] * G
    const double vn1 = Ainv[n - 1][n - 1] * inG[0];
    g = (1.0 - vn1) * inG[0];
}

} // namespace hhvs
