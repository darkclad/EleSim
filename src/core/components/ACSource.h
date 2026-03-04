#pragma once

#include "Source.h"

class ACSource : public Source
{
public:
    explicit ACSource(double amplitude = 5.0, double frequency = 50.0, double phase = 0.0);

    QString valueString() const override;
    QString valueUnit() const override { return "V"; }

    bool isVoltageSource() const override { return true; }

    double frequency() const { return m_frequency; }
    void setFrequency(double hz) { m_frequency = hz; }

    double phase() const { return m_phase; }
    void setPhase(double degrees) { m_phase = degrees; }

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

private:
    double m_frequency; // Hz
    double m_phase;     // degrees
};
