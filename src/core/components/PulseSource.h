#pragma once

#include "../Component.h"

class PulseSource : public Component
{
public:
    explicit PulseSource(double amplitude = 5.0, double frequency = 1000.0,
                         double phase = 0.0, double dutyCycle = 0.5);

    QString valueString() const override;
    QString valueUnit() const override { return "V"; }

    bool isVoltageSource() const override { return true; }

    double frequency() const { return m_frequency; }
    void setFrequency(double hz) { m_frequency = hz; }

    double phase() const { return m_phase; }
    void setPhase(double degrees) { m_phase = degrees; }

    double dutyCycle() const { return m_dutyCycle; }
    void setDutyCycle(double dc) { m_dutyCycle = qBound(0.01, dc, 0.99); }

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
    void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                 double omega, int size) const override;

private:
    double m_frequency; // Hz
    double m_phase;     // degrees
    double m_dutyCycle;  // 0.0–1.0 (fraction of period at +V)
};
