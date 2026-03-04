#pragma once

#include "Source.h"

class DCSource : public Source
{
public:
    explicit DCSource(double voltage = 5.0);

    QString valueString() const override;
    QString valueUnit() const override { return "V"; }

    bool isVoltageSource() const override { return true; }

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;
};
