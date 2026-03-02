#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/ACSource.h"
#include "core/components/Resistor.h"
#include "core/components/Capacitor.h"
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

// ─── Analytical RC low-pass formulas ───
struct RCExpected {
    double frequency;
    double xC;          // capacitive reactance
    double impedance;   // |Z| = sqrt(R² + X_C²)
    double iPeak;       // V_peak / |Z|
    double vResistor;   // I × R
    double vCapacitor;  // I × X_C  (= V_out)
    double phase;       // atan(-X_C / R) in degrees
};

static RCExpected computeExpected(double vPeak, double R, double C, double freq)
{
    RCExpected e;
    e.frequency = freq;
    double omega = 2.0 * M_PI * freq;
    e.xC = 1.0 / (omega * C);
    e.impedance = std::sqrt(R * R + e.xC * e.xC);
    e.iPeak = vPeak / e.impedance;
    e.vResistor = e.iPeak * R;
    e.vCapacitor = e.iPeak * e.xC;
    e.phase = std::atan2(-e.xC, R) * 180.0 / M_PI;
    return e;
}

// ═══════════════════════════════════════════════════════
//  Test class
// ═══════════════════════════════════════════════════════

class TestACAnalysis : public QObject
{
    Q_OBJECT

private slots:
    void rcLowPass_200Hz();
    void rcLowPass_2kHz();
    void rcLowPass_8kHz();

private:
    void runRCLowPassTest(double frequency);
};

void TestACAnalysis::runRCLowPassTest(double frequency)
{
    // Load circuit
    QString circuitPath = QString(TEST_DATA_DIR) + "/rc_lowpass.esim";
    auto circuit = loadCircuit(circuitPath);
    QVERIFY2(circuit != nullptr, "Failed to load rc_lowpass.esim");

    // Find AC source and set the test frequency
    Component* acComp = findByType(*circuit, ComponentType::ACSource);
    QVERIFY2(acComp != nullptr, "ACSource not found in circuit");
    auto* acSource = static_cast<ACSource*>(acComp);
    acSource->setFrequency(frequency);

    // Circuit parameters
    const double vPeak = acSource->value();  // 5V
    Component* resistor = findByName(*circuit, "R1");
    Component* capacitor = findByName(*circuit, "C1");
    QVERIFY(resistor != nullptr);
    QVERIFY(capacitor != nullptr);
    const double R = resistor->value();   // 1000 Ω
    const double C = capacitor->value();  // 100nF

    // Compute analytical expected values
    RCExpected expected = computeExpected(vPeak, R, C, frequency);

    // Run AC analysis
    SimulationResult result = MNASolver::solveAC(*circuit, frequency);
    QVERIFY2(result.success, qPrintable("AC solve failed: " + result.errorMessage));

    // --- Verify results ---
    const double tolerance = 0.01; // 1% relative tolerance

    // Find branch results by component name
    BranchResult brR, brC;
    bool foundR = false, foundC = false;
    for (auto& [id, br] : result.branchResults) {
        Component* comp = circuit->component(id);
        if (!comp) continue;
        if (comp->name() == "R1") { brR = br; foundR = true; }
        if (comp->name() == "C1") { brC = br; foundC = true; }
    }
    QVERIFY2(foundR, "Resistor R1 not found in branch results");
    QVERIFY2(foundC, "Capacitor C1 not found in branch results");

    // Resistor voltage drop
    double relErrVR = std::abs(brR.voltageDrop - expected.vResistor) / expected.vResistor;
    QVERIFY2(relErrVR < tolerance,
        qPrintable(QString("V_R: got %1, expected %2 (err %3%)")
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

    // Capacitor voltage (= V_out)
    double relErrVC = std::abs(brC.voltageDrop - expected.vCapacitor) / expected.vCapacitor;
    QVERIFY2(relErrVC < tolerance,
        qPrintable(QString("V_C (V_out): got %1, expected %2 (err %3%)")
            .arg(brC.voltageDrop, 0, 'f', 6)
            .arg(expected.vCapacitor, 0, 'f', 6)
            .arg(relErrVC * 100, 0, 'f', 2)));

    // Capacitor current (should equal resistor current in series circuit)
    double relErrIC = std::abs(brC.current - expected.iPeak) / expected.iPeak;
    QVERIFY2(relErrIC < tolerance,
        qPrintable(QString("I_C: got %1, expected %2 (err %3%)")
            .arg(brC.current, 0, 'e', 6)
            .arg(expected.iPeak, 0, 'e', 6)
            .arg(relErrIC * 100, 0, 'f', 2)));

    // Print summary
    qDebug() << QString("f=%1Hz: V_R=%2V  V_C=%3V  I=%4A  |Z|=%5  X_C=%6")
        .arg(frequency, 0, 'f', 0)
        .arg(brR.voltageDrop, 0, 'f', 4)
        .arg(brC.voltageDrop, 0, 'f', 4)
        .arg(brR.current, 0, 'e', 4)
        .arg(expected.impedance, 0, 'f', 2)
        .arg(expected.xC, 0, 'f', 2);
}

void TestACAnalysis::rcLowPass_200Hz()
{
    runRCLowPassTest(200.0);
}

void TestACAnalysis::rcLowPass_2kHz()
{
    runRCLowPassTest(2000.0);
}

void TestACAnalysis::rcLowPass_8kHz()
{
    runRCLowPassTest(8000.0);
}

QTEST_GUILESS_MAIN(TestACAnalysis)
#include "test_ac_analysis.moc"
