#pragma once

#include "../Component.h"

class Lamp : public Component
{
public:
    explicit Lamp(double resistance = 100.0);

    QString valueString() const override;
    QString valueUnit() const override { return QString::fromUtf8("\xCE\xA9"); } // Ω

    double currentThreshold() const { return m_currentThreshold; }
    void setCurrentThreshold(double amps) { m_currentThreshold = amps; }
    bool isOn(double current) const { return std::abs(current) >= m_currentThreshold; }

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

private:
    double m_currentThreshold = 0.01; // 10mA default
};
