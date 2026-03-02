#include "Diode.h"
#include <complex>

Diode::Diode(double forwardVoltage)
    : Component(ComponentType::Diode, 2, forwardVoltage)
{
}

QString Diode::valueString() const
{
    return QString::number(m_value, 'g', 3) + " " + valueUnit();
}

void Diode::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int /*size*/) const
{
    // Default DC stamp: forward biased (initial guess for iterative DC)
    stampWithState(G, rhs, true);
}

void Diode::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                    double /*omega*/, int /*size*/) const
{
    // AC small-signal: forward resistance
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

void Diode::stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, bool forward) const
{
    // Anode = pin 0 (current flows in), Cathode = pin 1 (current flows out)
    // Forward biased: low resistance R_on with Vf voltage drop
    // Reverse biased: high resistance R_off (blocks current)
    int n1 = m_pins[0].nodeId - 1;
    int n2 = m_pins[1].nodeId - 1;

    double geq = forward ? (1.0 / R_on) : (1.0 / R_off);

    // Conductance stamp
    if (n1 >= 0) G(n1, n1) += geq;
    if (n2 >= 0) G(n2, n2) += geq;
    if (n1 >= 0 && n2 >= 0) {
        G(n1, n2) -= geq;
        G(n2, n1) -= geq;
    }

    // Forward voltage offset: models the ~0.7V drop
    // Diode current I = (Vd - Vf) / R_on when forward biased
    // The Vf offset acts as a current source: Ieq = Vf / R_on
    if (forward) {
        double ieq = m_value / R_on;
        if (n1 >= 0) rhs(n1) += ieq;
        if (n2 >= 0) rhs(n2) -= ieq;
    }
}
