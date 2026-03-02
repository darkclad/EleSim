#pragma once

#include "../Component.h"

class ZenerDiode : public Component
{
public:
    explicit ZenerDiode(double forwardVoltage = 0.7, double zenerVoltage = 5.1);

    QString valueString() const override;
    QString valueUnit() const override { return "V"; }

    double forwardVoltage() const { return m_value; }
    double zenerVoltage() const { return m_zenerVoltage; }
    void setZenerVoltage(double vz) { m_zenerVoltage = vz; }

    // Resistances (same as regular Diode)
    static constexpr double R_on  = 1.0;    // 1Ω forward/breakdown resistance
    static constexpr double R_off = 1.0e6;  // 1MΩ blocking resistance

    enum State { Forward, Blocking, Breakdown };

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

    void stampWithState(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, State state) const;

private:
    double m_zenerVoltage;
};
