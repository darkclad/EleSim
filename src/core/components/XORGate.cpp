#include "XORGate.h"
#include <complex>

XORGate::XORGate(double thresholdVoltage)
    : Component(ComponentType::XORGate, 5, thresholdVoltage)
{
}

QString XORGate::valueString() const
{
    return QString::number(m_value, 'g', 3) + " " + valueUnit();
}

void XORGate::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int /*size*/) const
{
    stampWithState(G, rhs, OutputHigh);
}

void XORGate::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                       double /*omega*/, int /*size*/) const
{
    int nY   = m_pins[2].nodeId - 1;
    int nVDD = m_pins[3].nodeId - 1;
    int nGND = m_pins[4].nodeId - 1;

    std::complex<double> g_pu(1.0 / R_on, 0.0);
    std::complex<double> g_pd(1.0 / R_off, 0.0);

    if (nVDD >= 0) Y(nVDD, nVDD) += g_pu;
    if (nY >= 0)   Y(nY, nY)     += g_pu;
    if (nVDD >= 0 && nY >= 0) {
        Y(nVDD, nY) -= g_pu;
        Y(nY, nVDD) -= g_pu;
    }

    if (nY >= 0)   Y(nY, nY)     += g_pd;
    if (nGND >= 0) Y(nGND, nGND) += g_pd;
    if (nY >= 0 && nGND >= 0) {
        Y(nY, nGND) -= g_pd;
        Y(nGND, nY) -= g_pd;
    }
}

void XORGate::stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, State state) const
{
    int nY   = m_pins[2].nodeId - 1;
    int nVDD = m_pins[3].nodeId - 1;
    int nGND = m_pins[4].nodeId - 1;

    double g_pu = (state == OutputHigh) ? (1.0 / R_on)  : (1.0 / R_off);
    double g_pd = (state == OutputHigh) ? (1.0 / R_off) : (1.0 / R_on);

    if (nVDD >= 0) G(nVDD, nVDD) += g_pu;
    if (nY >= 0)   G(nY, nY)     += g_pu;
    if (nVDD >= 0 && nY >= 0) {
        G(nVDD, nY) -= g_pu;
        G(nY, nVDD) -= g_pu;
    }

    if (nY >= 0)   G(nY, nY)     += g_pd;
    if (nGND >= 0) G(nGND, nGND) += g_pd;
    if (nY >= 0 && nGND >= 0) {
        G(nY, nGND) -= g_pd;
        G(nGND, nY) -= g_pd;
    }
}
