#include "Lamp.h"

Lamp::Lamp(double resistance)
    : Component(ComponentType::Lamp, 2, resistance)
{
}

QString Lamp::valueString() const
{
    return formatEngineering(m_value, valueUnit());
}

void Lamp::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, int /*size*/) const
{
    // Lamp is modeled as a pure resistor
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

void Lamp::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                   double /*omega*/, int /*size*/) const
{
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
