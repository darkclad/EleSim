#pragma once

#include "../Component.h"

class NMosfet : public Component
{
public:
    explicit NMosfet(double thresholdVoltage = 2.0);

    QString valueString() const override;
    QString valueUnit() const override { return "V"; }

    double thresholdVoltage() const { return m_value; }
    void setThresholdVoltage(double vth) { m_value = vth; }

    // Channel resistances
    static constexpr double R_on  = 1.0;    // 1Ω on-resistance
    static constexpr double R_off = 1.0e6;  // 1MΩ off-resistance

    enum State { Cutoff, On };

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

    // Stamp MOSFET with known state for iterative DC / transient analysis
    void stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, State state) const;
};
