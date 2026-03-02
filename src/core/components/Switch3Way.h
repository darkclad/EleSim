#pragma once

#include "../Component.h"

// Three-way switch (SPDT): common pin0 connects to either pin1 or pin2
class Switch3Way : public Component
{
public:
    explicit Switch3Way(double contactResistance = 0.01);

    // 0 = connected to pin1 (position A), 1 = connected to pin2 (position B)
    int activePosition() const { return m_position; }
    void setActivePosition(int pos) { m_position = (pos == 0) ? 0 : 1; }
    void toggle() { m_position = (m_position == 0) ? 1 : 0; }

    QString valueString() const override;
    QString valueUnit() const override { return QString::fromUtf8("\xCE\xA9"); } // Ω

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

private:
    int m_position = 0; // 0 = pin1, 1 = pin2
};
