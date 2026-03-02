#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/DCCurrentSource.h"
#include "core/components/Resistor.h"
#include "simulation/MNASolver.h"

// ─── Helpers ────────────────────────────────────────────
static std::unique_ptr<Circuit> loadCircuit(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return Circuit::fromJson(doc.object());
}

static Component* findByName(Circuit& circuit, const QString& name)
{
    for (auto& [id, comp] : circuit.components())
        if (comp->name() == name) return comp.get();
    return nullptr;
}

// ═══════════════════════════════════════════════════════
//  DC Current Source — 4 real-circuit tests
// ═══════════════════════════════════════════════════════

class TestCurrentSource : public QObject
{
    Q_OBJECT

private slots:
    void singleResistorLoad();
    void currentDivider();
    void ladderNetwork();
    void superpositionWithVoltageSource();
};

// ─── Test 1 ─────────────────────────────────────────────
//  Circuit file: current_source_single_r.esim
//  I1(10 mA) → R1(1 kΩ) → GND
//
//  Ohm's law:
//      V_A = I × R = 10 mA × 1 kΩ = 10 V
//      I_R  = V_A / R = 10 mA
// ─────────────────────────────────────────────────────────
void TestCurrentSource::singleResistorLoad()
{
    QString path = QString(TEST_DATA_DIR) + "/current_source_single_r.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load current_source_single_r.esim");

    SimulationResult result = MNASolver::solveDC(*circuit);
    QVERIFY2(result.success, qPrintable("DC solve failed: " + result.errorMessage));

    Component* iSrc = findByName(*circuit, "I1");
    Component* r1   = findByName(*circuit, "R1");
    QVERIFY(iSrc != nullptr);
    QVERIFY(r1 != nullptr);

    // ── Verify node voltage: V_A = I × R = 10 V ──
    const double expectedV = iSrc->value() * r1->value();   // 0.01 × 1000 = 10 V
    int nodeA = iSrc->pin(0).nodeId;
    QVERIFY2(nodeA > 0, "I1 pin0 not connected");
    double vA = result.nodeResults[nodeA].voltage;

    double relErr = std::abs(vA - expectedV) / expectedV;
    QVERIFY2(relErr < 0.01, qPrintable(
        QString("V_A: got %1 V, expected %2 V (err %3%)")
            .arg(vA, 0, 'f', 4).arg(expectedV, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    // ── Verify resistor current: I_R = 10 mA ──
    double iR = result.branchResults[r1->id()].current;
    double relErrI = std::abs(iR - iSrc->value()) / iSrc->value();
    QVERIFY2(relErrI < 0.01, qPrintable(
        QString("I_R: got %1 A, expected %2 A (err %3%)")
            .arg(iR, 0, 'e', 4).arg(iSrc->value(), 0, 'e', 4)
            .arg(relErrI * 100, 0, 'f', 2)));

    qDebug() << QString("Test 1 — Single R: V_A = %1 V, I_R = %2 A")
        .arg(vA, 0, 'f', 4).arg(iR, 0, 'e', 4);
}

// ─── Test 2 ─────────────────────────────────────────────
//  Circuit file: current_source_divider.esim
//  I1(10 mA) → R1(1 kΩ) ∥ R2(2 kΩ) → GND
//
//  Parallel resistance:
//      R_par = (R1 × R2) / (R1 + R2) = 666.67 Ω
//  Node voltage:
//      V_A = I × R_par = 6.667 V
//  Current divider rule:
//      I_R1 = V_A / R1 = 6.667 mA
//      I_R2 = V_A / R2 = 3.333 mA
//  KCL check: I_R1 + I_R2 = 10 mA ✓
// ─────────────────────────────────────────────────────────
void TestCurrentSource::currentDivider()
{
    QString path = QString(TEST_DATA_DIR) + "/current_source_divider.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load current_source_divider.esim");

    SimulationResult result = MNASolver::solveDC(*circuit);
    QVERIFY2(result.success, qPrintable("DC solve failed: " + result.errorMessage));

    Component* iSrc = findByName(*circuit, "I1");
    Component* r1   = findByName(*circuit, "R1");
    Component* r2   = findByName(*circuit, "R2");
    QVERIFY(iSrc && r1 && r2);

    // ── Analytical values ──
    double R1 = r1->value(), R2 = r2->value(), I = iSrc->value();
    const double R_par = (R1 * R2) / (R1 + R2);    // 666.667 Ω
    const double expectedV   = I * R_par;            // 6.6667 V
    const double expectedIR1 = expectedV / R1;       // 6.6667 mA
    const double expectedIR2 = expectedV / R2;       // 3.3333 mA

    int nodeA = iSrc->pin(0).nodeId;
    double vA = result.nodeResults[nodeA].voltage;

    double relErr = std::abs(vA - expectedV) / expectedV;
    QVERIFY2(relErr < 0.01, qPrintable(
        QString("V_A: got %1 V, expected %2 V (err %3%)")
            .arg(vA, 0, 'f', 4).arg(expectedV, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    // ── Verify branch currents ──
    double iR1 = result.branchResults[r1->id()].current;
    double iR2 = result.branchResults[r2->id()].current;

    QVERIFY2(std::abs(iR1 - expectedIR1) / expectedIR1 < 0.01, qPrintable(
        QString("I_R1: got %1 A, expected %2 A").arg(iR1, 0, 'e', 4).arg(expectedIR1, 0, 'e', 4)));
    QVERIFY2(std::abs(iR2 - expectedIR2) / expectedIR2 < 0.01, qPrintable(
        QString("I_R2: got %1 A, expected %2 A").arg(iR2, 0, 'e', 4).arg(expectedIR2, 0, 'e', 4)));

    // ── KCL: I_R1 + I_R2 = I_source ──
    double totalI = iR1 + iR2;
    QVERIFY2(std::abs(totalI - I) / I < 0.01, qPrintable(
        QString("KCL: I_R1 + I_R2 = %1 A, expected %2 A").arg(totalI, 0, 'e', 4).arg(I, 0, 'e', 4)));

    qDebug() << QString("Test 2 — Current divider: V_A = %1 V, I_R1 = %2 A, I_R2 = %3 A")
        .arg(vA, 0, 'f', 4).arg(iR1, 0, 'e', 4).arg(iR2, 0, 'e', 4);
}

// ─── Test 3 ─────────────────────────────────────────────
//  Circuit file: current_source_ladder.esim
//  Ladder network:
//
//      I1(5 mA) → R1(1 kΩ) → ┬─ R2(1 kΩ) → GND
//                   (A)       │    (B)
//                             └─ R3(2 kΩ) → GND
//
//  KCL at node B:
//      I = V_B × (1/R2 + 1/R3) = V_B × 3/2000
//      V_B = 10/3 ≈ 3.333 V
//  KVL through R1:
//      V_A = V_B + I × R1 = 25/3 ≈ 8.333 V
//  KCL check: I_R2 + I_R3 = 5 mA ✓
// ─────────────────────────────────────────────────────────
void TestCurrentSource::ladderNetwork()
{
    QString path = QString(TEST_DATA_DIR) + "/current_source_ladder.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load current_source_ladder.esim");

    SimulationResult result = MNASolver::solveDC(*circuit);
    QVERIFY2(result.success, qPrintable("DC solve failed: " + result.errorMessage));

    Component* iSrc = findByName(*circuit, "I1");
    Component* r1   = findByName(*circuit, "R1");
    Component* r2   = findByName(*circuit, "R2");
    Component* r3   = findByName(*circuit, "R3");
    QVERIFY(iSrc && r1 && r2 && r3);

    double I = iSrc->value();           // 5 mA
    double R1 = r1->value();            // 1 kΩ
    double R2 = r2->value();            // 1 kΩ
    double R3 = r3->value();            // 2 kΩ

    // ── Analytical values ──
    const double expectedVB  = I / (1.0/R2 + 1.0/R3);   // 10/3 V
    const double expectedVA  = expectedVB + I * R1;       // 25/3 V
    const double expectedIR2 = expectedVB / R2;            // 3.333 mA
    const double expectedIR3 = expectedVB / R3;            // 1.667 mA

    // ── Verify node A ──
    int nodeA = iSrc->pin(0).nodeId;
    QVERIFY2(nodeA > 0, "I1 pin0 not connected");
    double vA = result.nodeResults[nodeA].voltage;

    double relErrA = std::abs(vA - expectedVA) / expectedVA;
    QVERIFY2(relErrA < 0.01, qPrintable(
        QString("V_A: got %1 V, expected %2 V (err %3%)")
            .arg(vA, 0, 'f', 4).arg(expectedVA, 0, 'f', 4)
            .arg(relErrA * 100, 0, 'f', 2)));

    // ── Verify node B ──
    int nodeB = r1->pin(1).nodeId;
    QVERIFY2(nodeB > 0, "R1 pin1 not connected");
    double vB = result.nodeResults[nodeB].voltage;

    double relErrB = std::abs(vB - expectedVB) / expectedVB;
    QVERIFY2(relErrB < 0.01, qPrintable(
        QString("V_B: got %1 V, expected %2 V (err %3%)")
            .arg(vB, 0, 'f', 4).arg(expectedVB, 0, 'f', 4)
            .arg(relErrB * 100, 0, 'f', 2)));

    // ── Verify branch currents ──
    double iR2 = result.branchResults[r2->id()].current;
    double iR3 = result.branchResults[r3->id()].current;

    QVERIFY2(std::abs(iR2 - expectedIR2) / expectedIR2 < 0.01, qPrintable(
        QString("I_R2: got %1 A, expected %2 A").arg(iR2, 0, 'e', 4).arg(expectedIR2, 0, 'e', 4)));
    QVERIFY2(std::abs(iR3 - expectedIR3) / expectedIR3 < 0.01, qPrintable(
        QString("I_R3: got %1 A, expected %2 A").arg(iR3, 0, 'e', 4).arg(expectedIR3, 0, 'e', 4)));

    // ── KCL at node B ──
    double totalI = iR2 + iR3;
    QVERIFY2(std::abs(totalI - I) / I < 0.01, qPrintable(
        QString("KCL: I_R2 + I_R3 = %1 A, expected %2 A").arg(totalI, 0, 'e', 4).arg(I, 0, 'e', 4)));

    qDebug() << QString("Test 3 — Ladder: V_A = %1 V, V_B = %2 V, I_R2 = %3 A, I_R3 = %4 A")
        .arg(vA, 0, 'f', 4).arg(vB, 0, 'f', 4)
        .arg(iR2, 0, 'e', 4).arg(iR3, 0, 'e', 4);
}

// ─── Test 4 ─────────────────────────────────────────────
//  Circuit file: current_source_superposition.esim
//  Superposition — DC voltage source + DC current source
//
//      V1(12 V)──R1(1 kΩ)──┬──nodeA
//                           │
//                       R2(2 kΩ)    I1(5 mA) ↑
//                           │              │
//                          GND            GND
//
//  KCL at node A:
//      (12 − V_A)/R1 + I1 = V_A/R2
//      V_A = 34/3 ≈ 11.333 V
//
//  I_R1 = (12 − V_A)/R1 ≈ 0.667 mA
//  I_R2 = V_A/R2          ≈ 5.667 mA
//  KCL check: I_R1 + I1 = I_R2 ✓
// ─────────────────────────────────────────────────────────
void TestCurrentSource::superpositionWithVoltageSource()
{
    QString path = QString(TEST_DATA_DIR) + "/current_source_superposition.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load current_source_superposition.esim");

    SimulationResult result = MNASolver::solveDC(*circuit);
    QVERIFY2(result.success, qPrintable("DC solve failed: " + result.errorMessage));

    Component* vSrc = findByName(*circuit, "V1");
    Component* iSrc = findByName(*circuit, "I1");
    Component* r1   = findByName(*circuit, "R1");
    Component* r2   = findByName(*circuit, "R2");
    QVERIFY(vSrc && iSrc && r1 && r2);

    double V = vSrc->value();   // 12 V
    double I = iSrc->value();   // 5 mA
    double R1 = r1->value();    // 1 kΩ
    double R2 = r2->value();    // 2 kΩ

    // ── Analytical: V_A = (V/R1 + I) / (1/R1 + 1/R2) ──
    const double expectedVA  = (V/R1 + I) / (1.0/R1 + 1.0/R2);    // 34/3 V
    const double expectedIR1 = (V - expectedVA) / R1;               // 0.667 mA
    const double expectedIR2 = expectedVA / R2;                      // 5.667 mA

    // ── Verify node A voltage ──
    int nodeA = r1->pin(1).nodeId;
    QVERIFY2(nodeA > 0, "R1 pin1 not connected");
    double vA = result.nodeResults[nodeA].voltage;

    double relErr = std::abs(vA - expectedVA) / expectedVA;
    QVERIFY2(relErr < 0.01, qPrintable(
        QString("V_A: got %1 V, expected %2 V (err %3%)")
            .arg(vA, 0, 'f', 4).arg(expectedVA, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    // ── Verify R2 current ──
    double iR2 = result.branchResults[r2->id()].current;
    QVERIFY2(std::abs(iR2 - expectedIR2) / expectedIR2 < 0.01, qPrintable(
        QString("I_R2: got %1 A, expected %2 A").arg(iR2, 0, 'e', 4).arg(expectedIR2, 0, 'e', 4)));

    // ── KCL at node A: I_R1 + I1 = I_R2 ──
    double iR1 = result.branchResults[r1->id()].current;
    double kclLeft = iR1 + I;
    double kclErr = std::abs(kclLeft - iR2) / iR2;
    QVERIFY2(kclErr < 0.02, qPrintable(
        QString("KCL: I_R1(%1) + I1(%2) = %3 A vs I_R2 = %4 A")
            .arg(iR1, 0, 'e', 4).arg(I, 0, 'e', 4)
            .arg(kclLeft, 0, 'e', 4).arg(iR2, 0, 'e', 4)));

    qDebug() << QString("Test 4 — Superposition: V_A = %1 V, I_R1 = %2 A, I_R2 = %3 A")
        .arg(vA, 0, 'f', 4).arg(iR1, 0, 'e', 4).arg(iR2, 0, 'e', 4);
}

QTEST_GUILESS_MAIN(TestCurrentSource)
#include "test_current_source.moc"
