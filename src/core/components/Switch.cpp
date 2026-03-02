#include "Switch.h"

Switch::Switch(double contactResistance)
    : Component(ComponentType::Switch2Way, 2, contactResistance)
{
}

QString Switch::valueString() const
{
    return formatEngineering(m_value, valueUnit());
}

void Switch::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, int /*size*/) const
{
    if (!m_closed) return; // Open switch: no connection

    int n1 = m_pins[0].nodeId - 1;
    int n2 = m_pins[1].nodeId - 1;
    double g = 1.0 / m_value;

    if (n1 >= 0) G(n1, n1) += g;
    if (n2 >= 0) G(n2, n2) += g;
    if (n1 >= 0 && n2 >= 0) {
        G(n1, n2) -= g;
        G(n2, n1) -= g;
    }
}

void Switch::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                     double /*omega*/, int /*size*/) const
{
    if (!m_closed) return;

    int n1 = m_pins[0].nodeId - 1;
    int n2 = m_pins[1].nodeId - 1;
    std::complex<double> g(1.0 / m_value, 0.0);

    if (n1 >= 0) Y(n1, n1) += g;
    if (n2 >= 0) Y(n2, n2) += g;
    if (n1 >= 0 && n2 >= 0) {
        Y(n1, n2) -= g;
        Y(n2, n1) -= g;
    }
}
