#include "MNASolver.h"
#include "SharedFrameBuffer.h"
#include "../core/Circuit.h"
#include "../core/Component.h"
#include "../core/Node.h"
#include "../core/components/ACSource.h"
#include "../core/components/PulseSource.h"
#include "../core/components/DCCurrentSource.h"
#include "../core/components/Diode.h"
#include "../core/components/ZenerDiode.h"
#include "../core/components/NMosfet.h"
#include "../core/components/PMosfet.h"
#include "../core/components/NOTGate.h"
#include "../core/components/ANDGate.h"
#include "../core/components/ORGate.h"
#include "../core/components/XORGate.h"
#include "../core/components/Switch3Way.h"
#include "../core/components/Switch4Way.h"

#include <Eigen/Dense>
#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: check if a component has the required pins connected
static bool canStamp(const Component* comp)
{
    // 3-way switch: only common pin + active throw need to be connected
    if (comp->type() == ComponentType::Switch3Way) {
        auto* sw = static_cast<const Switch3Way*>(comp);
        return comp->pin(0).nodeId >= 0
            && comp->pin(sw->activePosition() + 1).nodeId >= 0;
    }
    // All other components: all pins must be connected
    for (int i = 0; i < comp->pinCount(); ++i) {
        if (comp->pin(i).nodeId < 0) return false;
    }
    return true;
}

// Helper: check if a component is a logic gate
static bool isLogicGate(ComponentType t)
{
    return t == ComponentType::NOTGate || t == ComponentType::ANDGate ||
           t == ComponentType::ORGate  || t == ComponentType::XORGate;
}

// Helper: evaluate logic gate truth table from input voltages
// Returns true if output should be HIGH
static bool evaluateGate(const Component* comp, double vA, double vGND)
{
    double vth = comp->value();
    bool aHigh = (vA - vGND) >= vth;

    if (comp->type() == ComponentType::NOTGate) {
        return !aHigh;
    }
    // 2-input gates: caller must pass vB via the vGND trick — use separate overload
    return false; // should not reach here for 2-input gates
}

static bool evaluateGate2(const Component* comp, double vA, double vB, double vGND)
{
    double vth = comp->value();
    bool aHigh = (vA - vGND) >= vth;
    bool bHigh = (vB - vGND) >= vth;

    if (comp->type() == ComponentType::ANDGate)  return aHigh && bHigh;
    if (comp->type() == ComponentType::ORGate)   return aHigh || bHigh;
    if (comp->type() == ComponentType::XORGate)  return aHigh != bHigh;
    return false;
}

// Helper: get the two active pin indices for current calculation
static void getActivePins(const Component* comp, int& pin1, int& pin2)
{
    pin1 = 0;
    pin2 = 1;
    if (comp->type() == ComponentType::Switch3Way) {
        auto* sw = static_cast<const Switch3Way*>(comp);
        pin2 = sw->activePosition() + 1; // pin 1 or 2
    } else if (comp->type() == ComponentType::Switch4Way) {
        auto* sw = static_cast<const Switch4Way*>(comp);
        // Report current for first pair
        pin2 = sw->isCrossed() ? 3 : 2;
    } else if (comp->type() == ComponentType::NOTGate) {
        pin1 = 2; pin2 = 3; // VDD, GND (supply current)
    } else if (isLogicGate(comp->type())) {
        pin1 = 3; pin2 = 4; // VDD, GND for 2-input gates
    }
}

// Helper: common setup for MNA (build netlist, assign branch indices)
struct MNASetup {
    int numNodes = 0;
    int numVS = 0;
    int matSize = 0;
    bool ok = false;
    QString error;
};

static MNASetup prepareMNA(Circuit& circuit)
{
    MNASetup s;
    QString errorMsg;
    if (!circuit.isValid(errorMsg)) {
        s.error = errorMsg;
        return s;
    }

    s.numNodes = circuit.buildNetlist();
    if (s.numNodes <= 0) {
        s.error = "No valid nodes found in circuit";
        return s;
    }

    s.numVS = 0;
    for (auto& [id, comp] : circuit.components()) {
        if (comp->isVoltageSource()) {
            comp->setBranchIndex(s.numNodes + s.numVS);
            ++s.numVS;
        }
    }

    s.matSize = s.numNodes + s.numVS;
    s.ok = true;
    return s;
}

// --- DC Analysis ---

SimulationResult MNASolver::solveDC(Circuit& circuit, std::atomic<bool>* cancelled)
{
    SimulationResult result;

    auto setup = prepareMNA(circuit);
    if (!setup.ok) {
        result.errorMessage = setup.error;
        return result;
    }

    int numNodes = setup.numNodes;
    int matSize = setup.matSize;

    // Collect diodes for iterative state resolution
    std::map<int, bool> diodeForward; // componentId -> forward?
    for (auto& [id, comp] : circuit.components()) {
        if (comp->type() == ComponentType::Diode)
            diodeForward[id] = true; // initial guess: all forward
    }

    // Collect zener diodes for iterative state resolution
    std::map<int, ZenerDiode::State> zenerState;
    for (auto& [id, comp] : circuit.components()) {
        if (comp->type() == ComponentType::ZenerDiode)
            zenerState[id] = ZenerDiode::Forward; // initial guess
    }

    // Collect MOSFETs for iterative state resolution
    std::map<int, bool> mosfetOn; // componentId -> on?
    for (auto& [id, comp] : circuit.components()) {
        if (comp->type() == ComponentType::NMosfet ||
            comp->type() == ComponentType::PMosfet)
            mosfetOn[id] = true; // initial guess: on
    }

    // Collect logic gates for iterative state resolution
    std::map<int, bool> gateOutputHigh; // componentId -> output high?
    for (auto& [id, comp] : circuit.components()) {
        if (isLogicGate(comp->type()))
            gateOutputHigh[id] = true; // initial guess: output high
    }

    Eigen::VectorXd x;

    // Iterative DC solve: resolve diode states until stable
    for (int iter = 0; iter < 30; ++iter) {
        if (cancelled && cancelled->load()) {
            result.errorMessage = "Cancelled";
            return result;
        }
        Eigen::MatrixXd A = Eigen::MatrixXd::Zero(matSize, matSize);
        Eigen::VectorXd b = Eigen::VectorXd::Zero(matSize);

        for (auto& [id, comp] : circuit.components()) {
            if (!canStamp(comp.get())) continue;
            if (comp->type() == ComponentType::Diode) {
                auto* diode = static_cast<Diode*>(comp.get());
                diode->stampWithState(A, b, diodeForward[id]);
            } else if (comp->type() == ComponentType::ZenerDiode) {
                auto* zd = static_cast<ZenerDiode*>(comp.get());
                zd->stampWithState(A, b, zenerState[id]);
            } else if (comp->type() == ComponentType::NMosfet) {
                auto* m = static_cast<NMosfet*>(comp.get());
                m->stampWithState(A, b, mosfetOn[id] ? NMosfet::On : NMosfet::Cutoff);
            } else if (comp->type() == ComponentType::PMosfet) {
                auto* m = static_cast<PMosfet*>(comp.get());
                m->stampWithState(A, b, mosfetOn[id] ? PMosfet::On : PMosfet::Cutoff);
            } else if (comp->type() == ComponentType::NOTGate) {
                auto* g = static_cast<NOTGate*>(comp.get());
                g->stampWithState(A, b, gateOutputHigh[id] ? NOTGate::OutputHigh : NOTGate::OutputLow);
            } else if (comp->type() == ComponentType::ANDGate) {
                auto* g = static_cast<ANDGate*>(comp.get());
                g->stampWithState(A, b, gateOutputHigh[id] ? ANDGate::OutputHigh : ANDGate::OutputLow);
            } else if (comp->type() == ComponentType::ORGate) {
                auto* g = static_cast<ORGate*>(comp.get());
                g->stampWithState(A, b, gateOutputHigh[id] ? ORGate::OutputHigh : ORGate::OutputLow);
            } else if (comp->type() == ComponentType::XORGate) {
                auto* g = static_cast<XORGate*>(comp.get());
                g->stampWithState(A, b, gateOutputHigh[id] ? XORGate::OutputHigh : XORGate::OutputLow);
            } else {
                comp->stampDC(A, b, matSize);
            }
        }

        Eigen::PartialPivLU<Eigen::MatrixXd> solver(A);
        double det = solver.determinant();
        if (std::abs(det) < 1e-15) {
            result.errorMessage = "Singular matrix - circuit may have:\n"
                                  "- Floating nodes (unconnected pins)\n"
                                  "- Voltage source loops\n"
                                  "- Short circuits";
            return result;
        }

        x = solver.solve(b);

        // Check and update diode states
        bool statesChanged = false;
        for (auto& [id, comp] : circuit.components()) {
            if (comp->type() == ComponentType::Diode) {
                int n1 = comp->pin(0).nodeId; // anode
                int n2 = comp->pin(1).nodeId; // cathode
                double v1 = (n1 > 0 && n1 <= numNodes) ? x(n1 - 1) : 0.0;
                double v2 = (n2 > 0 && n2 <= numNodes) ? x(n2 - 1) : 0.0;
                double vd = v1 - v2;
                bool shouldBeForward = (vd > comp->value()); // Vf threshold
                if (shouldBeForward != diodeForward[id]) {
                    diodeForward[id] = shouldBeForward;
                    statesChanged = true;
                }
            } else if (comp->type() == ComponentType::ZenerDiode) {
                auto* zd = static_cast<ZenerDiode*>(comp.get());
                int n1 = comp->pin(0).nodeId;
                int n2 = comp->pin(1).nodeId;
                double v1 = (n1 > 0 && n1 <= numNodes) ? x(n1 - 1) : 0.0;
                double v2 = (n2 > 0 && n2 <= numNodes) ? x(n2 - 1) : 0.0;
                double vd = v1 - v2;
                ZenerDiode::State newState;
                if (vd > zd->forwardVoltage())
                    newState = ZenerDiode::Forward;
                else if (vd < -zd->zenerVoltage())
                    newState = ZenerDiode::Breakdown;
                else
                    newState = ZenerDiode::Blocking;
                if (newState != zenerState[id]) {
                    zenerState[id] = newState;
                    statesChanged = true;
                }
            } else if (comp->type() == ComponentType::NMosfet) {
                // NMOS: Pin 0=Gate, Pin 1=Drain, Pin 2=Source
                int nG = comp->pin(0).nodeId;
                int nS = comp->pin(2).nodeId;
                double vG = (nG > 0 && nG <= numNodes) ? x(nG - 1) : 0.0;
                double vS = (nS > 0 && nS <= numNodes) ? x(nS - 1) : 0.0;
                double vgs = vG - vS;
                bool shouldBeOn = (vgs >= comp->value()); // Vth
                if (shouldBeOn != mosfetOn[id]) {
                    mosfetOn[id] = shouldBeOn;
                    statesChanged = true;
                }
            } else if (comp->type() == ComponentType::PMosfet) {
                // PMOS: Pin 0=Gate, Pin 1=Source, Pin 2=Drain
                int nG = comp->pin(0).nodeId;
                int nS = comp->pin(1).nodeId;
                double vG = (nG > 0 && nG <= numNodes) ? x(nG - 1) : 0.0;
                double vS = (nS > 0 && nS <= numNodes) ? x(nS - 1) : 0.0;
                double vsg = vS - vG;
                bool shouldBeOn = (vsg >= comp->value()); // Vth
                if (shouldBeOn != mosfetOn[id]) {
                    mosfetOn[id] = shouldBeOn;
                    statesChanged = true;
                }
            } else if (comp->type() == ComponentType::NOTGate) {
                int nA   = comp->pin(0).nodeId;
                int nGND = comp->pin(3).nodeId;
                double vA   = (nA > 0 && nA <= numNodes) ? x(nA - 1) : 0.0;
                double vGND = (nGND > 0 && nGND <= numNodes) ? x(nGND - 1) : 0.0;
                bool newOut = evaluateGate(comp.get(), vA, vGND);
                if (newOut != gateOutputHigh[id]) {
                    gateOutputHigh[id] = newOut;
                    statesChanged = true;
                }
            } else if (comp->type() == ComponentType::ANDGate ||
                       comp->type() == ComponentType::ORGate ||
                       comp->type() == ComponentType::XORGate) {
                int nA   = comp->pin(0).nodeId;
                int nB   = comp->pin(1).nodeId;
                int nGND = comp->pin(4).nodeId;
                double vA   = (nA > 0 && nA <= numNodes) ? x(nA - 1) : 0.0;
                double vB   = (nB > 0 && nB <= numNodes) ? x(nB - 1) : 0.0;
                double vGND = (nGND > 0 && nGND <= numNodes) ? x(nGND - 1) : 0.0;
                bool newOut = evaluateGate2(comp.get(), vA, vB, vGND);
                if (newOut != gateOutputHigh[id]) {
                    gateOutputHigh[id] = newOut;
                    statesChanged = true;
                }
            }
        }
        if (!statesChanged) break;
    }

    // Ground node
    NodeResult groundResult;
    groundResult.nodeId = 0;
    groundResult.voltage = 0.0;
    result.nodeResults[0] = groundResult;

    for (int i = 0; i < numNodes; ++i) {
        NodeResult nr;
        nr.nodeId = i + 1;
        nr.voltage = x(i);
        result.nodeResults[nr.nodeId] = nr;
    }

    for (auto& node : circuit.nodes()) {
        auto it = result.nodeResults.find(node->id());
        if (it != result.nodeResults.end())
            node->setVoltage(it->second.voltage);
    }

    int vsIndex = 0;
    for (auto& [id, comp] : circuit.components()) {
        if (comp->pinCount() < 2) continue; // Skip Ground, Junction, etc.

        BranchResult br;
        br.componentId = id;

        int p1, p2;
        getActivePins(comp.get(), p1, p2);
        int n1 = comp->pin(p1).nodeId;
        int n2 = comp->pin(p2).nodeId;
        double v1 = (n1 >= 0 && result.nodeResults.count(n1)) ? result.nodeResults[n1].voltage : 0.0;
        double v2 = (n2 >= 0 && result.nodeResults.count(n2)) ? result.nodeResults[n2].voltage : 0.0;
        br.voltageDrop = v1 - v2;

        if (comp->isVoltageSource()) {
            br.current = x(numNodes + vsIndex);
            ++vsIndex;
        } else if (comp->type() == ComponentType::Inductor) {
            br.current = br.voltageDrop / 0.001; // R_dc = 1mΩ
        } else if (comp->type() == ComponentType::Capacitor) {
            br.current = 0.0; // Open circuit in DC
        } else if (comp->type() == ComponentType::Diode) {
            double vd = br.voltageDrop;
            if (vd > comp->value()) {
                br.current = (vd - comp->value()) / Diode::R_on;
            } else if (vd > 0) {
                br.current = vd / Diode::R_off;
            } else {
                br.current = vd / Diode::R_off;
            }
        } else if (comp->type() == ComponentType::ZenerDiode) {
            auto* zd = static_cast<ZenerDiode*>(comp.get());
            double vd = br.voltageDrop;
            if (vd > zd->forwardVoltage()) {
                br.current = (vd - zd->forwardVoltage()) / ZenerDiode::R_on;
            } else if (vd < -zd->zenerVoltage()) {
                br.current = (vd + zd->zenerVoltage()) / ZenerDiode::R_on;
            } else {
                br.current = vd / ZenerDiode::R_off;
            }
        } else if (comp->type() == ComponentType::NMosfet) {
            // NMOS: current through D(pin1)-S(pin2) channel
            int nD = comp->pin(1).nodeId;
            int nS = comp->pin(2).nodeId;
            double vD = (nD >= 0 && result.nodeResults.count(nD)) ? result.nodeResults[nD].voltage : 0.0;
            double vS = (nS >= 0 && result.nodeResults.count(nS)) ? result.nodeResults[nS].voltage : 0.0;
            double vds = vD - vS;
            br.voltageDrop = vds;
            double R = mosfetOn[id] ? NMosfet::R_on : NMosfet::R_off;
            br.current = vds / R;
        } else if (comp->type() == ComponentType::PMosfet) {
            // PMOS: current through S(pin1)-D(pin2) channel
            int nS = comp->pin(1).nodeId;
            int nD = comp->pin(2).nodeId;
            double vS = (nS >= 0 && result.nodeResults.count(nS)) ? result.nodeResults[nS].voltage : 0.0;
            double vD = (nD >= 0 && result.nodeResults.count(nD)) ? result.nodeResults[nD].voltage : 0.0;
            double vsd = vS - vD;
            br.voltageDrop = vsd;
            double R = mosfetOn[id] ? PMosfet::R_on : PMosfet::R_off;
            br.current = vsd / R;
        } else if (isLogicGate(comp->type())) {
            int vddPin = (comp->type() == ComponentType::NOTGate) ? 2 : 3;
            int gndPin = (comp->type() == ComponentType::NOTGate) ? 3 : 4;
            int nVDD = comp->pin(vddPin).nodeId;
            int nGND = comp->pin(gndPin).nodeId;
            double vVDD = (nVDD >= 0 && result.nodeResults.count(nVDD)) ? result.nodeResults[nVDD].voltage : 0.0;
            double vGND_v = (nGND >= 0 && result.nodeResults.count(nGND)) ? result.nodeResults[nGND].voltage : 0.0;
            br.voltageDrop = vVDD - vGND_v;
            double R_total = NOTGate::R_on + NOTGate::R_off;
            br.current = (vVDD - vGND_v) / R_total;
        } else if (comp->type() == ComponentType::DCCurrentSource) {
            br.current = comp->value();
        } else if (comp->value() > 0.0) {
            br.current = br.voltageDrop / comp->value();
        }

        br.power = std::abs(br.voltageDrop * br.current);
        result.branchResults[id] = br;
    }

    result.success = true;
    return result;
}

// --- AC Analysis ---

SimulationResult MNASolver::solveAC(Circuit& circuit, double frequency, std::atomic<bool>* cancelled)
{
    SimulationResult result;

    auto setup = prepareMNA(circuit);
    if (!setup.ok) {
        result.errorMessage = setup.error;
        return result;
    }

    int numNodes = setup.numNodes;
    int matSize = setup.matSize;
    double omega = 2.0 * M_PI * frequency;

    Eigen::MatrixXcd Y = Eigen::MatrixXcd::Zero(matSize, matSize);
    Eigen::VectorXcd b = Eigen::VectorXcd::Zero(matSize);

    for (auto& [id, comp] : circuit.components()) {
        if (!canStamp(comp.get())) continue;
        comp->stampAC(Y, b, omega, matSize);
    }

    Eigen::PartialPivLU<Eigen::MatrixXcd> solver(Y);
    auto det = solver.determinant();
    if (std::abs(det) < 1e-15) {
        result.errorMessage = "Singular AC matrix";
        return result;
    }

    Eigen::VectorXcd x = solver.solve(b);

    // Extract magnitudes as the "voltage" values
    NodeResult groundResult;
    groundResult.nodeId = 0;
    groundResult.voltage = 0.0;
    result.nodeResults[0] = groundResult;

    for (int i = 0; i < numNodes; ++i) {
        NodeResult nr;
        nr.nodeId = i + 1;
        nr.voltage = std::abs(x(i)); // magnitude
        result.nodeResults[nr.nodeId] = nr;
    }

    int vsIndex = 0;
    for (auto& [id, comp] : circuit.components()) {
        if (comp->pinCount() < 2) continue; // Skip Ground, Junction, etc.

        BranchResult br;
        br.componentId = id;

        int p1, p2;
        getActivePins(comp.get(), p1, p2);
        int n1 = comp->pin(p1).nodeId;
        int n2 = comp->pin(p2).nodeId;
        std::complex<double> v1 = (n1 > 0 && n1 <= numNodes) ? x(n1 - 1) : std::complex<double>(0.0);
        std::complex<double> v2 = (n2 > 0 && n2 <= numNodes) ? x(n2 - 1) : std::complex<double>(0.0);
        std::complex<double> vDrop = v1 - v2;
        br.voltageDrop = std::abs(vDrop);

        if (comp->isVoltageSource()) {
            br.current = std::abs(x(numNodes + vsIndex));
            ++vsIndex;
        } else if (comp->value() > 0.0) {
            // For impedance-based components
            if (comp->type() == ComponentType::Capacitor) {
                std::complex<double> impedance(0.0, -1.0 / (omega * comp->value()));
                br.current = std::abs(vDrop / impedance);
            } else if (comp->type() == ComponentType::Inductor) {
                std::complex<double> impedance(0.0, omega * comp->value());
                br.current = std::abs(vDrop / impedance);
            } else if (comp->type() == ComponentType::Diode) {
                br.current = std::abs(vDrop) / Diode::R_on;
            } else if (comp->type() == ComponentType::ZenerDiode) {
                br.current = std::abs(vDrop) / ZenerDiode::R_on;
            } else if (comp->type() == ComponentType::NMosfet ||
                       comp->type() == ComponentType::PMosfet) {
                // AC small-signal: use on-resistance
                br.current = std::abs(vDrop) / NMosfet::R_on;
            } else if (isLogicGate(comp->type())) {
                br.current = std::abs(vDrop) / (NOTGate::R_on + NOTGate::R_off);
            } else {
                br.current = std::abs(vDrop) / comp->value();
            }
        }

        br.power = br.voltageDrop * br.current;
        result.branchResults[id] = br;
    }

    result.success = true;
    return result;
}

// --- Transient Analysis ---

SimulationResult MNASolver::solveTransient(Circuit& circuit, double timeStep, double totalTime,
                                           std::atomic<bool>* cancelled)
{
    // For transient: use backward Euler discretization
    // Capacitor C -> equivalent conductance G_eq = C/dt, current source I_eq = C/dt * V_prev

    TransientResult tResult;

    auto setup = prepareMNA(circuit);
    if (!setup.ok) {
        SimulationResult r;
        r.errorMessage = setup.error;
        return r;
    }

    int numNodes = setup.numNodes;
    int matSize = setup.matSize;
    int numSteps = static_cast<int>(totalTime / timeStep);

    // Previous node voltages (start at 0)
    Eigen::VectorXd xPrev = Eigen::VectorXd::Zero(matSize);

    // Track inductor currents (state variable that accumulates over time)
    std::map<int, double> inductorCurrents; // componentId -> current
    for (auto& [id, comp] : circuit.components()) {
        if (comp->type() == ComponentType::Inductor)
            inductorCurrents[id] = 0.0;
    }

    // Track capacitor currents (computed each step from voltage difference)
    std::map<int, double> capacitorCurrents;
    for (auto& [id, comp] : circuit.components()) {
        if (comp->type() == ComponentType::Capacitor)
            capacitorCurrents[id] = 0.0;
    }

    // We'll return the last frame as a SimulationResult for simplicity
    SimulationResult result;

    for (int step = 0; step <= numSteps; ++step) {
        if (cancelled && cancelled->load()) {
            result.errorMessage = "Cancelled";
            return result;
        }
        double t = step * timeStep;

        Eigen::MatrixXd A = Eigen::MatrixXd::Zero(matSize, matSize);
        Eigen::VectorXd b = Eigen::VectorXd::Zero(matSize);

        for (auto& [id, comp] : circuit.components()) {
            if (!canStamp(comp.get())) continue;

            if (comp->type() == ComponentType::Capacitor) {
                // Backward Euler: C/dt conductance + C/dt * V_prev current source
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double geq = comp->value() / timeStep;

                if (n1 >= 0) A(n1, n1) += geq;
                if (n2 >= 0) A(n2, n2) += geq;
                if (n1 >= 0 && n2 >= 0) {
                    A(n1, n2) -= geq;
                    A(n2, n1) -= geq;
                }

                // Current source from previous voltage
                double vPrev1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vPrev2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                double ieq = geq * (vPrev1 - vPrev2);

                if (n1 >= 0) b(n1) += ieq;
                if (n2 >= 0) b(n2) -= ieq;

            } else if (comp->type() == ComponentType::Inductor) {
                // Backward Euler: I_L(n) = I_L(n-1) + dt/L * V_L(n)
                // Norton equivalent: conductance dt/L + current source I_L(n-1)
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double geq = timeStep / comp->value(); // G_eq = dt/L

                if (n1 >= 0) A(n1, n1) += geq;
                if (n2 >= 0) A(n2, n2) += geq;
                if (n1 >= 0 && n2 >= 0) {
                    A(n1, n2) -= geq;
                    A(n2, n1) -= geq;
                }

                // History current source: accumulated inductor current from previous step
                double iPrev = inductorCurrents[id];
                if (n1 >= 0) b(n1) -= iPrev;
                if (n2 >= 0) b(n2) += iPrev;

            } else if (comp->type() == ComponentType::ACSource) {
                // Time-varying source: V(t) = Vamp * sin(2*pi*f*t + phase)
                auto* ac = static_cast<ACSource*>(comp.get());
                double v = ac->value() * std::sin(2.0 * M_PI * ac->frequency() * t
                                                    + ac->phase() * M_PI / 180.0);

                int n_pos = comp->pin(0).nodeId - 1;
                int n_neg = comp->pin(1).nodeId - 1;
                int bi = comp->branchIndex();
                if (bi < 0) continue;

                if (n_pos >= 0) {
                    A(n_pos, bi) += 1.0;
                    A(bi, n_pos) += 1.0;
                }
                if (n_neg >= 0) {
                    A(n_neg, bi) -= 1.0;
                    A(bi, n_neg) -= 1.0;
                }
                b(bi) = v;

            } else if (comp->type() == ComponentType::PulseSource) {
                auto* sq = static_cast<PulseSource*>(comp.get());
                double period = 1.0 / sq->frequency();
                double phaseOffset = sq->phase() / 360.0 * period;
                double tMod = std::fmod(t + phaseOffset, period);
                if (tMod < 0) tMod += period;
                double v = (tMod < period * sq->dutyCycle()) ? sq->value() : -sq->value();

                int n_pos = comp->pin(0).nodeId - 1;
                int n_neg = comp->pin(1).nodeId - 1;
                int bi = comp->branchIndex();
                if (bi < 0) continue;

                if (n_pos >= 0) {
                    A(n_pos, bi) += 1.0;
                    A(bi, n_pos) += 1.0;
                }
                if (n_neg >= 0) {
                    A(n_neg, bi) -= 1.0;
                    A(bi, n_neg) -= 1.0;
                }
                b(bi) = v;

            } else if (comp->type() == ComponentType::Diode) {
                // Piecewise-linear diode: state from previous voltage
                auto* diode = static_cast<Diode*>(comp.get());
                int n1 = comp->pin(0).nodeId - 1; // anode
                int n2 = comp->pin(1).nodeId - 1; // cathode
                double vPrev1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vPrev2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                bool forward = (vPrev1 - vPrev2) > comp->value(); // Vf threshold
                diode->stampWithState(A, b, forward);

            } else if (comp->type() == ComponentType::ZenerDiode) {
                auto* zd = static_cast<ZenerDiode*>(comp.get());
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double vPrev1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vPrev2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                double vd = vPrev1 - vPrev2;
                ZenerDiode::State state;
                if (vd > zd->forwardVoltage())
                    state = ZenerDiode::Forward;
                else if (vd < -zd->zenerVoltage())
                    state = ZenerDiode::Breakdown;
                else
                    state = ZenerDiode::Blocking;
                zd->stampWithState(A, b, state);

            } else if (comp->type() == ComponentType::NMosfet) {
                auto* m = static_cast<NMosfet*>(comp.get());
                int nG = comp->pin(0).nodeId - 1;
                int nS = comp->pin(2).nodeId - 1;
                double vG = (nG >= 0) ? xPrev(nG) : 0.0;
                double vS = (nS >= 0) ? xPrev(nS) : 0.0;
                bool on = (vG - vS) >= comp->value();
                m->stampWithState(A, b, on ? NMosfet::On : NMosfet::Cutoff);

            } else if (comp->type() == ComponentType::PMosfet) {
                auto* m = static_cast<PMosfet*>(comp.get());
                int nG = comp->pin(0).nodeId - 1;
                int nS = comp->pin(1).nodeId - 1;
                double vG = (nG >= 0) ? xPrev(nG) : 0.0;
                double vS = (nS >= 0) ? xPrev(nS) : 0.0;
                bool on = (vS - vG) >= comp->value();
                m->stampWithState(A, b, on ? PMosfet::On : PMosfet::Cutoff);

            } else if (comp->type() == ComponentType::NOTGate) {
                auto* g = static_cast<NOTGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nGND = comp->pin(3).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate(comp.get(), vA, vGND);
                g->stampWithState(A, b, outHigh ? NOTGate::OutputHigh : NOTGate::OutputLow);

            } else if (comp->type() == ComponentType::ANDGate) {
                auto* g = static_cast<ANDGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, b, outHigh ? ANDGate::OutputHigh : ANDGate::OutputLow);

            } else if (comp->type() == ComponentType::ORGate) {
                auto* g = static_cast<ORGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, b, outHigh ? ORGate::OutputHigh : ORGate::OutputLow);

            } else if (comp->type() == ComponentType::XORGate) {
                auto* g = static_cast<XORGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, b, outHigh ? XORGate::OutputHigh : XORGate::OutputLow);

            } else {
                // DC stamp for resistors, DC sources, lamps
                comp->stampDC(A, b, matSize);
            }
        }

        Eigen::PartialPivLU<Eigen::MatrixXd> solver(A);
        double det = solver.determinant();
        if (std::abs(det) < 1e-15) {
            result.errorMessage = "Singular matrix at t=" + QString::number(t, 'f', 4) + "s";
            return result;
        }

        Eigen::VectorXd x = solver.solve(b);

        // Update inductor currents: I_L(n) = I_L(n-1) + dt/L * V_L(n)
        for (auto& [id, comp] : circuit.components()) {
            if (comp->type() != ComponentType::Inductor) continue;
            if (!canStamp(comp.get())) continue;
            int n1 = comp->pin(0).nodeId - 1;
            int n2 = comp->pin(1).nodeId - 1;
            double v1 = (n1 >= 0) ? x(n1) : 0.0;
            double v2 = (n2 >= 0) ? x(n2) : 0.0;
            double vL = v1 - v2;
            inductorCurrents[id] += (timeStep / comp->value()) * vL;
        }

        // Update capacitor currents: I_C = C/dt * (V_n - V_n-1)
        for (auto& [id, comp] : circuit.components()) {
            if (comp->type() != ComponentType::Capacitor) continue;
            if (!canStamp(comp.get())) continue;
            int n1 = comp->pin(0).nodeId - 1;
            int n2 = comp->pin(1).nodeId - 1;
            double v1 = (n1 >= 0) ? x(n1) : 0.0;
            double v2 = (n2 >= 0) ? x(n2) : 0.0;
            double v1p = (n1 >= 0) ? xPrev(n1) : 0.0;
            double v2p = (n2 >= 0) ? xPrev(n2) : 0.0;
            capacitorCurrents[id] = comp->value() / timeStep * ((v1 - v2) - (v1p - v2p));
        }

        xPrev = x;
    }

    // Return last step results
    NodeResult groundResult;
    groundResult.nodeId = 0;
    groundResult.voltage = 0.0;
    result.nodeResults[0] = groundResult;

    for (int i = 0; i < numNodes; ++i) {
        NodeResult nr;
        nr.nodeId = i + 1;
        nr.voltage = xPrev(i);
        result.nodeResults[nr.nodeId] = nr;
    }

    for (auto& node : circuit.nodes()) {
        auto it = result.nodeResults.find(node->id());
        if (it != result.nodeResults.end())
            node->setVoltage(it->second.voltage);
    }

    int vsIndex = 0;
    for (auto& [id, comp] : circuit.components()) {
        if (comp->pinCount() < 2) continue; // Skip Ground, Junction, etc.

        BranchResult br;
        br.componentId = id;

        int p1, p2;
        getActivePins(comp.get(), p1, p2);
        int n1 = comp->pin(p1).nodeId;
        int n2 = comp->pin(p2).nodeId;
        double v1 = (n1 >= 0 && result.nodeResults.count(n1)) ? result.nodeResults[n1].voltage : 0.0;
        double v2 = (n2 >= 0 && result.nodeResults.count(n2)) ? result.nodeResults[n2].voltage : 0.0;
        br.voltageDrop = v1 - v2;

        if (comp->isVoltageSource()) {
            br.current = xPrev(numNodes + vsIndex);
            ++vsIndex;
        } else if (comp->type() == ComponentType::Inductor) {
            br.current = inductorCurrents[id];
        } else if (comp->type() == ComponentType::Capacitor) {
            br.current = capacitorCurrents[id];
        } else if (comp->type() == ComponentType::Diode) {
            double vd = br.voltageDrop;
            if (vd > comp->value())
                br.current = (vd - comp->value()) / Diode::R_on;
            else if (vd > 0)
                br.current = 0.0;
            else
                br.current = vd / Diode::R_off;
        } else if (comp->type() == ComponentType::ZenerDiode) {
            auto* zd = static_cast<ZenerDiode*>(comp.get());
            double vd = br.voltageDrop;
            if (vd > zd->forwardVoltage())
                br.current = (vd - zd->forwardVoltage()) / ZenerDiode::R_on;
            else if (vd < -zd->zenerVoltage())
                br.current = (vd + zd->zenerVoltage()) / ZenerDiode::R_on;
            else
                br.current = vd / ZenerDiode::R_off;
        } else if (comp->type() == ComponentType::NMosfet) {
            int nG = comp->pin(0).nodeId;
            int nS = comp->pin(2).nodeId;
            int nD = comp->pin(1).nodeId;
            double vG = (nG >= 0 && result.nodeResults.count(nG)) ? result.nodeResults[nG].voltage : 0.0;
            double vS = (nS >= 0 && result.nodeResults.count(nS)) ? result.nodeResults[nS].voltage : 0.0;
            double vD = (nD >= 0 && result.nodeResults.count(nD)) ? result.nodeResults[nD].voltage : 0.0;
            double vds = vD - vS;
            br.voltageDrop = vds;
            double R = ((vG - vS) >= comp->value()) ? NMosfet::R_on : NMosfet::R_off;
            br.current = vds / R;
        } else if (comp->type() == ComponentType::PMosfet) {
            int nG = comp->pin(0).nodeId;
            int nS = comp->pin(1).nodeId;
            int nD = comp->pin(2).nodeId;
            double vG = (nG >= 0 && result.nodeResults.count(nG)) ? result.nodeResults[nG].voltage : 0.0;
            double vS = (nS >= 0 && result.nodeResults.count(nS)) ? result.nodeResults[nS].voltage : 0.0;
            double vD = (nD >= 0 && result.nodeResults.count(nD)) ? result.nodeResults[nD].voltage : 0.0;
            double vsd = vS - vD;
            br.voltageDrop = vsd;
            double R = ((vS - vG) >= comp->value()) ? PMosfet::R_on : PMosfet::R_off;
            br.current = vsd / R;
        } else if (isLogicGate(comp->type())) {
            int vddPin = (comp->type() == ComponentType::NOTGate) ? 2 : 3;
            int gndPin = (comp->type() == ComponentType::NOTGate) ? 3 : 4;
            int nVDD = comp->pin(vddPin).nodeId;
            int nGND = comp->pin(gndPin).nodeId;
            double vVDD = (nVDD >= 0 && result.nodeResults.count(nVDD)) ? result.nodeResults[nVDD].voltage : 0.0;
            double vGND_v = (nGND >= 0 && result.nodeResults.count(nGND)) ? result.nodeResults[nGND].voltage : 0.0;
            br.voltageDrop = vVDD - vGND_v;
            br.current = (vVDD - vGND_v) / (NOTGate::R_on + NOTGate::R_off);
        } else if (comp->type() == ComponentType::DCCurrentSource) {
            br.current = comp->value();
        } else if (comp->value() > 0.0) {
            br.current = br.voltageDrop / comp->value();
        }

        br.power = std::abs(br.voltageDrop * br.current);
        result.branchResults[id] = br;
    }

    result.success = true;
    return result;
}

// --- Transient Analysis (full frames for oscilloscope) ---

TransientResult MNASolver::solveTransientFull(Circuit& circuit, double timeStep, double totalTime,
                                              std::atomic<bool>* cancelled)
{
    TransientResult tResult;

    auto setup = prepareMNA(circuit);
    if (!setup.ok) {
        tResult.errorMessage = setup.error;
        return tResult;
    }

    int numNodes = setup.numNodes;
    int matSize = setup.matSize;
    int numSteps = static_cast<int>(totalTime / timeStep);

    Eigen::VectorXd xPrev = Eigen::VectorXd::Zero(matSize);

    // Track inductor currents (state variable that accumulates over time)
    std::map<int, double> inductorCurrents;
    for (auto& [id, comp] : circuit.components()) {
        if (comp->type() == ComponentType::Inductor)
            inductorCurrents[id] = 0.0;
    }

    // Track capacitor currents (computed each step from voltage difference)
    std::map<int, double> capacitorCurrents;
    for (auto& [id, comp] : circuit.components()) {
        if (comp->type() == ComponentType::Capacitor)
            capacitorCurrents[id] = 0.0;
    }

    for (int step = 0; step <= numSteps; ++step) {
        if (cancelled && cancelled->load()) {
            tResult.errorMessage = "Cancelled";
            return tResult;
        }
        double t = step * timeStep;

        Eigen::MatrixXd A = Eigen::MatrixXd::Zero(matSize, matSize);
        Eigen::VectorXd bVec = Eigen::VectorXd::Zero(matSize);

        for (auto& [id, comp] : circuit.components()) {
            if (!canStamp(comp.get())) continue;

            if (comp->type() == ComponentType::Capacitor) {
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double geq = comp->value() / timeStep;

                if (n1 >= 0) A(n1, n1) += geq;
                if (n2 >= 0) A(n2, n2) += geq;
                if (n1 >= 0 && n2 >= 0) { A(n1, n2) -= geq; A(n2, n1) -= geq; }

                double vP1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vP2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                double ieq = geq * (vP1 - vP2);
                if (n1 >= 0) bVec(n1) += ieq;
                if (n2 >= 0) bVec(n2) -= ieq;

            } else if (comp->type() == ComponentType::Inductor) {
                // Backward Euler: I_L(n) = I_L(n-1) + dt/L * V_L(n)
                // Norton equivalent: conductance dt/L + current source I_L(n-1)
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double geq = timeStep / comp->value();

                if (n1 >= 0) A(n1, n1) += geq;
                if (n2 >= 0) A(n2, n2) += geq;
                if (n1 >= 0 && n2 >= 0) { A(n1, n2) -= geq; A(n2, n1) -= geq; }

                // History current source: accumulated inductor current
                double iPrev = inductorCurrents[id];
                if (n1 >= 0) bVec(n1) -= iPrev;
                if (n2 >= 0) bVec(n2) += iPrev;

            } else if (comp->type() == ComponentType::ACSource) {
                auto* ac = static_cast<ACSource*>(comp.get());
                double v = ac->value() * std::sin(2.0 * M_PI * ac->frequency() * t
                                                    + ac->phase() * M_PI / 180.0);
                int n_pos = comp->pin(0).nodeId - 1;
                int n_neg = comp->pin(1).nodeId - 1;
                int bi = comp->branchIndex();
                if (bi < 0) continue;

                if (n_pos >= 0) { A(n_pos, bi) += 1.0; A(bi, n_pos) += 1.0; }
                if (n_neg >= 0) { A(n_neg, bi) -= 1.0; A(bi, n_neg) -= 1.0; }
                bVec(bi) = v;

            } else if (comp->type() == ComponentType::PulseSource) {
                auto* sq = static_cast<PulseSource*>(comp.get());
                double period = 1.0 / sq->frequency();
                double phaseOffset = sq->phase() / 360.0 * period;
                double tMod = std::fmod(t + phaseOffset, period);
                if (tMod < 0) tMod += period;
                double v = (tMod < period * sq->dutyCycle()) ? sq->value() : -sq->value();

                int n_pos = comp->pin(0).nodeId - 1;
                int n_neg = comp->pin(1).nodeId - 1;
                int bi = comp->branchIndex();
                if (bi < 0) continue;

                if (n_pos >= 0) { A(n_pos, bi) += 1.0; A(bi, n_pos) += 1.0; }
                if (n_neg >= 0) { A(n_neg, bi) -= 1.0; A(bi, n_neg) -= 1.0; }
                bVec(bi) = v;

            } else if (comp->type() == ComponentType::Diode) {
                // Piecewise-linear diode: state from previous voltage
                auto* diode = static_cast<Diode*>(comp.get());
                int n1 = comp->pin(0).nodeId - 1; // anode
                int n2 = comp->pin(1).nodeId - 1; // cathode
                double vPrev1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vPrev2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                bool forward = (vPrev1 - vPrev2) > comp->value(); // Vf threshold
                diode->stampWithState(A, bVec, forward);

            } else if (comp->type() == ComponentType::ZenerDiode) {
                auto* zd = static_cast<ZenerDiode*>(comp.get());
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double vPrev1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vPrev2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                double vd = vPrev1 - vPrev2;
                ZenerDiode::State state;
                if (vd > zd->forwardVoltage())
                    state = ZenerDiode::Forward;
                else if (vd < -zd->zenerVoltage())
                    state = ZenerDiode::Breakdown;
                else
                    state = ZenerDiode::Blocking;
                zd->stampWithState(A, bVec, state);

            } else if (comp->type() == ComponentType::NMosfet) {
                auto* m = static_cast<NMosfet*>(comp.get());
                int nG = comp->pin(0).nodeId - 1;
                int nS = comp->pin(2).nodeId - 1;
                double vG = (nG >= 0) ? xPrev(nG) : 0.0;
                double vS = (nS >= 0) ? xPrev(nS) : 0.0;
                bool on = (vG - vS) >= comp->value();
                m->stampWithState(A, bVec, on ? NMosfet::On : NMosfet::Cutoff);

            } else if (comp->type() == ComponentType::PMosfet) {
                auto* m = static_cast<PMosfet*>(comp.get());
                int nG = comp->pin(0).nodeId - 1;
                int nS = comp->pin(1).nodeId - 1;
                double vG = (nG >= 0) ? xPrev(nG) : 0.0;
                double vS = (nS >= 0) ? xPrev(nS) : 0.0;
                bool on = (vS - vG) >= comp->value();
                m->stampWithState(A, bVec, on ? PMosfet::On : PMosfet::Cutoff);

            } else if (comp->type() == ComponentType::NOTGate) {
                auto* g = static_cast<NOTGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nGND = comp->pin(3).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate(comp.get(), vA, vGND);
                g->stampWithState(A, bVec, outHigh ? NOTGate::OutputHigh : NOTGate::OutputLow);

            } else if (comp->type() == ComponentType::ANDGate) {
                auto* g = static_cast<ANDGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, bVec, outHigh ? ANDGate::OutputHigh : ANDGate::OutputLow);

            } else if (comp->type() == ComponentType::ORGate) {
                auto* g = static_cast<ORGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, bVec, outHigh ? ORGate::OutputHigh : ORGate::OutputLow);

            } else if (comp->type() == ComponentType::XORGate) {
                auto* g = static_cast<XORGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, bVec, outHigh ? XORGate::OutputHigh : XORGate::OutputLow);

            } else {
                comp->stampDC(A, bVec, matSize);
            }
        }

        Eigen::PartialPivLU<Eigen::MatrixXd> solver(A);
        double det = solver.determinant();
        if (std::abs(det) < 1e-15) {
            tResult.errorMessage = "Singular matrix at t=" + QString::number(t, 'f', 4) + "s";
            return tResult;
        }

        Eigen::VectorXd x = solver.solve(bVec);

        // Update inductor currents: I_L(n) = I_L(n-1) + dt/L * V_L(n)
        for (auto& [id, comp] : circuit.components()) {
            if (comp->type() != ComponentType::Inductor) continue;
            if (!canStamp(comp.get())) continue;
            int n1 = comp->pin(0).nodeId - 1;
            int n2 = comp->pin(1).nodeId - 1;
            double v1 = (n1 >= 0) ? x(n1) : 0.0;
            double v2 = (n2 >= 0) ? x(n2) : 0.0;
            double vL = v1 - v2;
            inductorCurrents[id] += (timeStep / comp->value()) * vL;
        }

        // Update capacitor currents: I_C = C/dt * (V_n - V_n-1)
        for (auto& [id, comp] : circuit.components()) {
            if (comp->type() != ComponentType::Capacitor) continue;
            if (!canStamp(comp.get())) continue;
            int n1 = comp->pin(0).nodeId - 1;
            int n2 = comp->pin(1).nodeId - 1;
            double v1 = (n1 >= 0) ? x(n1) : 0.0;
            double v2 = (n2 >= 0) ? x(n2) : 0.0;
            double v1p = (n1 >= 0) ? xPrev(n1) : 0.0;
            double v2p = (n2 >= 0) ? xPrev(n2) : 0.0;
            capacitorCurrents[id] = comp->value() / timeStep * ((v1 - v2) - (v1p - v2p));
        }

        // Record frame
        TransientFrame frame;
        frame.time = t;
        frame.nodeVoltages[0] = 0.0;
        for (int i = 0; i < numNodes; ++i) {
            frame.nodeVoltages[i + 1] = x(i);
        }

        int vsIdx = 0;
        for (auto& [id, comp] : circuit.components()) {
            if (comp->pinCount() < 2) continue; // Skip Ground, etc.

            int p1, p2;
            getActivePins(comp.get(), p1, p2);
            int n1 = comp->pin(p1).nodeId;
            int n2 = comp->pin(p2).nodeId;
            double v1 = (n1 > 0 && n1 <= numNodes) ? x(n1 - 1) : 0.0;
            double v2 = (n2 > 0 && n2 <= numNodes) ? x(n2 - 1) : 0.0;
            double vDrop = v1 - v2;

            double current = 0.0;
            if (comp->isVoltageSource()) {
                current = x(numNodes + vsIdx);
                ++vsIdx;
            } else if (comp->type() == ComponentType::Inductor) {
                current = inductorCurrents[id];
            } else if (comp->type() == ComponentType::Capacitor) {
                current = capacitorCurrents[id];
            } else if (comp->type() == ComponentType::Diode) {
                if (vDrop > comp->value())
                    current = (vDrop - comp->value()) / Diode::R_on;
                else if (vDrop > 0)
                    current = 0.0;
                else
                    current = vDrop / Diode::R_off;
            } else if (comp->type() == ComponentType::ZenerDiode) {
                auto* zd = static_cast<ZenerDiode*>(comp.get());
                if (vDrop > zd->forwardVoltage())
                    current = (vDrop - zd->forwardVoltage()) / ZenerDiode::R_on;
                else if (vDrop < -zd->zenerVoltage())
                    current = (vDrop + zd->zenerVoltage()) / ZenerDiode::R_on;
                else
                    current = vDrop / ZenerDiode::R_off;
            } else if (comp->type() == ComponentType::NMosfet) {
                // NMOS: Pin 0=Gate, Pin 1=Drain, Pin 2=Source
                int nG2 = comp->pin(0).nodeId;
                int nS2 = comp->pin(2).nodeId;
                int nD2 = comp->pin(1).nodeId;
                double vG2 = (nG2 > 0 && nG2 <= numNodes) ? x(nG2 - 1) : 0.0;
                double vS2 = (nS2 > 0 && nS2 <= numNodes) ? x(nS2 - 1) : 0.0;
                double vD2 = (nD2 > 0 && nD2 <= numNodes) ? x(nD2 - 1) : 0.0;
                double vds2 = vD2 - vS2;
                vDrop = vds2;
                double R = ((vG2 - vS2) >= comp->value()) ? NMosfet::R_on : NMosfet::R_off;
                current = vds2 / R;
            } else if (comp->type() == ComponentType::PMosfet) {
                // PMOS: Pin 0=Gate, Pin 1=Source, Pin 2=Drain
                int nG2 = comp->pin(0).nodeId;
                int nS2 = comp->pin(1).nodeId;
                int nD2 = comp->pin(2).nodeId;
                double vG2 = (nG2 > 0 && nG2 <= numNodes) ? x(nG2 - 1) : 0.0;
                double vS2 = (nS2 > 0 && nS2 <= numNodes) ? x(nS2 - 1) : 0.0;
                double vD2 = (nD2 > 0 && nD2 <= numNodes) ? x(nD2 - 1) : 0.0;
                double vsd2 = vS2 - vD2;
                vDrop = vsd2;
                double R = ((vS2 - vG2) >= comp->value()) ? PMosfet::R_on : PMosfet::R_off;
                current = vsd2 / R;
            } else if (isLogicGate(comp->type())) {
                int vddPin = (comp->type() == ComponentType::NOTGate) ? 2 : 3;
                int gndPin = (comp->type() == ComponentType::NOTGate) ? 3 : 4;
                int nVDD2 = comp->pin(vddPin).nodeId;
                int nGND2 = comp->pin(gndPin).nodeId;
                double vVDD2 = (nVDD2 > 0 && nVDD2 <= numNodes) ? x(nVDD2 - 1) : 0.0;
                double vGND2 = (nGND2 > 0 && nGND2 <= numNodes) ? x(nGND2 - 1) : 0.0;
                vDrop = vVDD2 - vGND2;
                current = (vVDD2 - vGND2) / (NOTGate::R_on + NOTGate::R_off);
            } else if (comp->type() == ComponentType::DCCurrentSource) {
                current = comp->value();
            } else if (comp->value() > 0.0) {
                current = vDrop / comp->value();
            }
            frame.branchCurrents[id] = current;
            frame.componentVoltages[id] = vDrop;
        }

        tResult.frames.push_back(std::move(frame));
        xPrev = x;
    }

    tResult.success = true;
    return tResult;
}

// --- Mixed DC+AC Analysis (Superposition) ---

SimulationResult MNASolver::solveMixed(Circuit& circuit, double acFrequency, std::atomic<bool>* cancelled)
{
    // Step 1: Solve DC (AC sources contribute 0V in DC)
    SimulationResult dcResult = solveDC(circuit, cancelled);
    if (!dcResult.success) {
        dcResult.errorMessage = "DC part of superposition failed: " + dcResult.errorMessage;
        return dcResult;
    }

    // Step 2: Solve AC
    SimulationResult acResult = solveAC(circuit, acFrequency, cancelled);
    if (!acResult.success) {
        acResult.errorMessage = "AC part of superposition failed: " + acResult.errorMessage;
        return acResult;
    }

    // Step 3: Combine — DC values are the bias point, AC values are the amplitude
    SimulationResult result;
    result.success = true;

    // Node voltages: use DC as the total (AC is magnitude overlay)
    result.nodeResults = dcResult.nodeResults;

    // Branch results: combine DC + AC
    for (auto& [compId, dcBr] : dcResult.branchResults) {
        BranchResult br;
        br.componentId = compId;
        br.isMixed = true;

        br.dcVoltage = dcBr.voltageDrop;
        br.dcCurrent = dcBr.current;

        auto acIt = acResult.branchResults.find(compId);
        if (acIt != acResult.branchResults.end()) {
            br.acVoltage = acIt->second.voltageDrop;
            br.acCurrent = acIt->second.current;
        }

        // Total values: DC + AC peak
        br.voltageDrop = br.dcVoltage + br.acVoltage;
        br.current = br.dcCurrent + br.acCurrent;
        br.power = std::abs(br.voltageDrop * br.current);

        result.branchResults[compId] = br;
    }

    return result;
}

// --- Extract SimulationResult from last frame of TransientResult ---

SimulationResult MNASolver::resultFromLastFrame(const TransientResult& tResult, Circuit& circuit)
{
    SimulationResult result;
    if (!tResult.success || tResult.frames.empty()) {
        result.errorMessage = tResult.errorMessage;
        return result;
    }

    const auto& frame = tResult.frames.back();

    // Node voltages
    for (auto& [nodeId, voltage] : frame.nodeVoltages) {
        NodeResult nr;
        nr.nodeId = nodeId;
        nr.voltage = voltage;
        result.nodeResults[nodeId] = nr;
    }

    // Update node objects
    for (auto& node : circuit.nodes()) {
        auto it = result.nodeResults.find(node->id());
        if (it != result.nodeResults.end())
            node->setVoltage(it->second.voltage);
    }

    // Branch results
    for (auto& [compId, comp] : circuit.components()) {
        if (comp->pinCount() < 2) continue;

        BranchResult br;
        br.componentId = compId;

        auto vIt = frame.componentVoltages.find(compId);
        br.voltageDrop = (vIt != frame.componentVoltages.end()) ? vIt->second : 0.0;

        auto iIt = frame.branchCurrents.find(compId);
        br.current = (iIt != frame.branchCurrents.end()) ? iIt->second : 0.0;

        br.power = std::abs(br.voltageDrop * br.current);
        result.branchResults[compId] = br;
    }

    result.success = true;
    return result;
}

// --- Streaming Transient with Continuation ---

void MNASolver::solveTransientStreaming(
    Circuit& circuit, double timeStep, double totalTime,
    const TransientState& initialState,
    SharedFrameBuffer& buffer,
    std::atomic<bool>* cancelled)
{
    auto setup = prepareMNA(circuit);
    if (!setup.ok) {
        buffer.markComplete(false, setup.error, TransientState());
        return;
    }

    int numNodes = setup.numNodes;
    int matSize = setup.matSize;
    int numSteps = static_cast<int>(totalTime / timeStep);

    // Determine starting state: resume or fresh
    int startStep;
    Eigen::VectorXd xPrev;
    std::map<int, double> inductorCurrents;
    std::map<int, double> capacitorCurrents;

    if (initialState.isValid()
        && initialState.numNodes == numNodes
        && initialState.matSize == matSize
        && std::abs(initialState.timeStep - timeStep) < 1e-15) {
        // Resume from saved state
        startStep = initialState.completedSteps + 1;
        xPrev = initialState.xPrev;
        inductorCurrents = initialState.inductorCurrents;
        capacitorCurrents = initialState.capacitorCurrents;
    } else {
        // Fresh start
        startStep = 0;
        xPrev = Eigen::VectorXd::Zero(matSize);
        for (auto& [id, comp] : circuit.components()) {
            if (comp->type() == ComponentType::Inductor)
                inductorCurrents[id] = 0.0;
            if (comp->type() == ComponentType::Capacitor)
                capacitorCurrents[id] = 0.0;
        }
    }

    constexpr int BATCH_SIZE = 64;
    std::vector<TransientFrame> localBatch;
    localBatch.reserve(BATCH_SIZE);

    auto saveState = [&](int lastStep) {
        TransientState state;
        state.completedSteps = lastStep;
        state.currentTime = lastStep * timeStep;
        state.xPrev = xPrev;
        state.inductorCurrents = inductorCurrents;
        state.capacitorCurrents = capacitorCurrents;
        state.numNodes = numNodes;
        state.matSize = matSize;
        state.timeStep = timeStep;
        return state;
    };

    for (int step = startStep; step <= numSteps; ++step) {
        if (cancelled && cancelled->load()) {
            if (!localBatch.empty())
                buffer.appendFrames(std::move(localBatch));
            buffer.markComplete(false, "Cancelled", saveState(step > 0 ? step - 1 : 0));
            return;
        }

        double t = step * timeStep;

        Eigen::MatrixXd A = Eigen::MatrixXd::Zero(matSize, matSize);
        Eigen::VectorXd bVec = Eigen::VectorXd::Zero(matSize);

        for (auto& [id, comp] : circuit.components()) {
            if (!canStamp(comp.get())) continue;

            if (comp->type() == ComponentType::Capacitor) {
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double geq = comp->value() / timeStep;

                if (n1 >= 0) A(n1, n1) += geq;
                if (n2 >= 0) A(n2, n2) += geq;
                if (n1 >= 0 && n2 >= 0) { A(n1, n2) -= geq; A(n2, n1) -= geq; }

                double vP1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vP2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                double ieq = geq * (vP1 - vP2);
                if (n1 >= 0) bVec(n1) += ieq;
                if (n2 >= 0) bVec(n2) -= ieq;

            } else if (comp->type() == ComponentType::Inductor) {
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double geq = timeStep / comp->value();

                if (n1 >= 0) A(n1, n1) += geq;
                if (n2 >= 0) A(n2, n2) += geq;
                if (n1 >= 0 && n2 >= 0) { A(n1, n2) -= geq; A(n2, n1) -= geq; }

                double iPrev = inductorCurrents[id];
                if (n1 >= 0) bVec(n1) -= iPrev;
                if (n2 >= 0) bVec(n2) += iPrev;

            } else if (comp->type() == ComponentType::ACSource) {
                auto* ac = static_cast<ACSource*>(comp.get());
                double v = ac->value() * std::sin(2.0 * M_PI * ac->frequency() * t
                                                    + ac->phase() * M_PI / 180.0);
                int n_pos = comp->pin(0).nodeId - 1;
                int n_neg = comp->pin(1).nodeId - 1;
                int bi = comp->branchIndex();
                if (bi < 0) continue;

                if (n_pos >= 0) { A(n_pos, bi) += 1.0; A(bi, n_pos) += 1.0; }
                if (n_neg >= 0) { A(n_neg, bi) -= 1.0; A(bi, n_neg) -= 1.0; }
                bVec(bi) = v;

            } else if (comp->type() == ComponentType::PulseSource) {
                auto* sq = static_cast<PulseSource*>(comp.get());
                double period = 1.0 / sq->frequency();
                double phaseOffset = sq->phase() / 360.0 * period;
                double tMod = std::fmod(t + phaseOffset, period);
                if (tMod < 0) tMod += period;
                double v = (tMod < period * sq->dutyCycle()) ? sq->value() : -sq->value();

                int n_pos = comp->pin(0).nodeId - 1;
                int n_neg = comp->pin(1).nodeId - 1;
                int bi = comp->branchIndex();
                if (bi < 0) continue;

                if (n_pos >= 0) { A(n_pos, bi) += 1.0; A(bi, n_pos) += 1.0; }
                if (n_neg >= 0) { A(n_neg, bi) -= 1.0; A(bi, n_neg) -= 1.0; }
                bVec(bi) = v;

            } else if (comp->type() == ComponentType::Diode) {
                auto* diode = static_cast<Diode*>(comp.get());
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double vPrev1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vPrev2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                bool forward = (vPrev1 - vPrev2) > comp->value();
                diode->stampWithState(A, bVec, forward);

            } else if (comp->type() == ComponentType::ZenerDiode) {
                auto* zd = static_cast<ZenerDiode*>(comp.get());
                int n1 = comp->pin(0).nodeId - 1;
                int n2 = comp->pin(1).nodeId - 1;
                double vPrev1 = (n1 >= 0) ? xPrev(n1) : 0.0;
                double vPrev2 = (n2 >= 0) ? xPrev(n2) : 0.0;
                double vd = vPrev1 - vPrev2;
                ZenerDiode::State state;
                if (vd > zd->forwardVoltage())
                    state = ZenerDiode::Forward;
                else if (vd < -zd->zenerVoltage())
                    state = ZenerDiode::Breakdown;
                else
                    state = ZenerDiode::Blocking;
                zd->stampWithState(A, bVec, state);

            } else if (comp->type() == ComponentType::NMosfet) {
                auto* m = static_cast<NMosfet*>(comp.get());
                int nG = comp->pin(0).nodeId - 1;
                int nS = comp->pin(2).nodeId - 1;
                double vG = (nG >= 0) ? xPrev(nG) : 0.0;
                double vS = (nS >= 0) ? xPrev(nS) : 0.0;
                bool on = (vG - vS) >= comp->value();
                m->stampWithState(A, bVec, on ? NMosfet::On : NMosfet::Cutoff);

            } else if (comp->type() == ComponentType::PMosfet) {
                auto* m = static_cast<PMosfet*>(comp.get());
                int nG = comp->pin(0).nodeId - 1;
                int nS = comp->pin(1).nodeId - 1;
                double vG = (nG >= 0) ? xPrev(nG) : 0.0;
                double vS = (nS >= 0) ? xPrev(nS) : 0.0;
                bool on = (vS - vG) >= comp->value();
                m->stampWithState(A, bVec, on ? PMosfet::On : PMosfet::Cutoff);

            } else if (comp->type() == ComponentType::NOTGate) {
                auto* g = static_cast<NOTGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nGND = comp->pin(3).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate(comp.get(), vA, vGND);
                g->stampWithState(A, bVec, outHigh ? NOTGate::OutputHigh : NOTGate::OutputLow);

            } else if (comp->type() == ComponentType::ANDGate) {
                auto* g = static_cast<ANDGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, bVec, outHigh ? ANDGate::OutputHigh : ANDGate::OutputLow);

            } else if (comp->type() == ComponentType::ORGate) {
                auto* g = static_cast<ORGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, bVec, outHigh ? ORGate::OutputHigh : ORGate::OutputLow);

            } else if (comp->type() == ComponentType::XORGate) {
                auto* g = static_cast<XORGate*>(comp.get());
                int nA   = comp->pin(0).nodeId - 1;
                int nB   = comp->pin(1).nodeId - 1;
                int nGND = comp->pin(4).nodeId - 1;
                double vA   = (nA >= 0) ? xPrev(nA) : 0.0;
                double vB   = (nB >= 0) ? xPrev(nB) : 0.0;
                double vGND = (nGND >= 0) ? xPrev(nGND) : 0.0;
                bool outHigh = evaluateGate2(comp.get(), vA, vB, vGND);
                g->stampWithState(A, bVec, outHigh ? XORGate::OutputHigh : XORGate::OutputLow);

            } else {
                comp->stampDC(A, bVec, matSize);
            }
        }

        Eigen::PartialPivLU<Eigen::MatrixXd> solver(A);
        double det = solver.determinant();
        if (std::abs(det) < 1e-15) {
            if (!localBatch.empty())
                buffer.appendFrames(std::move(localBatch));
            buffer.markComplete(false,
                "Singular matrix at t=" + QString::number(t, 'f', 4) + "s",
                saveState(step > 0 ? step - 1 : 0));
            return;
        }

        Eigen::VectorXd x = solver.solve(bVec);

        // Update inductor currents
        for (auto& [id, comp] : circuit.components()) {
            if (comp->type() != ComponentType::Inductor) continue;
            if (!canStamp(comp.get())) continue;
            int n1 = comp->pin(0).nodeId - 1;
            int n2 = comp->pin(1).nodeId - 1;
            double v1 = (n1 >= 0) ? x(n1) : 0.0;
            double v2 = (n2 >= 0) ? x(n2) : 0.0;
            double vL = v1 - v2;
            inductorCurrents[id] += (timeStep / comp->value()) * vL;
        }

        // Update capacitor currents
        for (auto& [id, comp] : circuit.components()) {
            if (comp->type() != ComponentType::Capacitor) continue;
            if (!canStamp(comp.get())) continue;
            int n1 = comp->pin(0).nodeId - 1;
            int n2 = comp->pin(1).nodeId - 1;
            double v1 = (n1 >= 0) ? x(n1) : 0.0;
            double v2 = (n2 >= 0) ? x(n2) : 0.0;
            double v1p = (n1 >= 0) ? xPrev(n1) : 0.0;
            double v2p = (n2 >= 0) ? xPrev(n2) : 0.0;
            capacitorCurrents[id] = comp->value() / timeStep * ((v1 - v2) - (v1p - v2p));
        }

        // Record frame
        TransientFrame frame;
        frame.time = t;
        frame.nodeVoltages[0] = 0.0;
        for (int i = 0; i < numNodes; ++i) {
            frame.nodeVoltages[i + 1] = x(i);
        }

        int vsIdx = 0;
        for (auto& [id, comp] : circuit.components()) {
            if (comp->pinCount() < 2) continue;

            int p1, p2;
            getActivePins(comp.get(), p1, p2);
            int n1 = comp->pin(p1).nodeId;
            int n2 = comp->pin(p2).nodeId;
            double v1 = (n1 > 0 && n1 <= numNodes) ? x(n1 - 1) : 0.0;
            double v2 = (n2 > 0 && n2 <= numNodes) ? x(n2 - 1) : 0.0;
            double vDrop = v1 - v2;

            double current = 0.0;
            if (comp->isVoltageSource()) {
                current = x(numNodes + vsIdx);
                ++vsIdx;
            } else if (comp->type() == ComponentType::Inductor) {
                current = inductorCurrents[id];
            } else if (comp->type() == ComponentType::Capacitor) {
                current = capacitorCurrents[id];
            } else if (comp->type() == ComponentType::Diode) {
                if (vDrop > comp->value())
                    current = (vDrop - comp->value()) / Diode::R_on;
                else if (vDrop > 0)
                    current = 0.0;
                else
                    current = vDrop / Diode::R_off;
            } else if (comp->type() == ComponentType::ZenerDiode) {
                auto* zd = static_cast<ZenerDiode*>(comp.get());
                if (vDrop > zd->forwardVoltage())
                    current = (vDrop - zd->forwardVoltage()) / ZenerDiode::R_on;
                else if (vDrop < -zd->zenerVoltage())
                    current = (vDrop + zd->zenerVoltage()) / ZenerDiode::R_on;
                else
                    current = vDrop / ZenerDiode::R_off;
            } else if (comp->type() == ComponentType::NMosfet) {
                int nG2 = comp->pin(0).nodeId;
                int nS2 = comp->pin(2).nodeId;
                int nD2 = comp->pin(1).nodeId;
                double vG2 = (nG2 > 0 && nG2 <= numNodes) ? x(nG2 - 1) : 0.0;
                double vS2 = (nS2 > 0 && nS2 <= numNodes) ? x(nS2 - 1) : 0.0;
                double vD2 = (nD2 > 0 && nD2 <= numNodes) ? x(nD2 - 1) : 0.0;
                double vds2 = vD2 - vS2;
                vDrop = vds2;
                double R = ((vG2 - vS2) >= comp->value()) ? NMosfet::R_on : NMosfet::R_off;
                current = vds2 / R;
            } else if (comp->type() == ComponentType::PMosfet) {
                int nG2 = comp->pin(0).nodeId;
                int nS2 = comp->pin(1).nodeId;
                int nD2 = comp->pin(2).nodeId;
                double vG2 = (nG2 > 0 && nG2 <= numNodes) ? x(nG2 - 1) : 0.0;
                double vS2 = (nS2 > 0 && nS2 <= numNodes) ? x(nS2 - 1) : 0.0;
                double vD2 = (nD2 > 0 && nD2 <= numNodes) ? x(nD2 - 1) : 0.0;
                double vsd2 = vS2 - vD2;
                vDrop = vsd2;
                double R = ((vS2 - vG2) >= comp->value()) ? PMosfet::R_on : PMosfet::R_off;
                current = vsd2 / R;
            } else if (isLogicGate(comp->type())) {
                int vddPin = (comp->type() == ComponentType::NOTGate) ? 2 : 3;
                int gndPin = (comp->type() == ComponentType::NOTGate) ? 3 : 4;
                int nVDD2 = comp->pin(vddPin).nodeId;
                int nGND2 = comp->pin(gndPin).nodeId;
                double vVDD2 = (nVDD2 > 0 && nVDD2 <= numNodes) ? x(nVDD2 - 1) : 0.0;
                double vGND2 = (nGND2 > 0 && nGND2 <= numNodes) ? x(nGND2 - 1) : 0.0;
                vDrop = vVDD2 - vGND2;
                current = (vVDD2 - vGND2) / (NOTGate::R_on + NOTGate::R_off);
            } else if (comp->type() == ComponentType::DCCurrentSource) {
                current = comp->value();
            } else if (comp->value() > 0.0) {
                current = vDrop / comp->value();
            }
            frame.branchCurrents[id] = current;
            frame.componentVoltages[id] = vDrop;
        }

        localBatch.push_back(std::move(frame));
        xPrev = x;

        // Flush batch when full
        if (static_cast<int>(localBatch.size()) >= BATCH_SIZE) {
            buffer.appendFrames(std::move(localBatch));
            localBatch.clear();
            localBatch.reserve(BATCH_SIZE);
        }
    }

    // Flush remaining frames
    if (!localBatch.empty())
        buffer.appendFrames(std::move(localBatch));

    buffer.markComplete(true, QString(), saveState(numSteps));
}
