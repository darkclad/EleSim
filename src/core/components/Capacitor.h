#pragma once

#include "../Component.h"

class Capacitor : public Component
{
public:
    explicit Capacitor(double capacitance = 100e-6);

    QString valueString() const override;
    QString valueUnit() const override { return "F"; }

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;
};
