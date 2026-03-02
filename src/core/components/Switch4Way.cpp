#include "Switch4Way.h"

Switch4Way::Switch4Way(double contactResistance)
    : Component(ComponentType::Switch4Way, 4, contactResistance)
{
}

QString Switch4Way::valueString() const
{
    return formatEngineering(m_value, valueUnit());
}

static void stampPair(Eigen::MatrixXd& G, int n1, int n2, double g)
{
    if (n1 >= 0) G(n1, n1) += g;
    if (n2 >= 0) G(n2, n2) += g;
    if (n1 >= 0 && n2 >= 0) {
        G(n1, n2) -= g;
        G(n2, n1) -= g;
    }
}

static void stampPairAC(Eigen::MatrixXcd& Y, int n1, int n2, std::complex<double> g)
{
    if (n1 >= 0) Y(n1, n1) += g;
    if (n2 >= 0) Y(n2, n2) += g;
    if (n1 >= 0 && n2 >= 0) {
        Y(n1, n2) -= g;
        Y(n2, n1) -= g;
    }
}

void Switch4Way::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, int /*size*/) const
{
    double g = 1.0 / m_value;

    if (!m_crossed) {
        // Straight: pin0-pin2, pin1-pin3
        stampPair(G, m_pins[0].nodeId - 1, m_pins[2].nodeId - 1, g);
        stampPair(G, m_pins[1].nodeId - 1, m_pins[3].nodeId - 1, g);
    } else {
        // Crossed: pin0-pin3, pin1-pin2
        stampPair(G, m_pins[0].nodeId - 1, m_pins[3].nodeId - 1, g);
        stampPair(G, m_pins[1].nodeId - 1, m_pins[2].nodeId - 1, g);
    }
}

void Switch4Way::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                         double /*omega*/, int /*size*/) const
{
    std::complex<double> g(1.0 / m_value, 0.0);

    if (!m_crossed) {
        stampPairAC(Y, m_pins[0].nodeId - 1, m_pins[2].nodeId - 1, g);
        stampPairAC(Y, m_pins[1].nodeId - 1, m_pins[3].nodeId - 1, g);
    } else {
        stampPairAC(Y, m_pins[0].nodeId - 1, m_pins[3].nodeId - 1, g);
        stampPairAC(Y, m_pins[1].nodeId - 1, m_pins[2].nodeId - 1, g);
    }
}
