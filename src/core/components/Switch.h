#pragma once

#include "../Component.h"

class Switch : public Component
{
public:
    explicit Switch(double contactResistance = 0.01);

    bool isClosed() const { return m_closed; }
    void setClosed(bool closed) { m_closed = closed; }
    void toggle() { m_closed = !m_closed; }

    QString valueString() const override;
    QString valueUnit() const override { return QString::fromUtf8("\xCE\xA9"); } // Ω

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

private:
    bool m_closed = false;
};
