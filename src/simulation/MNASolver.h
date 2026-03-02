#pragma once

#include "SimulationResult.h"
#include "SimulationConfig.h"
#include <vector>

class Circuit;

struct TransientFrame {
    double time = 0.0;
    std::map<int, double> nodeVoltages;      // nodeId -> voltage at this time
    std::map<int, double> branchCurrents;    // componentId -> current at this time
    std::map<int, double> componentVoltages; // componentId -> voltage drop at this time
};

struct TransientResult {
    bool success = false;
    QString errorMessage;
    std::vector<TransientFrame> frames;
};

class MNASolver
{
public:
    // Run DC analysis
    static SimulationResult solveDC(Circuit& circuit);

    // Run AC analysis at a given frequency
    static SimulationResult solveAC(Circuit& circuit, double frequency);

    // Run transient analysis (returns final-state results)
    static SimulationResult solveTransient(Circuit& circuit, double timeStep, double totalTime);

    // Run transient analysis returning all frames (for oscilloscope)
    static TransientResult solveTransientFull(Circuit& circuit, double timeStep, double totalTime);

    // Run mixed DC+AC analysis (superposition)
    static SimulationResult solveMixed(Circuit& circuit, double acFrequency);
};
