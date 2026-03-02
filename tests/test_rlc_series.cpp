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

// ─── Analytical Series RLC formulas ───
//
//  Series RLC:  AC -> R -> L -> C -> GND
//
//  X_L = ωL
//  X_C = 1/(ωC)
//  |Z| = sqrt(R² + (X_L - X_C)²)
//  I   = V_peak / |Z|
//  V_R = I × R
//  V_L = I × X_L
//  V_C = I × X_C
//  f₀  = 1/(2π√(LC))     (resonant frequency)
//  Q   = (1/R) × √(L/C)  (quality factor)
//
struct RLCExpected {
    double frequency;
    double xL;
    double xC;
    double impedance;
    double iPeak;
    double vResistor;
    double vInductor;
    double vCapacitor;
    double fResonant;
    double qFactor;
};

static RLCExpected computeExpected(double vPeak, double R, double L, double C, double freq)
{
    RLCExpected e;
    e.frequency = freq;
    double omega = 2.0 * M_PI * freq;
    e.xL = omega * L;
    e.xC = 1.0 / (omega * C);
    double xNet = e.xL - e.xC;
    e.impedance = std::sqrt(R * R + xNet * xNet);
    e.iPeak = vPeak / e.impedance;
    e.vResistor = e.iPeak * R;
    e.vInductor = e.iPeak * e.xL;
    e.vCapacitor = e.iPeak * e.xC;
    e.fResonant = 1.0 / (2.0 * M_PI * std::sqrt(L * C));
    e.qFactor = (1.0 / R) * std::sqrt(L / C);
    return e;
}

// ═══════════════════════════════════════════════════════
//  Test class
// ═══════════════════════════════════════════════════════

class TestRLCSeries : public QObject
{
    Q_OBJECT

private slots:
    void rlcSeries_1kHz();
    void rlcSeries_5kHz_resonance();
    void rlcSeries_15kHz();

private:
    void runRLCSeriesTest(double frequency);
};

void TestRLCSeries::runRLCSeriesTest(double frequency)
{
    // Load circuit
    QString circuitPath = QString(TEST_DATA_DIR) + "/rlc_series.esim";
    auto circuit = loadCircuit(circuitPath);
    QVERIFY2(circuit != nullptr, "Failed to load rlc_series.esim");

    // Find AC source and set the test frequency
    Component* acComp = findByType(*circuit, ComponentType::ACSource);
    QVERIFY2(acComp != nullptr, "ACSource not found in circuit");
    auto* acSource = static_cast<ACSource*>(acComp);
    acSource->setFrequency(frequency);

    // Circuit parameters
    const double vPeak = acSource->value();  // 5V
    Component* resistor = findByName(*circuit, "R1");
    Component* inductor = findByName(*circuit, "L1");
    Component* capacitor = findByName(*circuit, "C1");
    QVERIFY(resistor != nullptr);
    QVERIFY(inductor != nullptr);
    QVERIFY(capacitor != nullptr);
    const double R = resistor->value();   // 100 Ω
    const double L = inductor->value();   // 10mH
    const double C = capacitor->value();  // 100nF

    // Compute analytical expected values
    RLCExpected expected = computeExpected(vPeak, R, L, C, frequency);

    // Run AC analysis
    SimulationResult result = MNASolver::solveAC(*circuit, frequency);
    QVERIFY2(result.success, qPrintable("AC solve failed: " + result.errorMessage));

    // --- Verify results ---
    const double tolerance = 0.01; // 1% relative tolerance

    // Find branch results by component name
    BranchResult brR, brL, brC;
    bool foundR = false, foundL = false, foundC = false;
    for (auto& [id, br] : result.branchResults) {
        Component* comp = circuit->component(id);
        if (!comp) continue;
        if (comp->name() == "R1") { brR = br; foundR = true; }
        if (comp->name() == "L1") { brL = br; foundL = true; }
        if (comp->name() == "C1") { brC = br; foundC = true; }
    }
    QVERIFY2(foundR, "Resistor R1 not found in branch results");
    QVERIFY2(foundL, "Inductor L1 not found in branch results");
    QVERIFY2(foundC, "Capacitor C1 not found in branch results");

    // Resistor voltage drop
    double relErrVR = std::abs(brR.voltageDrop - expected.vResistor) / expected.vResistor;
    QVERIFY2(relErrVR < tolerance,
        qPrintable(QString("V_R: got %1, expected %2 (err %3%)")
            .arg(brR.voltageDrop, 0, 'f', 6)
            .arg(expected.vResistor, 0, 'f', 6)
            .arg(relErrVR * 100, 0, 'f', 2)));

    // Resistor current (= series current)
    double relErrI = std::abs(brR.current - expected.iPeak) / expected.iPeak;
    QVERIFY2(relErrI < tolerance,
        qPrintable(QString("I: got %1, expected %2 (err %3%)")
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

    // Capacitor voltage drop
    double relErrVC = std::abs(brC.voltageDrop - expected.vCapacitor) / expected.vCapacitor;
    QVERIFY2(relErrVC < tolerance,
        qPrintable(QString("V_C: got %1, expected %2 (err %3%)")
            .arg(brC.voltageDrop, 0, 'f', 6)
            .arg(expected.vCapacitor, 0, 'f', 6)
            .arg(relErrVC * 100, 0, 'f', 2)));

    // At resonance, verify special properties
    bool atResonance = std::abs(frequency - expected.fResonant) / expected.fResonant < 0.01;
    if (atResonance) {
        // At resonance: |Z| ≈ R, so V_R ≈ V_in
        double relErrZR = std::abs(brR.voltageDrop - vPeak) / vPeak;
        QVERIFY2(relErrZR < tolerance,
            qPrintable(QString("At resonance V_R should equal V_in: got %1, expected %2 (err %3%)")
                .arg(brR.voltageDrop, 0, 'f', 6)
                .arg(vPeak, 0, 'f', 6)
                .arg(relErrZR * 100, 0, 'f', 2)));

        // V_L ≈ V_C ≈ Q × V_in
        double expectedQV = expected.qFactor * vPeak;
        double relErrVLQ = std::abs(brL.voltageDrop - expectedQV) / expectedQV;
        QVERIFY2(relErrVLQ < tolerance,
            qPrintable(QString("At resonance V_L should be Q*V_in: got %1, expected %2 (err %3%)")
                .arg(brL.voltageDrop, 0, 'f', 6)
                .arg(expectedQV, 0, 'f', 6)
                .arg(relErrVLQ * 100, 0, 'f', 2)));
    }

    // Print summary
    qDebug() << QString("f=%1Hz: V_R=%2V  V_L=%3V  V_C=%4V  I=%5A  |Z|=%6  X_L=%7  X_C=%8  f0=%9Hz  Q=%10")
        .arg(frequency, 0, 'f', 0)
        .arg(brR.voltageDrop, 0, 'f', 4)
        .arg(brL.voltageDrop, 0, 'f', 4)
        .arg(brC.voltageDrop, 0, 'f', 4)
        .arg(brR.current, 0, 'e', 4)
        .arg(expected.impedance, 0, 'f', 2)
        .arg(expected.xL, 0, 'f', 2)
        .arg(expected.xC, 0, 'f', 2)
        .arg(expected.fResonant, 0, 'f', 1)
        .arg(expected.qFactor, 0, 'f', 2);
}

void TestRLCSeries::rlcSeries_1kHz()
{
    runRLCSeriesTest(1000.0);
}

void TestRLCSeries::rlcSeries_5kHz_resonance()
{
    // f₀ = 1/(2π√(LC)) = 1/(2π√(0.01 × 1e-7)) ≈ 5032.9 Hz
    // Use the exact resonant frequency for precision
    double fResonant = 1.0 / (2.0 * M_PI * std::sqrt(0.01 * 1e-7));
    runRLCSeriesTest(fResonant);
}

void TestRLCSeries::rlcSeries_15kHz()
{
    runRLCSeriesTest(15000.0);
}

QTEST_GUILESS_MAIN(TestRLCSeries)
#include "test_rlc_series.moc"
