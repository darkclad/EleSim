#pragma once

#include <Eigen/Dense>
#include <map>

struct TransientState {
    int completedSteps = 0;
    double currentTime = 0.0;

    // Previous step's solution vector (node voltages + branch currents)
    Eigen::VectorXd xPrev;

    // Accumulated inductor currents (componentId -> current)
    std::map<int, double> inductorCurrents;

    // Per-step capacitor currents (componentId -> current)
    std::map<int, double> capacitorCurrents;

    // MNA setup parameters (must match for valid continuation)
    int numNodes = 0;
    int matSize = 0;
    double timeStep = 0.0;

    bool isValid() const { return matSize > 0 && xPrev.size() == matSize; }

    void reset() {
        completedSteps = 0;
        currentTime = 0.0;
        xPrev = Eigen::VectorXd();
        inductorCurrents.clear();
        capacitorCurrents.clear();
        numNodes = 0;
        matSize = 0;
        timeStep = 0.0;
    }
};
