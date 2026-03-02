#include "Inductor.h"
#include <complex>

Inductor::Inductor(double inductance)
    : Component(ComponentType::Inductor, 2, inductance)
{
}

QString Inductor::valueString() const
{
    return formatEngineering(m_value, valueUnit());
}

void Inductor::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, int /*size*/) const
{
    // Inductor is a near-short circuit in DC: stamp large conductance
    int n1 = m_pins[0].nodeId - 1;
    int n2 = m_pins[1].nodeId - 1;
    double g = 1.0 / 0.001; // R = 1mΩ

    if (n1 >= 0) G(n1, n1) += g;
    if (n2 >= 0) G(n2, n2) += g;
    if (n1 >= 0 && n2 >= 0) {
        G(n1, n2) -= g;
        G(n2, n1) -= g;
    }
}

void Inductor::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                       double omega, int /*size*/) const
{
    int n1 = m_pins[0].nodeId - 1;
    int n2 = m_pins[1].nodeId - 1;
    // Admittance Y = 1/(jωL) = -j/(ωL)
    std::complex<double> y(0.0, -1.0 / (omega * m_value));

    if (n1 >= 0) Y(n1, n1) += y;
    if (n2 >= 0) Y(n2, n2) += y;
    if (n1 >= 0 && n2 >= 0) {
        Y(n1, n2) -= y;
        Y(n2, n1) -= y;
    }
}
