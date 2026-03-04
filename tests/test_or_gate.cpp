#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/ORGate.h"
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

class TestORGate : public QObject
{
    Q_OBJECT

private slots:
    // Test 1: OR gate output HIGH (single input: A=5V, B=0V)
    // Any input HIGH → output HIGH
    void outputHigh()
    {
        auto circuit = loadCircuit("or_gate_output_high.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        Component* r1 = findByName(*circuit, "R1");
        QVERIFY(r1 != nullptr);

        auto result = MNASolver::solveDC(*circuit);
        QVERIFY2(result.success, qPrintable(result.errorMessage));

        int nodeY = r1->pin(0).nodeId;
        QVERIFY(nodeY > 0);
        double vY = result.nodeResults[nodeY].voltage;

        QVERIFY2(vY > 4.5,
                 qPrintable(QString("V_Y: got %1 V, expected >4.5V (OR output HIGH)")
                            .arg(vY)));
    }

    // Test 2: OR gate output LOW (both inputs LOW: A=0V, B=0V)
    // No inputs HIGH → output LOW
    void outputLow()
    {
        auto circuit = loadCircuit("or_gate_output_low.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        Component* r1 = findByName(*circuit, "R1");
        QVERIFY(r1 != nullptr);

        auto result = MNASolver::solveDC(*circuit);
        QVERIFY2(result.success, qPrintable(result.errorMessage));

        int nodeY = r1->pin(0).nodeId;
        QVERIFY(nodeY > 0);
        double vY = result.nodeResults[nodeY].voltage;

        QVERIFY2(vY < 0.5,
                 qPrintable(QString("V_Y: got %1 V, expected <0.5V (OR output LOW)")
                            .arg(vY)));
    }

    // Test 3: OR gate transient switching
    // Pulse on A, B=GND → Y toggles with A
    void transientSwitching()
    {
        auto circuit = loadCircuit("or_gate_pulse.esim");
        QVERIFY(circuit != nullptr);

        circuit->buildNetlist();

        Component* r1 = findByName(*circuit, "R1");
        QVERIFY(r1 != nullptr);
        int nodeY = r1->pin(0).nodeId;
        QVERIFY(nodeY > 0);

        double timeStep = 0.00005;
        double totalTime = 0.02;

        auto tResult = MNASolver::solveTransientFull(*circuit, timeStep, totalTime);
        QVERIFY2(tResult.success, qPrintable(tResult.errorMessage));
        QVERIFY(tResult.frames.size() > 100);

        double vMax = -1000.0;
        double vMin = 1000.0;
        int steadyStart = static_cast<int>(tResult.frames.size()) / 2;

        for (int i = steadyStart; i < static_cast<int>(tResult.frames.size()); ++i) {
            double vY = tResult.frames[i].nodeVoltages[nodeY];
            if (vY > vMax) vMax = vY;
            if (vY < vMin) vMin = vY;
        }

        // When pulse LOW (A=0): OR(0,0)=0 → V_Y ≈ 0V
        QVERIFY2(vMin < 0.5,
                 qPrintable(QString("V_Y_min: got %1 V, expected <0.5V").arg(vMin)));

        // When pulse HIGH (A=1): OR(1,0)=1 → V_Y ≈ 5V
        QVERIFY2(vMax > 4.5,
                 qPrintable(QString("V_Y_max: got %1 V, expected >4.5V").arg(vMax)));
    }

    // Test 4: Save/load roundtrip with custom Vth
    void saveLoadRoundtrip()
    {
        auto circuit = loadCircuit("or_gate_roundtrip.esim");
        QVERIFY(circuit != nullptr);

        Component* u1 = findByName(*circuit, "U1");
        QVERIFY(u1 != nullptr);
        QCOMPARE(u1->type(), ComponentType::ORGate);

        auto* gate = static_cast<ORGate*>(u1);
        QVERIFY2(std::abs(gate->thresholdVoltage() - 3.0) < 0.001,
                 qPrintable(QString("Vth: got %1, expected 3.0").arg(gate->thresholdVoltage())));

        QJsonObject json = circuit->toJson();

        auto circuit2 = Circuit::fromJson(json);
        QVERIFY(circuit2 != nullptr);

        Component* u2 = findByName(*circuit2, "U1");
        QVERIFY(u2 != nullptr);
        QCOMPARE(u2->type(), ComponentType::ORGate);

        auto* gate2 = static_cast<ORGate*>(u2);
        QVERIFY2(std::abs(gate2->thresholdVoltage() - 3.0) < 0.001,
                 qPrintable(QString("Roundtrip Vth: got %1, expected 3.0")
                            .arg(gate2->thresholdVoltage())));
    }
};

QTEST_GUILESS_MAIN(TestORGate)
#include "test_or_gate.moc"
