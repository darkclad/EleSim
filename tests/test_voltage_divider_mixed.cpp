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

// ─── Helper: find component by name ───
static Component* findByName(Circuit& circuit, const QString& name)
{
    for (auto& [id, comp] : circuit.components()) {
        if (comp->name() == name) return comp.get();
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════
//  Voltage Divider (AC + DC) — Superposition Test
//
//  Circuit: DC 5V + AC 2Vpk (series) → R1 1kΩ → R2 2.2kΩ → GND
//  Divider ratio: R2/(R1+R2) = 2200/3200 = 0.6875
//
//  DC component:
//    I_DC  = 5 / 3200 = 1.5625 mA
//    V_R1  = 1.5625 V
//    V_R2  = V_out_DC = 3.4375 V
//
//  AC component:
//    I_AC  = 2 / 3200 = 0.625 mA
//    V_R1  = 0.625 V
//    V_R2  = V_out_AC = 1.375 V
//
//  Combined:
//    V_out_max = 3.4375 + 1.375 = 4.8125 V
//    V_out_min = 3.4375 - 1.375 = 2.0625 V
// ═══════════════════════════════════════════════════════

class TestVoltageDividerMixed : public QObject
{
    Q_OBJECT

private slots:
    void mixedAnalysis();
};

void TestVoltageDividerMixed::mixedAnalysis()
{
    // Load circuit
    QString circuitPath = QString(TEST_DATA_DIR) + "/voltage_divider_mixed.esim";
    auto circuit = loadCircuit(circuitPath);
    QVERIFY2(circuit != nullptr, "Failed to load voltage_divider_mixed.esim");

    // Circuit parameters
    const double V_DC = 5.0;
    const double V_AC = 2.0;
    const double R1 = 1000.0;
    const double R2 = 2200.0;
    const double ratio = R2 / (R1 + R2); // 0.6875
    const double acFreq = 50.0;

    // Expected values
    const double expectedIDC      = V_DC / (R1 + R2);          // 1.5625 mA
    const double expectedVR1DC    = expectedIDC * R1;           // 1.5625 V
    const double expectedVoutDC   = expectedIDC * R2;           // 3.4375 V
    const double expectedIAC      = V_AC / (R1 + R2);          // 0.625 mA
    const double expectedVR1AC    = expectedIAC * R1;           // 0.625 V
    const double expectedVoutAC   = expectedIAC * R2;           // 1.375 V
    const double expectedVoutMax  = expectedVoutDC + expectedVoutAC; // 4.8125 V
    const double expectedVoutMin  = expectedVoutDC - expectedVoutAC; // 2.0625 V

    // Run mixed (superposition) analysis
    SimulationResult result = MNASolver::solveMixed(*circuit, acFreq);
    QVERIFY2(result.success, qPrintable("Mixed solve failed: " + result.errorMessage));

    const double tolerance = 0.01; // 1%

    // Find branch results by component name
    BranchResult brR1, brR2;
    bool foundR1 = false, foundR2 = false;
    for (auto& [id, br] : result.branchResults) {
        Component* comp = circuit->component(id);
        if (!comp) continue;
        if (comp->name() == "R1") { brR1 = br; foundR1 = true; }
        if (comp->name() == "R2") { brR2 = br; foundR2 = true; }
    }
    QVERIFY2(foundR1, "R1 not found in branch results");
    QVERIFY2(foundR2, "R2 not found in branch results");

    // Verify isMixed flag
    QVERIFY2(brR1.isMixed, "R1 should have isMixed=true");
    QVERIFY2(brR2.isMixed, "R2 should have isMixed=true");

    // --- DC Component ---
    double relErr;

    relErr = std::abs(brR1.dcVoltage - expectedVR1DC) / expectedVR1DC;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("V_R1 DC: got %1, expected %2 (err %3%)")
            .arg(brR1.dcVoltage, 0, 'f', 4)
            .arg(expectedVR1DC, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    relErr = std::abs(brR2.dcVoltage - expectedVoutDC) / expectedVoutDC;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("V_out DC: got %1, expected %2 (err %3%)")
            .arg(brR2.dcVoltage, 0, 'f', 4)
            .arg(expectedVoutDC, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    relErr = std::abs(brR1.dcCurrent - expectedIDC) / expectedIDC;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("I_DC: got %1, expected %2 (err %3%)")
            .arg(brR1.dcCurrent, 0, 'e', 4)
            .arg(expectedIDC, 0, 'e', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    // --- AC Component ---
    relErr = std::abs(brR1.acVoltage - expectedVR1AC) / expectedVR1AC;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("V_R1 AC: got %1, expected %2 (err %3%)")
            .arg(brR1.acVoltage, 0, 'f', 4)
            .arg(expectedVR1AC, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    relErr = std::abs(brR2.acVoltage - expectedVoutAC) / expectedVoutAC;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("V_out AC: got %1, expected %2 (err %3%)")
            .arg(brR2.acVoltage, 0, 'f', 4)
            .arg(expectedVoutAC, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    relErr = std::abs(brR1.acCurrent - expectedIAC) / expectedIAC;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("I_AC: got %1, expected %2 (err %3%)")
            .arg(brR1.acCurrent, 0, 'e', 4)
            .arg(expectedIAC, 0, 'e', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    // --- Combined (total = DC + AC peak) ---
    relErr = std::abs(brR2.voltageDrop - expectedVoutMax) / expectedVoutMax;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("V_out max (DC+AC): got %1, expected %2 (err %3%)")
            .arg(brR2.voltageDrop, 0, 'f', 4)
            .arg(expectedVoutMax, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    // V_out minimum = DC - AC
    double actualVoutMin = brR2.dcVoltage - brR2.acVoltage;
    relErr = std::abs(actualVoutMin - expectedVoutMin) / expectedVoutMin;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("V_out min (DC-AC): got %1, expected %2 (err %3%)")
            .arg(actualVoutMin, 0, 'f', 4)
            .arg(expectedVoutMin, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    // Verify divider ratio
    double actualRatio = brR2.dcVoltage / (brR1.dcVoltage + brR2.dcVoltage);
    relErr = std::abs(actualRatio - ratio) / ratio;
    QVERIFY2(relErr < tolerance,
        qPrintable(QString("Divider ratio: got %1, expected %2 (err %3%)")
            .arg(actualRatio, 0, 'f', 4)
            .arg(ratio, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 2)));

    qDebug() << QString("DC: I=%1mA  V_R1=%2V  V_out=%3V")
        .arg(brR1.dcCurrent * 1000, 0, 'f', 4)
        .arg(brR1.dcVoltage, 0, 'f', 4)
        .arg(brR2.dcVoltage, 0, 'f', 4);
    qDebug() << QString("AC: I=%1mA  V_R1=%2V  V_out=%3V")
        .arg(brR1.acCurrent * 1000, 0, 'f', 4)
        .arg(brR1.acVoltage, 0, 'f', 4)
        .arg(brR2.acVoltage, 0, 'f', 4);
    qDebug() << QString("Combined: V_out_max=%1V  V_out_min=%2V  ratio=%3")
        .arg(brR2.voltageDrop, 0, 'f', 4)
        .arg(actualVoutMin, 0, 'f', 4)
        .arg(actualRatio, 0, 'f', 4);
}

QTEST_GUILESS_MAIN(TestVoltageDividerMixed)
#include "test_voltage_divider_mixed.moc"
