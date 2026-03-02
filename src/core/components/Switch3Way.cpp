#include "Switch3Way.h"

Switch3Way::Switch3Way(double contactResistance)
    : Component(ComponentType::Switch3Way, 3, contactResistance)
{
}

QString Switch3Way::valueString() const
{
    return formatEngineering(m_value, valueUnit());
}

void Switch3Way::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, int /*size*/) const
{
    // Common = pin0, connect to pin1 (pos A) or pin2 (pos B)
    int nCommon = m_pins[0].nodeId - 1;
    int nActive = m_pins[m_position + 1].nodeId - 1;
    double g = 1.0 / m_value;

    if (nCommon >= 0) G(nCommon, nCommon) += g;
    if (nActive >= 0) G(nActive, nActive) += g;
    if (nCommon >= 0 && nActive >= 0) {
        G(nCommon, nActive) -= g;
        G(nActive, nCommon) -= g;
    }
}

void Switch3Way::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                         double /*omega*/, int /*size*/) const
{
    int nCommon = m_pins[0].nodeId - 1;
    int nActive = m_pins[m_position + 1].nodeId - 1;
    std::complex<double> g(1.0 / m_value, 0.0);

    if (nCommon >= 0) Y(nCommon, nCommon) += g;
    if (nActive >= 0) Y(nActive, nActive) += g;
    if (nCommon >= 0 && nActive >= 0) {
        Y(nCommon, nActive) -= g;
        Y(nActive, nCommon) -= g;
    }
}
