#include "DCCurrentSource.h"

DCCurrentSource::DCCurrentSource(double current)
    : Source(ComponentType::DCCurrentSource, 2, current)
{
}

QString DCCurrentSource::valueString() const
{
    return formatEngineering(m_value, valueUnit());
}

void DCCurrentSource::stampDC(Eigen::MatrixXd& /*G*/, Eigen::VectorXd& rhs, int /*size*/) const
{
    // Current source injects current directly into the RHS vector.
    // Arrow points from pin1 (negative) to pin0 (positive) inside the source.
    // Externally, current exits pin0 and enters pin1.
    // MNA convention: rhs(k) = current injected INTO node k.
    int n_pos = m_pins[0].nodeId - 1;
    int n_neg = m_pins[1].nodeId - 1;

    if (n_pos >= 0) rhs(n_pos) += m_value;
    if (n_neg >= 0) rhs(n_neg) -= m_value;
}

void DCCurrentSource::stampAC(Eigen::MatrixXcd& /*Y*/, Eigen::VectorXcd& /*rhs*/,
                               double /*omega*/, int /*size*/) const
{
    // DC current source contributes nothing to AC small-signal analysis
}
