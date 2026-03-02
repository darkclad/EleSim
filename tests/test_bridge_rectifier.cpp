#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <algorithm>
#include <numeric>

#include "core/Circuit.h"
#include "core/components/ACSource.h"
#include "simulation/MNASolver.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Helper: load a circuit from JSON file ───
static std::unique_ptr<Circuit> loadCircuit(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return Circuit::fromJson(doc.object());
}

// ─── Helper: find component by name ───
static Component* findByName(Circuit& circuit, const QString& name)
{
    for (auto& [id, comp] : circuit.components()) {
        if (comp->name() == name) return comp.get();
    }
    return nullptr;
}

// ─── Helper: get output node ID (R_load pin 0 = cathode side of bridge) ───
static int getOutputNode(Circuit& circuit)
{
    Component* resistor = findByName(circuit, "R_load");
    if (!resistor) return -1;
    return resistor->pin(0).nodeId;
}

// ═══════════════════════════════════════════════════════
//  Full-Wave Bridge Rectifier Test
//
//  Circuit: AC 10Vpk@50Hz → D1-D4 bridge → R_load 4.7kΩ → GND
//
//  Without filter:
//    V_out peak = V_in - 2×Vf = 10 - 1.4 = 8.60V
//    I_peak = 8.60 / 4700 = 1.83mA
//    Full-wave: 100Hz ripple (double input freq)
//    V_out always ≥ 0 (both half cycles rectified)
//
//  With filter (47µF):
//    V_peak = 8.60V
//    T_discharge = 1/(2×50) = 10ms (full-wave → 100Hz)
//    RC = 4700 × 47µ = 0.2209s
//    V_min = V_peak × exp(-T/RC) = 8.60 × 0.9557 = 8.22V
//    Ripple = 0.38V
//    V_avg ≈ 8.41V
// ═══════════════════════════════════════════════════════

class TestBridgeRectifier : public QObject
{
    Q_OBJECT

private slots:
    void withoutFilter();
    void withFilter();
};

void TestBridgeRectifier::withoutFilter()
{
    // Load circuit (AC source + 4 diodes bridge + R_load)
    QString circuitPath = QString(TEST_DATA_DIR) + "/bridge_rectifier.esim";
    auto circuit = loadCircuit(circuitPath);
    QVERIFY2(circuit != nullptr, "Failed to load bridge_rectifier.esim");

    // Transient parameters: 50Hz → T=20ms, run 5 cycles = 100ms
    const double timeStep = 0.0001;  // 100µs
    const double totalTime = 0.1;    // 100ms

    // Run transient analysis
    TransientResult result = MNASolver::solveTransientFull(*circuit, timeStep, totalTime);
    QVERIFY2(result.success, qPrintable("Transient solve failed: " + result.errorMessage));
    QVERIFY(result.frames.size() > 100);

    // Find the output node (R_load pin 0)
    int outNode = getOutputNode(*circuit);
    QVERIFY2(outNode > 0, "Could not find output node");

    // Extract V_out waveform from the last 3 cycles (t > 40ms) for steady state
    std::vector<double> vOut;
    for (const auto& frame : result.frames) {
        if (frame.time < 0.04) continue; // skip first 2 cycles
        auto it = frame.nodeVoltages.find(outNode);
        if (it != frame.nodeVoltages.end())
            vOut.push_back(it->second);
    }
    QVERIFY(!vOut.empty());

    double vMax = *std::max_element(vOut.begin(), vOut.end());
    double vMin = *std::min_element(vOut.begin(), vOut.end());

    // Expected: V_out peak ≈ 8.60V (= 10.0 - 2×0.7V diode drop)
    // With R_on=1Ω per diode: V_out = (V_in - 2×Vf) × R_load / (R_load + 2×R_on)
    //                                = 8.6 × 4700/4702 ≈ 8.596V
    const double expectedPeak = 8.60;
    const double tolerance = 0.05; // 5% for transient sim

    double relErrPeak = std::abs(vMax - expectedPeak) / expectedPeak;
    QVERIFY2(relErrPeak < tolerance,
        qPrintable(QString("V_out peak: got %1, expected %2 (err %3%)")
            .arg(vMax, 0, 'f', 4)
            .arg(expectedPeak, 0, 'f', 4)
            .arg(relErrPeak * 100, 0, 'f', 2)));

    // Full-wave: V_out should never go significantly negative
    // Piecewise-linear diode model causes brief negative spikes at zero crossings
    // due to all 4 diodes transitioning states simultaneously
    QVERIFY2(vMin > -0.5,
        qPrintable(QString("V_out min should be near 0 (full-wave): got %1V")
            .arg(vMin, 0, 'f', 4)));

    // Find peak current through R_load
    Component* resistor = findByName(*circuit, "R_load");
    QVERIFY(resistor != nullptr);
    double iPeakMax = 0;
    for (const auto& frame : result.frames) {
        if (frame.time < 0.04) continue;
        auto it = frame.branchCurrents.find(resistor->id());
        if (it != frame.branchCurrents.end())
            iPeakMax = std::max(iPeakMax, std::abs(it->second));
    }

    // Expected I_peak ≈ 1.83mA (= 8.60V / 4700Ω)
    const double expectedIPeak = 1.83e-3;
    double relErrI = std::abs(iPeakMax - expectedIPeak) / expectedIPeak;
    QVERIFY2(relErrI < tolerance,
        qPrintable(QString("I_peak: got %1mA, expected %2mA (err %3%)")
            .arg(iPeakMax * 1000, 0, 'f', 4)
            .arg(expectedIPeak * 1000, 0, 'f', 4)
            .arg(relErrI * 100, 0, 'f', 2)));

    // Full-wave shape: V_out should be positive for nearly 100% of time
    // (vs ~50% for half-wave). Only near zero at AC zero crossings.
    int positiveCount = 0;
    for (double v : vOut) {
        if (v > 0.1) ++positiveCount;
    }
    double positiveRatio = static_cast<double>(positiveCount) / vOut.size();
    // 85% threshold: clearly distinguishes full-wave (~90%) from half-wave (~50%)
    // Small dips below 0.1V at zero crossings are expected with piecewise diode model
    QVERIFY2(positiveRatio > 0.85,
        qPrintable(QString("Full-wave shape: %1% positive (expected >85%%)")
            .arg(positiveRatio * 100, 0, 'f', 1)));

    qDebug() << QString("Without filter: V_peak=%1V  V_min=%2V  I_peak=%3mA  positive=%4%")
        .arg(vMax, 0, 'f', 4)
        .arg(vMin, 0, 'f', 4)
        .arg(iPeakMax * 1000, 0, 'f', 4)
        .arg(positiveRatio * 100, 0, 'f', 1);
}

void TestBridgeRectifier::withFilter()
{
    // Load circuit (AC source + 4 diodes bridge + R_load + C_filter)
    QString circuitPath = QString(TEST_DATA_DIR) + "/bridge_filtered.esim";
    auto circuit = loadCircuit(circuitPath);
    QVERIFY2(circuit != nullptr, "Failed to load bridge_filtered.esim");

    // Transient parameters: 50Hz → T=20ms, run 10 cycles = 200ms for cap steady state
    const double timeStep = 0.0001;  // 100µs
    const double totalTime = 0.2;    // 200ms (10 cycles for cap to reach steady state)

    // Run transient analysis
    TransientResult result = MNASolver::solveTransientFull(*circuit, timeStep, totalTime);
    QVERIFY2(result.success, qPrintable("Transient solve failed: " + result.errorMessage));

    // Find the output node
    int outNode = getOutputNode(*circuit);
    QVERIFY2(outNode > 0, "Could not find output node");

    // Extract V_out from the last 2 cycles (steady state)
    // At 50Hz, last 2 cycles = last 40ms → t > 160ms
    std::vector<double> vOut;
    for (const auto& frame : result.frames) {
        if (frame.time < 0.16) continue;
        auto it = frame.nodeVoltages.find(outNode);
        if (it != frame.nodeVoltages.end())
            vOut.push_back(it->second);
    }
    QVERIFY(!vOut.empty());

    double vMax = *std::max_element(vOut.begin(), vOut.end());
    double vMin = *std::min_element(vOut.begin(), vOut.end());
    double ripple = vMax - vMin;
    double vAvg = std::accumulate(vOut.begin(), vOut.end(), 0.0) / vOut.size();

    const double tolerance = 0.10; // 10% for filtered transient

    // Expected V_out peak ≈ 8.60V (same as without filter)
    const double expectedPeak = 8.60;
    double relErrPeak = std::abs(vMax - expectedPeak) / expectedPeak;
    QVERIFY2(relErrPeak < tolerance,
        qPrintable(QString("V_out peak: got %1, expected %2 (err %3%)")
            .arg(vMax, 0, 'f', 4)
            .arg(expectedPeak, 0, 'f', 4)
            .arg(relErrPeak * 100, 0, 'f', 2)));

    // V_out min: exponential discharge model
    // Full-wave → T_discharge = 1/(2f) = 10ms, RC = 4700 × 47e-6 = 0.2209s
    const double R = 4700.0;
    const double C = 47e-6;
    const double RC = R * C;
    const double T = 1.0 / (2.0 * 50.0); // 10ms for full-wave (100Hz)
    const double expectedMin = expectedPeak * std::exp(-T / RC); // ≈ 8.22V
    const double expectedRipple = expectedPeak - expectedMin;    // ≈ 0.38V

    QVERIFY2(vMin > 7.0,
        qPrintable(QString("V_out min should be well above 0 with filter: got %1V")
            .arg(vMin, 0, 'f', 4)));

    double relErrMin = std::abs(vMin - expectedMin) / expectedMin;
    QVERIFY2(relErrMin < tolerance,
        qPrintable(QString("V_out min: got %1, expected %2 (err %3%)")
            .arg(vMin, 0, 'f', 4)
            .arg(expectedMin, 0, 'f', 4)
            .arg(relErrMin * 100, 0, 'f', 2)));

    // Ripple voltage
    double relErrRipple = std::abs(ripple - expectedRipple) / expectedRipple;
    QVERIFY2(relErrRipple < tolerance,
        qPrintable(QString("Ripple: got %1V, expected %2V (err %3%)")
            .arg(ripple, 0, 'f', 4)
            .arg(expectedRipple, 0, 'f', 4)
            .arg(relErrRipple * 100, 0, 'f', 2)));

    // DC average
    const double expectedAvg = (expectedPeak + expectedMin) / 2.0;
    double relErrAvg = std::abs(vAvg - expectedAvg) / expectedAvg;
    QVERIFY2(relErrAvg < tolerance,
        qPrintable(QString("V_DC avg: got %1V, expected ~%2V (err %3%)")
            .arg(vAvg, 0, 'f', 4)
            .arg(expectedAvg, 0, 'f', 4)
            .arg(relErrAvg * 100, 0, 'f', 2)));

    // Ripple percentage
    double ripplePct = ripple / vAvg * 100.0;
    QVERIFY2(ripplePct < 10.0,
        qPrintable(QString("Ripple %% should be small: got %1%")
            .arg(ripplePct, 0, 'f', 1)));

    qDebug() << QString("With filter: V_peak=%1V  V_min=%2V  Ripple=%3V  V_avg=%4V  Ripple%%=%5%")
        .arg(vMax, 0, 'f', 4)
        .arg(vMin, 0, 'f', 4)
        .arg(ripple, 0, 'f', 4)
        .arg(vAvg, 0, 'f', 4)
        .arg(ripplePct, 0, 'f', 1);
}

QTEST_GUILESS_MAIN(TestBridgeRectifier)
#include "test_bridge_rectifier.moc"
