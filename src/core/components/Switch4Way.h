#pragma once

#include "../Component.h"

// Four-way switch (DPDT crossover):
// Straight: pin0-pin2 and pin1-pin3 connected
// Crossed:  pin0-pin3 and pin1-pin2 connected
class Switch4Way : public Component
{
public:
    explicit Switch4Way(double contactResistance = 0.01);

    bool isCrossed() const { return m_crossed; }
    void setCrossed(bool crossed) { m_crossed = crossed; }
    void toggle() { m_crossed = !m_crossed; }

    QString valueString() const override;
    QString valueUnit() const override { return QString::fromUtf8("\xCE\xA9"); } // Ω

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

private:
    bool m_crossed = false;
};
