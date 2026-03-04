#include "PMosfet.h"
#include <complex>

PMosfet::PMosfet(double thresholdVoltage)
    : Component(ComponentType::PMosfet, 3, thresholdVoltage)
{
}

QString PMosfet::valueString() const
{
    return QString::number(m_value, 'g', 3) + " " + valueUnit();
}

void PMosfet::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int /*size*/) const
{
    // Default DC stamp: assume ON (initial guess for iterative DC)
    stampWithState(G, rhs, On);
}

void PMosfet::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                       double /*omega*/, int /*size*/) const
{
    // AC small-signal: on-resistance between Source and Drain
    // Pin 0 = Gate (no current), Pin 1 = Source, Pin 2 = Drain
    int nS = m_pins[1].nodeId - 1;
    int nD = m_pins[2].nodeId - 1;
    std::complex<double> g(1.0 / R_on, 0.0);

    if (nS >= 0) Y(nS, nS) += g;
    if (nD >= 0) Y(nD, nD) += g;
    if (nS >= 0 && nD >= 0) {
        Y(nS, nD) -= g;
        Y(nD, nS) -= g;
    }
}

void PMosfet::stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, State state) const
{
    // Pin 0 = Gate (infinite impedance, no stamp)
    // Pin 1 = Source, Pin 2 = Drain
    // Cutoff: high resistance between S-D
    // On: low resistance between S-D
    int nS = m_pins[1].nodeId - 1;
    int nD = m_pins[2].nodeId - 1;

    double geq = (state == On) ? (1.0 / R_on) : (1.0 / R_off);

    // Conductance stamp between Source and Drain
    if (nS >= 0) G(nS, nS) += geq;
    if (nD >= 0) G(nD, nD) += geq;
    if (nS >= 0 && nD >= 0) {
        G(nS, nD) -= geq;
        G(nD, nS) -= geq;
    }
}
