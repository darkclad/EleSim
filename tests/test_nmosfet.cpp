#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/NMosfet.h"
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

class TestNMosfet : public QObject
{
    Q_OBJECT

private slots:
    // Test 1: NMOS switch ON (DC)
    // V1(10V) → R1(1kΩ) → Drain ← NMOS(Vth=2V) → Source → GND
    // Gate tied to V_gate(5V) → V_GS = 5V > Vth=2V → ON
    // NMOS acts as R_on=1Ω, so V_drain ≈ 10 * 1/(1000+1) ≈ 0.01V
    void switchOn()
    {
        auto circuit = loadCircuit("nmosfet_switch_dc.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        Component* r1 = findByName(*circuit, "R1");
        Component* m1 = findByName(*circuit, "M1");
        QVERIFY(r1 != nullptr);
        QVERIFY(m1 != nullptr);

        auto result = MNASolver::solveDC(*circuit);
        QVERIFY2(result.success, qPrintable(result.errorMessage));

        // Get voltage at drain node (R1 pin1 = M1 pin1 = Drain)
        int nodeDrain = r1->pin(1).nodeId;
        QVERIFY(nodeDrain > 0);
        double vDrain = result.nodeResults[nodeDrain].voltage;

        // NMOS ON: R_on=1Ω, so V_drain ≈ 10 * 1/(1000+1) ≈ 0.01V
        QVERIFY2(std::abs(vDrain) < 0.1,
                 qPrintable(QString("V_drain: got %1 V, expected ~0V (NMOS ON)")
                            .arg(vDrain)));

        // Current through R1: I = (10 - V_drain) / 1000 ≈ 10 mA
        double iR1 = (10.0 - vDrain) / 1000.0;
        QVERIFY2(std::abs(iR1 - 0.01) < 0.001,
                 qPrintable(QString("I_R1: got %1 A, expected ~10 mA").arg(iR1)));
    }

    // Test 2: NMOS cutoff (DC)
    // V1(10V) → R1(1kΩ) → Drain ← NMOS(Vth=2V) → Source → GND
    // Gate tied to GND (0V) → V_GS = 0V < Vth=2V → CUTOFF
    // V_drain ≈ 10V (no current flows)
    void cutoff()
    {
        auto circuit = loadCircuit("nmosfet_cutoff_dc.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        Component* r1 = findByName(*circuit, "R1");
        QVERIFY(r1 != nullptr);

        auto result = MNASolver::solveDC(*circuit);
        QVERIFY2(result.success, qPrintable(result.errorMessage));

        int nodeDrain = r1->pin(1).nodeId;
        QVERIFY(nodeDrain > 0);
        double vDrain = result.nodeResults[nodeDrain].voltage;

        // NMOS OFF: R_off=1MΩ, V_drain ≈ 10 * 1e6/(1000+1e6) ≈ 9.99V
        QVERIFY2(std::abs(vDrain - 10.0) < 0.1,
                 qPrintable(QString("V_drain: got %1 V, expected ~10V (NMOS OFF)")
                            .arg(vDrain)));
    }

    // Test 3: NMOS gate-controlled switching (transient)
    // Pulse on gate: HIGH(+5V) → ON, LOW(-5V) → OFF
    // When ON: V_drain ≈ 0V, when OFF: V_drain ≈ 10V
    void gatePulse()
    {
        auto circuit = loadCircuit("nmosfet_gate_pulse.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        Component* r1 = findByName(*circuit, "R1");
        QVERIFY(r1 != nullptr);
        int nodeDrain = r1->pin(1).nodeId;
        QVERIFY(nodeDrain > 0);

        // Run transient for 2 cycles at 100Hz (20ms)
        double timeStep = 0.00005;  // 50µs
        double totalTime = 0.02;     // 20ms

        auto tResult = MNASolver::solveTransientFull(*circuit, timeStep, totalTime);
        QVERIFY2(tResult.success, qPrintable(tResult.errorMessage));
        QVERIFY(tResult.frames.size() > 100);

        // Analyze second cycle (steady state): skip first 10ms
        double vMax = -1000.0;
        double vMin = 1000.0;
        int steadyStart = static_cast<int>(tResult.frames.size()) / 2;

        for (int i = steadyStart; i < static_cast<int>(tResult.frames.size()); ++i) {
            double vD = tResult.frames[i].nodeVoltages[nodeDrain];
            if (vD > vMax) vMax = vD;
            if (vD < vMin) vMin = vD;
        }

        // When pulse HIGH (+5V): V_GS=5V > Vth → ON → V_drain ≈ 0V
        QVERIFY2(vMin < 0.5,
                 qPrintable(QString("V_drain_min: got %1 V, expected <0.5V (ON)")
                            .arg(vMin)));

        // When pulse LOW (-5V): V_GS=-5V < Vth → OFF → V_drain ≈ 10V
        QVERIFY2(vMax > 9.0,
                 qPrintable(QString("V_drain_max: got %1 V, expected >9V (OFF)")
                            .arg(vMax)));
    }

    // Test 4: Save/load roundtrip
    void saveLoadRoundtrip()
    {
        auto circuit = loadCircuit("nmosfet_roundtrip.esim");
        QVERIFY(circuit != nullptr);

        Component* m1 = findByName(*circuit, "M1");
        QVERIFY(m1 != nullptr);
        QCOMPARE(m1->type(), ComponentType::NMosfet);

        auto* mosfet = static_cast<NMosfet*>(m1);
        QVERIFY2(std::abs(mosfet->thresholdVoltage() - 3.5) < 0.001,
                 qPrintable(QString("Vth: got %1, expected 3.5").arg(mosfet->thresholdVoltage())));

        // Serialize to JSON
        QJsonObject json = circuit->toJson();

        // Reload from JSON
        auto circuit2 = Circuit::fromJson(json);
        QVERIFY(circuit2 != nullptr);

        Component* m2 = findByName(*circuit2, "M1");
        QVERIFY(m2 != nullptr);
        QCOMPARE(m2->type(), ComponentType::NMosfet);

        auto* mosfet2 = static_cast<NMosfet*>(m2);
        QVERIFY2(std::abs(mosfet2->thresholdVoltage() - 3.5) < 0.001,
                 qPrintable(QString("Roundtrip Vth: got %1, expected 3.5")
                            .arg(mosfet2->thresholdVoltage())));
    }
};

QTEST_GUILESS_MAIN(TestNMosfet)
#include "test_nmosfet.moc"
