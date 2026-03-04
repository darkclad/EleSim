#pragma once

#include "SimulationResult.h"
#include "SimulationConfig.h"
#include "TransientState.h"
#include <atomic>
#include <vector>

class Circuit;
class SharedFrameBuffer;

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
    static SimulationResult solveDC(Circuit& circuit, std::atomic<bool>* cancelled = nullptr);

    // Run AC analysis at a given frequency
    static SimulationResult solveAC(Circuit& circuit, double frequency, std::atomic<bool>* cancelled = nullptr);

    // Run transient analysis (returns final-state results)
    static SimulationResult solveTransient(Circuit& circuit, double timeStep, double totalTime,
                                           std::atomic<bool>* cancelled = nullptr);

    // Run transient analysis returning all frames (for oscilloscope)
    static TransientResult solveTransientFull(Circuit& circuit, double timeStep, double totalTime,
                                              std::atomic<bool>* cancelled = nullptr);

    // Run mixed DC+AC analysis (superposition)
    static SimulationResult solveMixed(Circuit& circuit, double acFrequency, std::atomic<bool>* cancelled = nullptr);

    // Extract SimulationResult from the last frame of a TransientResult
    static SimulationResult resultFromLastFrame(const TransientResult& tResult, Circuit& circuit);

    // Streaming transient with continuation support.
    // If initialState is valid and params match, resumes from that state.
    // Frames are pushed to the SharedFrameBuffer as they are computed.
    static void solveTransientStreaming(
        Circuit& circuit, double timeStep, double totalTime,
        const TransientState& initialState,
        SharedFrameBuffer& buffer,
        std::atomic<bool>* cancelled = nullptr);
};
