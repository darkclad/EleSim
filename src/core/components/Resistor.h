#pragma once

#include "../Component.h"

class Resistor : public Component
{
public:
    explicit Resistor(double resistance = 1000.0);

    QString valueString() const override;
    QString valueUnit() const override { return QString::fromUtf8("\xCE\xA9"); } // Ω

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;
};
