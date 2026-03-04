#pragma once

#include "../Component.h"

class NOTGate : public Component
{
public:
    explicit NOTGate(double thresholdVoltage = 2.5);

    QString valueString() const override;
    QString valueUnit() const override { return "V"; }

    double thresholdVoltage() const { return m_value; }
    void setThresholdVoltage(double vth) { m_value = vth; }

    static constexpr double R_on  = 1.0;
    static constexpr double R_off = 1.0e6;

    enum State { OutputHigh, OutputLow };

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

    void stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, State state) const;
};
