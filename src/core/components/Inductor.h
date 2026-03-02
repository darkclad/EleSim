#pragma once

#include "../Component.h"

class Inductor : public Component
{
public:
    explicit Inductor(double inductance = 10e-3);

    QString valueString() const override;
    QString valueUnit() const override { return "H"; }

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;
};
