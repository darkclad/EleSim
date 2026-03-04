#include "ACSource.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ACSource::ACSource(double amplitude, double frequency, double phase)
    : Source(ComponentType::ACSource, 2, amplitude)
    , m_frequency(frequency)
    , m_phase(phase)
{
}

QString ACSource::valueString() const
{
    return formatEngineering(m_value, "V") + " " +
           formatEngineering(m_frequency, "Hz");
}

void ACSource::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int /*size*/) const
{
    // In DC analysis, AC source is a 0V source (short circuit)
    int n_pos = m_pins[0].nodeId - 1;
    int n_neg = m_pins[1].nodeId - 1;
    int bi = m_branchIndex;

    if (bi < 0) return;

    if (n_pos >= 0) {
        G(n_pos, bi) += 1.0;
        G(bi, n_pos) += 1.0;
    }
    if (n_neg >= 0) {
        G(n_neg, bi) -= 1.0;
        G(bi, n_neg) -= 1.0;
    }

    rhs(bi) = 0.0; // 0V in DC
}

void ACSource::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                       double /*omega*/, int /*size*/) const
{
    int n_pos = m_pins[0].nodeId - 1;
    int n_neg = m_pins[1].nodeId - 1;
    int bi = m_branchIndex;

    if (bi < 0) return;

    if (n_pos >= 0) {
        Y(n_pos, bi) += 1.0;
        Y(bi, n_pos) += 1.0;
    }
    if (n_neg >= 0) {
        Y(n_neg, bi) -= 1.0;
        Y(bi, n_neg) -= 1.0;
    }

    double phaseRad = m_phase * M_PI / 180.0;
    rhs(bi) = std::polar(m_value, phaseRad);
}
