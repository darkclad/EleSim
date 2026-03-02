#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/ZenerDiode.h"
#include "core/components/Resistor.h"
#include "simulation/MNASolver.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "."
#endif

static std::unique_ptr<Circuit> loadCircuit(const QString& filename)
{
    QString path = QString(TEST_DATA_DIR) + "/" + filename;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return Circuit::fromJson(doc.object());
}

static Component* findByName(Circuit& circuit, const QString& name)
{
    for (auto& [id, comp] : circuit.components()) {
        if (comp->name() == name) return comp.get();
    }
    return nullptr;
}

class TestZenerDiode : public QObject
{
    Q_OBJECT

private slots:
    // Test 1: Zener voltage regulator (DC, no load)
    // V1(12V) → R1(470Ω) → node_A ← ZD1(Vz=5.1V, cathode=A, anode=GND)
    // Zener clamps V_A = Vz = 5.1V
    // I_R1 = (12 - 5.1) / 470 = 14.68 mA
    void regulatorDC()
    {
        auto circuit = loadCircuit("zener_regulator_dc.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        // Get node_A: where R1 pin1 and ZD1 cathode (pin1) meet
        Component* r1 = findByName(*circuit, "R1");
        Component* zd1 = findByName(*circuit, "ZD1");
        QVERIFY(r1 != nullptr);
        QVERIFY(zd1 != nullptr);

        auto result = MNASolver::solveDC(*circuit);
        QVERIFY2(result.success, qPrintable(result.errorMessage));

        // Get voltage at node_A (R1 pin 1 = ZD1 cathode pin 1)
        int nodeA = r1->pin(1).nodeId;
        QVERIFY(nodeA > 0);
        double vA = result.nodeResults[nodeA].voltage;

        // Zener clamps at Vz = 5.1V
        // With R_on = 1Ω, the actual voltage is slightly above 5.1V
        // V_A = Vz + I_zd * R_on. I_zd ≈ (12 - 5.1) / 470 ≈ 14.68 mA
        // V_A ≈ 5.1 + 0.01468 ≈ 5.115V
        double expectedVA = 5.1;
        QVERIFY2(std::abs(vA - expectedVA) < 0.1,
                 qPrintable(QString("V_A: got %1 V, expected ~%2 V").arg(vA).arg(expectedVA)));

        // Check current through R1
        double iR1 = (12.0 - vA) / 470.0;
        double expectedI = (12.0 - 5.1) / 470.0; // ≈ 14.68 mA
        QVERIFY2(std::abs(iR1 - expectedI) < 0.001,
                 qPrintable(QString("I_R1: got %1 A, expected ~%2 A").arg(iR1).arg(expectedI)));
    }

    // Test 2: Zener regulator with load
    // V1(15V) → R1(220Ω) → node_A ← ZD1(Vz=5.1V) ∥ R_load(1kΩ) → GND
    // V_A = 5.1V, I_load = 5.1/1000 = 5.1 mA
    // I_R1 = (15-5.1)/220 ≈ 45 mA, I_ZD = I_R1 - I_load ≈ 39.9 mA
    void regulatorWithLoad()
    {
        auto circuit = loadCircuit("zener_regulator_loaded.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        Component* r1 = findByName(*circuit, "R1");
        Component* rLoad = findByName(*circuit, "R_load");
        Component* zd1 = findByName(*circuit, "ZD1");
        QVERIFY(r1 != nullptr);
        QVERIFY(rLoad != nullptr);
        QVERIFY(zd1 != nullptr);

        auto result = MNASolver::solveDC(*circuit);
        QVERIFY2(result.success, qPrintable(result.errorMessage));

        // Get voltage at node_A
        int nodeA = r1->pin(1).nodeId;
        QVERIFY(nodeA > 0);
        double vA = result.nodeResults[nodeA].voltage;

        // Zener clamps at ~5.1V
        QVERIFY2(std::abs(vA - 5.1) < 0.2,
                 qPrintable(QString("V_A: got %1 V, expected ~5.1 V").arg(vA)));

        // Load current: I_load = V_A / R_load
        double iLoad = vA / 1000.0;
        QVERIFY2(std::abs(iLoad - 0.0051) < 0.001,
                 qPrintable(QString("I_load: got %1 A, expected ~5.1 mA").arg(iLoad)));

        // Series resistor current: I_R1 = (15 - V_A) / 220
        double iR1 = (15.0 - vA) / 220.0;
        double expectedIR1 = (15.0 - 5.1) / 220.0; // ≈ 45 mA
        QVERIFY2(std::abs(iR1 - expectedIR1) < 0.002,
                 qPrintable(QString("I_R1: got %1 A, expected ~%2 A").arg(iR1).arg(expectedIR1)));

        // KCL: I_R1 ≈ I_ZD + I_load (all within tolerance)
        double iZD = iR1 - iLoad;
        QVERIFY2(iZD > 0.0,
                 qPrintable(QString("I_ZD should be positive: got %1 A").arg(iZD)));
    }

    // Test 3: AC clamping with Zener (transient)
    // V_AC(10V, 50Hz) → R1(1kΩ) → node_A ← ZD1(Vz=5.1V, cathode=A, anode=GND)
    // Positive half: Zener breakdown clamps at ~+5.1V
    // Negative half: Zener forward biases, clamps at ~-0.7V
    void acClamp()
    {
        auto circuit = loadCircuit("zener_clamp_ac.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        Component* r1 = findByName(*circuit, "R1");
        QVERIFY(r1 != nullptr);
        int nodeA = r1->pin(1).nodeId;
        QVERIFY(nodeA > 0);

        // Run transient for 2 full cycles at 50Hz (40ms)
        double timeStep = 0.0001;  // 100µs
        double totalTime = 0.04;    // 40ms = 2 cycles

        auto tResult = MNASolver::solveTransientFull(*circuit, timeStep, totalTime);
        QVERIFY2(tResult.success, qPrintable(tResult.errorMessage));
        QVERIFY(tResult.frames.size() > 100);

        // Analyze second cycle (steady state): skip first 20ms
        double vPeakPos = -1000.0;
        double vPeakNeg = 1000.0;
        int steadyStart = static_cast<int>(tResult.frames.size()) / 2;

        for (int i = steadyStart; i < static_cast<int>(tResult.frames.size()); ++i) {
            auto& frame = tResult.frames[i];
            double vA = frame.nodeVoltages[nodeA];
            if (vA > vPeakPos) vPeakPos = vA;
            if (vA < vPeakNeg) vPeakNeg = vA;
        }

        // Positive peak should be clamped near Vz = 5.1V
        // (with R_on=1Ω and R_series=1kΩ, actual clamp ≈ 5.1V + small offset)
        QVERIFY2(std::abs(vPeakPos - 5.1) < 0.5,
                 qPrintable(QString("V_peak_pos: got %1 V, expected ~5.1 V").arg(vPeakPos)));

        // Negative peak should be clamped near -Vf = -0.7V
        QVERIFY2(std::abs(vPeakNeg - (-0.7)) < 0.5,
                 qPrintable(QString("V_peak_neg: got %1 V, expected ~-0.7 V").arg(vPeakNeg)));
    }

    // Test 4: Save/load roundtrip
    // Load circuit with ZD1(Vf=0.7, Vz=3.3), serialize to JSON, reload, verify params
    void saveLoadRoundtrip()
    {
        auto circuit = loadCircuit("zener_roundtrip.esim");
        QVERIFY(circuit != nullptr);

        // Find ZD1 and verify initial params
        Component* zd1 = findByName(*circuit, "ZD1");
        QVERIFY(zd1 != nullptr);
        QCOMPARE(zd1->type(), ComponentType::ZenerDiode);

        auto* zener = static_cast<ZenerDiode*>(zd1);
        QVERIFY2(std::abs(zener->forwardVoltage() - 0.7) < 0.001,
                 qPrintable(QString("Vf: got %1, expected 0.7").arg(zener->forwardVoltage())));
        QVERIFY2(std::abs(zener->zenerVoltage() - 3.3) < 0.001,
                 qPrintable(QString("Vz: got %1, expected 3.3").arg(zener->zenerVoltage())));

        // Serialize to JSON
        QJsonObject json = circuit->toJson();

        // Reload from JSON
        auto circuit2 = Circuit::fromJson(json);
        QVERIFY(circuit2 != nullptr);

        // Find ZD1 in reloaded circuit
        Component* zd2 = findByName(*circuit2, "ZD1");
        QVERIFY(zd2 != nullptr);
        QCOMPARE(zd2->type(), ComponentType::ZenerDiode);

        auto* zener2 = static_cast<ZenerDiode*>(zd2);
        QVERIFY2(std::abs(zener2->forwardVoltage() - 0.7) < 0.001,
                 qPrintable(QString("Roundtrip Vf: got %1, expected 0.7").arg(zener2->forwardVoltage())));
        QVERIFY2(std::abs(zener2->zenerVoltage() - 3.3) < 0.001,
                 qPrintable(QString("Roundtrip Vz: got %1, expected 3.3").arg(zener2->zenerVoltage())));
    }
};

QTEST_GUILESS_MAIN(TestZenerDiode)
#include "test_zener_diode.moc"
