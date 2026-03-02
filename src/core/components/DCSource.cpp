#include "DCSource.h"

DCSource::DCSource(double voltage)
    : Component(ComponentType::DCSource, 2, voltage)
{
}

QString DCSource::valueString() const
{
    return formatEngineering(m_value, valueUnit());
}

void DCSource::stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int /*size*/) const
{
    // MNA extension for voltage source:
    // Extra row/column for branch current variable
    int n_pos = m_pins[0].nodeId - 1; // positive terminal
    int n_neg = m_pins[1].nodeId - 1; // negative terminal
    int bi = m_branchIndex;

    if (bi < 0) return; // branch index not assigned

    if (n_pos >= 0) {
        G(n_pos, bi) += 1.0;
        G(bi, n_pos) += 1.0;
    }
    if (n_neg >= 0) {
        G(n_neg, bi) -= 1.0;
        G(bi, n_neg) -= 1.0;
    }

    rhs(bi) = m_value;
}

void DCSource::stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
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

    rhs(bi) = std::complex<double>(0.0, 0.0); // 0V in AC (superposition)
}
