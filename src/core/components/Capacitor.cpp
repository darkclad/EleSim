#include "Capacitor.h"
#include <complex>

Capacitor::Capacitor(double capacitance)
    : Component(ComponentType::Capacitor, 2, capacitance)
{
}

QString Capacitor::valueString() const
{
    return formatEngineering(m_value, valueUnit());
}

void Capacitor::stampDC(Eigen::MatrixXd& /*G*/, Eigen::VectorXd& /*rhs*/, int /*size*/) const
{
    // Capacitor is an open circuit in DC analysis - no stamp
}

void Capacitor::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                        double omega, int /*size*/) const
{
    int n1 = m_pins[0].nodeId - 1;
    int n2 = m_pins[1].nodeId - 1;
    std::complex<double> y(0.0, omega * m_value); // jωC

    if (n1 >= 0) Y(n1, n1) += y;
    if (n2 >= 0) Y(n2, n2) += y;
    if (n1 >= 0 && n2 >= 0) {
        Y(n1, n2) -= y;
        Y(n2, n1) -= y;
    }
}
