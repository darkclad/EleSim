#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/ACSource.h"
#include "core/components/Resistor.h"
#include "core/components/Lamp.h"
#include "simulation/MNASolver.h"

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
//  AC Source + Resistor + Lamp series loop
//
//  Circuit:  V1 (100V, 50Hz) → R1 (100Ω) → L1 (Lamp 100Ω) → V1
//
//  Analytical:
//    Total Z = R1 + R_lamp = 200Ω  (purely resistive)
//    I = V / Z = 100 / 200 = 0.5 A
//    V_R1 = I × R1 = 50 V
//    V_lamp = I × R_lamp = 50 V
//    Phase = 0° (resistive circuit)
// ═══════════════════════════════════════════════════════

class TestACResistorLamp : public QObject
{
    Q_OBJECT

private slots:
    void seriesLoop_50Hz();
};

void TestACResistorLamp::seriesLoop_50Hz()
{
    QString circuitPath = QString(TEST_DATA_DIR) + "/ac_resistor_lamp_loop.esim";
    auto circuit = loadCircuit(circuitPath);
    QVERIFY2(circuit != nullptr, "Failed to load ac_resistor_lamp_loop.esim");

    Component* r1 = findByName(*circuit, "R1");
    Component* lamp = findByName(*circuit, "L1");
    Component* ac = findByName(*circuit, "V1");
    QVERIFY(r1 && lamp && ac);

    const double V = ac->value();       // 100 V
    const double R1 = r1->value();      // 100 Ω
    const double R_lamp = lamp->value(); // 100 Ω
    const double freq = 50.0;

    // Analytical expectations
    const double Z_total = R1 + R_lamp;         // 200 Ω
    const double I_expected = V / Z_total;      // 0.5 A
    const double V_R1_expected = I_expected * R1;       // 50 V
    const double V_lamp_expected = I_expected * R_lamp; // 50 V

    // Run AC analysis
    SimulationResult result = MNASolver::solveAC(*circuit, freq);
    QVERIFY2(result.success, qPrintable("AC solve failed: " + result.errorMessage));

    // Find branch results
    BranchResult brR1, brLamp;
    bool foundR1 = false, foundLamp = false;
    for (auto& [id, br] : result.branchResults) {
        Component* comp = circuit->component(id);
        if (!comp) continue;
        if (comp->name() == "R1")  { brR1 = br; foundR1 = true; }
        if (comp->name() == "L1")  { brLamp = br; foundLamp = true; }
    }
    QVERIFY2(foundR1, "R1 not found in branch results");
    QVERIFY2(foundLamp, "Lamp L1 not found in branch results");

    const double tol = 0.01; // 1%

    // Verify resistor voltage
    double errVR = std::abs(brR1.voltageDrop - V_R1_expected) / V_R1_expected;
    QVERIFY2(errVR < tol,
        qPrintable(QString("V_R1: got %1, expected %2 (err %3%)")
            .arg(brR1.voltageDrop, 0, 'f', 4)
            .arg(V_R1_expected, 0, 'f', 4)
            .arg(errVR * 100, 0, 'f', 2)));

    // Verify lamp voltage
    double errVL = std::abs(brLamp.voltageDrop - V_lamp_expected) / V_lamp_expected;
    QVERIFY2(errVL < tol,
        qPrintable(QString("V_Lamp: got %1, expected %2 (err %3%)")
            .arg(brLamp.voltageDrop, 0, 'f', 4)
            .arg(V_lamp_expected, 0, 'f', 4)
            .arg(errVL * 100, 0, 'f', 2)));

    // Verify current through both (series — must be equal)
    double errI_R = std::abs(brR1.current - I_expected) / I_expected;
    QVERIFY2(errI_R < tol,
        qPrintable(QString("I_R1: got %1, expected %2 (err %3%)")
            .arg(brR1.current, 0, 'e', 4)
            .arg(I_expected, 0, 'e', 4)
            .arg(errI_R * 100, 0, 'f', 2)));

    double errI_L = std::abs(brLamp.current - I_expected) / I_expected;
    QVERIFY2(errI_L < tol,
        qPrintable(QString("I_Lamp: got %1, expected %2 (err %3%)")
            .arg(brLamp.current, 0, 'e', 4)
            .arg(I_expected, 0, 'e', 4)
            .arg(errI_L * 100, 0, 'f', 2)));

    qDebug() << QString("AC Resistor+Lamp loop @ %1Hz: V_R1=%2V  V_Lamp=%3V  I=%4A  Z=%5Ω")
        .arg(freq, 0, 'f', 0)
        .arg(brR1.voltageDrop, 0, 'f', 4)
        .arg(brLamp.voltageDrop, 0, 'f', 4)
        .arg(brR1.current, 0, 'f', 4)
        .arg(Z_total, 0, 'f', 0);
}

QTEST_GUILESS_MAIN(TestACResistorLamp)
#include "test_ac_resistor_lamp.moc"
