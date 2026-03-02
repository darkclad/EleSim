#pragma once

enum class AnalysisType {
    DC,
    AC,
    Transient,
    Mixed       // Superposition: DC + AC combined
};

struct SimulationConfig {
    AnalysisType type = AnalysisType::DC;

    // AC / Mixed analysis
    double acFrequency = 50.0; // Hz

    // Simulation (transient) time parameters
    double timeStep = 0.001;   // seconds
    double totalTime = 0.1;    // seconds
};
