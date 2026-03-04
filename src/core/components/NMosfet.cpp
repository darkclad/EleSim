#include "NMosfet.h"
#include <complex>

NMosfet::NMosfet(double thresholdVoltage)
    : Component(ComponentType::NMosfet, 3, thresholdVoltage)
{
}

QString NMosfet::valueString() const
{
    return QString::number(m_value, 'g', 3) + " " + valueUnit();
}

void NMosfet::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int /*size*/) const
{
    // Default DC stamp: assume ON (initial guess for iterative DC)
    stampWithState(G, rhs, On);
}

void NMosfet::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                       double /*omega*/, int /*size*/) const
{
    // AC small-signal: on-resistance between Drain and Source
    // Pin 0 = Gate (no current), Pin 1 = Drain, Pin 2 = Source
    int nD = m_pins[1].nodeId - 1;
    int nS = m_pins[2].nodeId - 1;
    std::complex<double> g(1.0 / R_on, 0.0);

    if (nD >= 0) Y(nD, nD) += g;
    if (nS >= 0) Y(nS, nS) += g;
    if (nD >= 0 && nS >= 0) {
        Y(nD, nS) -= g;
        Y(nS, nD) -= g;
    }
}

void NMosfet::stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, State state) const
{
    // Pin 0 = Gate (infinite impedance, no stamp)
    // Pin 1 = Drain, Pin 2 = Source
    // Cutoff: high resistance between D-S
    // On: low resistance between D-S
    int nD = m_pins[1].nodeId - 1;
    int nS = m_pins[2].nodeId - 1;

    double geq = (state == On) ? (1.0 / R_on) : (1.0 / R_off);

    // Conductance stamp between Drain and Source
    if (nD >= 0) G(nD, nD) += geq;
    if (nS >= 0) G(nS, nS) += geq;
    if (nD >= 0 && nS >= 0) {
        G(nD, nS) -= geq;
        G(nS, nD) -= geq;
    }
}
