#pragma once

#include <map>
#include <QString>

struct NodeResult {
    int nodeId = -1;
    double voltage = 0.0;
};

struct BranchResult {
    int componentId = -1;
    double current = 0.0;
    double voltageDrop = 0.0;
    double power = 0.0;

    // Mixed (superposition) analysis: separate DC and AC contributions
    double dcVoltage = 0.0;
    double acVoltage = 0.0;
    double dcCurrent = 0.0;
    double acCurrent = 0.0;
    bool isMixed = false;
};

class SimulationResult
{
public:
    bool success = false;
    QString errorMessage;

    std::map<int, NodeResult> nodeResults;       // nodeId -> result
    std::map<int, BranchResult> branchResults;   // componentId -> result
};
