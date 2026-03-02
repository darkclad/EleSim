#pragma once

#include "../Component.h"

class DCCurrentSource : public Component
{
public:
    explicit DCCurrentSource(double current = 0.01);

    QString valueString() const override;
    QString valueUnit() const override { return "A"; }

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;
};
