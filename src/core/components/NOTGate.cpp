#include "NOTGate.h"
#include <complex>

NOTGate::NOTGate(double thresholdVoltage)
    : Component(ComponentType::NOTGate, 4, thresholdVoltage)
{
}

QString NOTGate::valueString() const
{
    return QString::number(m_value, 'g', 3) + " " + valueUnit();
}

void NOTGate::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int /*size*/) const
{
    stampWithState(G, rhs, OutputHigh);
}

void NOTGate::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& /*rhs*/,
                       double /*omega*/, int /*size*/) const
{
    // Pin 0 = A (no stamp), Pin 1 = Y, Pin 2 = VDD, Pin 3 = GND
    // AC: stamp on-state pull-up (VDD-Y) and off-state pull-down (Y-GND)
    int nY   = m_pins[1].nodeId - 1;
    int nVDD = m_pins[2].nodeId - 1;
    int nGND = m_pins[3].nodeId - 1;

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

void NOTGate::stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& /*rhs*/, State state) const
{
    // Pin 0 = A (infinite impedance, no stamp)
    // Pin 1 = Y (output), Pin 2 = VDD, Pin 3 = GND
    int nY   = m_pins[1].nodeId - 1;
    int nVDD = m_pins[2].nodeId - 1;
    int nGND = m_pins[3].nodeId - 1;

    double g_pu = (state == OutputHigh) ? (1.0 / R_on)  : (1.0 / R_off);
    double g_pd = (state == OutputHigh) ? (1.0 / R_off) : (1.0 / R_on);

    // Pull-up: VDD to Y
    if (nVDD >= 0) G(nVDD, nVDD) += g_pu;
    if (nY >= 0)   G(nY, nY)     += g_pu;
    if (nVDD >= 0 && nY >= 0) {
        G(nVDD, nY) -= g_pu;
        G(nY, nVDD) -= g_pu;
    }

    // Pull-down: Y to GND
    if (nY >= 0)   G(nY, nY)     += g_pd;
    if (nGND >= 0) G(nGND, nGND) += g_pd;
    if (nY >= 0 && nGND >= 0) {
        G(nY, nGND) -= g_pd;
        G(nGND, nY) -= g_pd;
    }
}
