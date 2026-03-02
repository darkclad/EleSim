#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

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

// ─── Helper: find component by type ───
static Component* findByType(Circuit& circuit, ComponentType type)
{
    for (auto& [id, comp] : circuit.components()) {
        if (comp->type() == type) return comp.get();
    }
    return nullptr;
}

// ─── Helper: find component by name ───
static Component* findByName(Circuit& circuit, const QString& name)
{
    for (auto& [id, comp] : circuit.components()) {
        if (comp->name() == name) return comp.get();
    }
    return nullptr;
}

// ─── Analytical RL high-pass formulas ───
//
//  Series RL:  AC -> L -> R -> GND
//  V_out = voltage across R
//
//  X_L = ωL = 2πfL
//  |Z| = sqrt(R² + X_L²)
//  I   = V_peak / |Z|
//  V_R = I × R    (output)
//  V_L = I × X_L
//
struct RLExpected {
    double frequency;
    double xL;          // inductive reactance
    double impedance;   // |Z| total
    double iPeak;       // peak current
    double vResistor;   // V_out = I × R
    double vInductor;   // V_L = I × X_L
};

static RLExpected computeExpected(double vPeak, double R, double L, double freq)
{
    RLExpected e;
    e.frequency = freq;
    double omega = 2.0 * M_PI * freq;
    e.xL = omega * L;
    e.impedance = std::sqrt(R * R + e.xL * e.xL);
    e.iPeak = vPeak / e.impedance;
    e.vResistor = e.iPeak * R;
    e.vInductor = e.iPeak * e.xL;
    return e;
}

// ═══════════════════════════════════════════════════════
//  Test class
// ═══════════════════════════════════════════════════════

class TestRLHighPass : public QObject
{
    Q_OBJECT

private slots:
    void rlHighPass_200Hz();
    void rlHighPass_2kHz();
    void rlHighPass_8kHz();

private:
    void runRLHighPassTest(double frequency);
};

void TestRLHighPass::runRLHighPassTest(double frequency)
{
    // Load circuit
    QString circuitPath = QString(TEST_DATA_DIR) + "/rl_highpass.esim";
    auto circuit = loadCircuit(circuitPath);
    QVERIFY2(circuit != nullptr, "Failed to load rl_highpass.esim");

    // Find AC source and set the test frequency
    Component* acComp = findByType(*circuit, ComponentType::ACSource);
    QVERIFY2(acComp != nullptr, "ACSource not found in circuit");
    auto* acSource = static_cast<ACSource*>(acComp);
    acSource->setFrequency(frequency);

    // Circuit parameters
    const double vPeak = acSource->value();  // 5V
    Component* inductor = findByName(*circuit, "L1");
    Component* resistor = findByName(*circuit, "R1");
    QVERIFY(inductor != nullptr);
    QVERIFY(resistor != nullptr);
    const double L = inductor->value();  // 10mH
    const double R = resistor->value();  // 100 Ω

    // Compute analytical expected values
    RLExpected expected = computeExpected(vPeak, R, L, frequency);

    // Run AC analysis
    SimulationResult result = MNASolver::solveAC(*circuit, frequency);
    QVERIFY2(result.success, qPrintable("AC solve failed: " + result.errorMessage));

    // --- Verify results ---
    const double tolerance = 0.01; // 1% relative tolerance

    // Find branch results by component name
    BranchResult brR, brL;
    bool foundR = false, foundL = false;
    for (auto& [id, br] : result.branchResults) {
        Component* comp = circuit->component(id);
        if (!comp) continue;
        if (comp->name() == "R1") { brR = br; foundR = true; }
        if (comp->name() == "L1") { brL = br; foundL = true; }
    }
    QVERIFY2(foundR, "Resistor R1 not found in branch results");
    QVERIFY2(foundL, "Inductor L1 not found in branch results");

    // Resistor voltage drop (= V_out)
    double relErrVR = std::abs(brR.voltageDrop - expected.vResistor) / expected.vResistor;
    QVERIFY2(relErrVR < tolerance,
        qPrintable(QString("V_R (V_out): got %1, expected %2 (err %3%)")
            .arg(brR.voltageDrop, 0, 'f', 6)
            .arg(expected.vResistor, 0, 'f', 6)
            .arg(relErrVR * 100, 0, 'f', 2)));

    // Resistor current
    double relErrI = std::abs(brR.current - expected.iPeak) / expected.iPeak;
    QVERIFY2(relErrI < tolerance,
        qPrintable(QString("I_R: got %1, expected %2 (err %3%)")
            .arg(brR.current, 0, 'e', 6)
            .arg(expected.iPeak, 0, 'e', 6)
            .arg(relErrI * 100, 0, 'f', 2)));

    // Inductor voltage drop
    double relErrVL = std::abs(brL.voltageDrop - expected.vInductor) / expected.vInductor;
    QVERIFY2(relErrVL < tolerance,
        qPrintable(QString("V_L: got %1, expected %2 (err %3%)")
            .arg(brL.voltageDrop, 0, 'f', 6)
            .arg(expected.vInductor, 0, 'f', 6)
            .arg(relErrVL * 100, 0, 'f', 2)));

    // Inductor current (should equal resistor current in series circuit)
    double relErrIL = std::abs(brL.current - expected.iPeak) / expected.iPeak;
    QVERIFY2(relErrIL < tolerance,
        qPrintable(QString("I_L: got %1, expected %2 (err %3%)")
            .arg(brL.current, 0, 'e', 6)
            .arg(expected.iPeak, 0, 'e', 6)
            .arg(relErrIL * 100, 0, 'f', 2)));

    // Print summary
    qDebug() << QString("f=%1Hz: V_R(out)=%2V  V_L=%3V  I=%4A  |Z|=%5  X_L=%6")
        .arg(frequency, 0, 'f', 0)
        .arg(brR.voltageDrop, 0, 'f', 4)
        .arg(brL.voltageDrop, 0, 'f', 4)
        .arg(brR.current, 0, 'e', 4)
        .arg(expected.impedance, 0, 'f', 2)
        .arg(expected.xL, 0, 'f', 2);
}

void TestRLHighPass::rlHighPass_200Hz()
{
    runRLHighPassTest(200.0);
}

void TestRLHighPass::rlHighPass_2kHz()
{
    runRLHighPassTest(2000.0);
}

void TestRLHighPass::rlHighPass_8kHz()
{
    runRLHighPassTest(8000.0);
}

QTEST_GUILESS_MAIN(TestRLHighPass)
#include "test_rl_highpass.moc"
