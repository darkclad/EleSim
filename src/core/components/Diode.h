#pragma once

#include "../Component.h"

class Diode : public Component
{
public:
    explicit Diode(double forwardVoltage = 0.7);

    QString valueString() const override;
    QString valueUnit() const override { return "V"; }

    double forwardVoltage() const { return m_value; }

    // Diode resistances
    static constexpr double R_on  = 1.0;    // 1Ω forward resistance
    static constexpr double R_off = 1.0e6;  // 1MΩ reverse resistance

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

    // Stamp diode with known state for transient/iterative DC analysis
    // forward=true: low resistance + Vf offset; forward=false: high resistance
    void stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, bool forward) const;
};
