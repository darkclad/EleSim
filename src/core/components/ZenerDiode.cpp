#include "ZenerDiode.h"
#include <complex>

ZenerDiode::ZenerDiode(double forwardVoltage, double zenerVoltage)
    : Component(ComponentType::ZenerDiode, 2, forwardVoltage)
    , m_zenerVoltage(zenerVoltage)
{
}

QString ZenerDiode::valueString() const
{
    return QString::number(m_zenerVoltage, 'g', 3) + " " + valueUnit();
}

void ZenerDiode::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int /*size*/) const
{
    // Default DC stamp: forward biased (initial guess for iterative DC)
    stampWithState(G, rhs, Forward);
}

void ZenerDiode::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                         double /*omega*/, int /*size*/) const
{
    // AC small-signal: forward resistance (same as regular diode)
    int n1 = m_pins[0].nodeId - 1;
    int n2 = m_pins[1].nodeId - 1;
    std::complex<double> g(1.0 / R_on, 0.0);

    if (n1 >= 0) Y(n1, n1) += g;
    if (n2 >= 0) Y(n2, n2) += g;
    if (n1 >= 0 && n2 >= 0) {
        Y(n1, n2) -= g;
        Y(n2, n1) -= g;
    }
}

void ZenerDiode::stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, State state) const
{
    // Anode = pin 0, Cathode = pin 1
    // Vd = V(anode) - V(cathode)
    // Forward: Vd > Vf  → low R, offset by Vf
    // Blocking: -Vz < Vd < Vf → high R
    // Breakdown: Vd < -Vz → low R, offset by -Vz
    int n1 = m_pins[0].nodeId - 1; // anode
    int n2 = m_pins[1].nodeId - 1; // cathode

    double geq;
    if (state == Blocking)
        geq = 1.0 / R_off;
    else
        geq = 1.0 / R_on;

    // Conductance stamp
    if (n1 >= 0) G(n1, n1) += geq;
    if (n2 >= 0) G(n2, n2) += geq;
    if (n1 >= 0 && n2 >= 0) {
        G(n1, n2) -= geq;
        G(n2, n1) -= geq;
    }

    if (state == Forward) {
        // Forward voltage offset: I = (Vd - Vf) / R_on
        // Current source Ieq = Vf / R_on flows from anode to cathode
        double ieq = m_value / R_on;
        if (n1 >= 0) rhs(n1) += ieq;
        if (n2 >= 0) rhs(n2) -= ieq;
    } else if (state == Breakdown) {
        // Breakdown voltage offset: I = (Vd + Vz) / R_on  (Vd is negative here)
        // Current source Ieq = Vz / R_on flows from cathode to anode
        double ieq = m_zenerVoltage / R_on;
        if (n1 >= 0) rhs(n1) -= ieq;
        if (n2 >= 0) rhs(n2) += ieq;
    }
}
