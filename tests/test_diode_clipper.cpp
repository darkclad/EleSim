#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/ACSource.h"
#include "core/components/DCSource.h"
#include "core/components/Resistor.h"
#include "core/components/Diode.h"
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
//  Symmetrical Diode Clipper Test
//
//  Circuit: AC(5Vpk,1kHz) → R(1kΩ) → V_out
//           D1: anode at V_out, cathode to Vb1(+2V) → GND
//           D2: anode at Vb2(-2V), cathode at V_out → GND
//
//  No bias (Vb=0V): clips at ±Vf = ±0.7V
//  With ±2V bias:   clips at ±(Vbias+Vf) = ±2.7V
//
//  I through R at clip = (Vpeak - Vclip) / R
// ═══════════════════════════════════════════════════════

class TestDiodeClipper : public QObject
{
    Q_OBJECT

private slots:
    void noBias();
    void withBias();

private:
    void runTest(double biasVoltage, double expectedClipPos, double expectedClipNeg,
                 double clipTolerance, double currentTolerance);
};

void TestDiodeClipper::runTest(double biasVoltage, double expectedClipPos, double expectedClipNeg,
                               double clipTolerance, double currentTolerance)
{
    QString path = QString(TEST_DATA_DIR) + "/diode_clipper.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load diode_clipper.esim");

    // Set bias voltage (0V for no-bias, 2V for biased)
    Component* vb1Comp = findByName(*circuit, "Vb1");
    Component* vb2Comp = findByName(*circuit, "Vb2");
    QVERIFY(vb1Comp && vb2Comp);
    vb1Comp->setValue(biasVoltage);
    vb2Comp->setValue(biasVoltage);

    Component* acComp = findByName(*circuit, "V1");
    Component* rComp  = findByName(*circuit, "R1");
    QVERIFY(acComp && rComp);
    auto* ac = static_cast<ACSource*>(acComp);

    double vPeak = ac->value();    // 5V
    double freq  = ac->frequency(); // 1kHz
    double R     = rComp->value(); // 1kΩ

    // Transient: one full cycle with fine resolution
    double period = 1.0 / freq;
    double dt = period / 500.0;   // 2µs steps
    double totalTime = period;

    auto result = MNASolver::solveTransientFull(*circuit, dt, totalTime);
    QVERIFY2(result.success, qPrintable("Transient solve failed: " + result.errorMessage));
    QVERIFY2(!result.frames.empty(), "No transient frames");

    // Find output node: R1.pin1
    int outNode = rComp->pin(1).nodeId;
    QVERIFY2(outNode > 0, qPrintable(QString("R1.pin1 nodeId invalid: %1").arg(outNode)));

    // Scan all frames for max and min V_out
    double vMax = -1e9, vMin = 1e9;
    for (auto& frame : result.frames) {
        auto it = frame.nodeVoltages.find(outNode);
        if (it != frame.nodeVoltages.end()) {
            if (it->second > vMax) vMax = it->second;
            if (it->second < vMin) vMin = it->second;
        }
    }

    // Verify positive clipping level
    double posErr = std::abs(vMax - expectedClipPos);
    QVERIFY2(posErr < clipTolerance,
        qPrintable(QString("V_out max: got %1V, expected %2V (err %3V)")
            .arg(vMax, 0, 'f', 4).arg(expectedClipPos, 0, 'f', 4).arg(posErr, 0, 'f', 4)));

    // Verify negative clipping level
    double negErr = std::abs(vMin - expectedClipNeg);
    QVERIFY2(negErr < clipTolerance,
        qPrintable(QString("V_out min: got %1V, expected %2V (err %3V)")
            .arg(vMin, 0, 'f', 4).arg(expectedClipNeg, 0, 'f', 4).arg(negErr, 0, 'f', 4)));

    // Verify current through R at positive clip
    // I = (Vpeak - Vclip) / R
    double expectedI = (vPeak - expectedClipPos) / R;
    // Find frame near positive peak (t ≈ T/4 = quarter cycle)
    int peakIdx = static_cast<int>(result.frames.size()) / 4;
    double vIn_atPeak = vPeak; // sin(π/2) = 1
    double vOut_atPeak = 0.0;
    {
        auto it = result.frames[peakIdx].nodeVoltages.find(outNode);
        if (it != result.frames[peakIdx].nodeVoltages.end())
            vOut_atPeak = it->second;
    }
    double iAtClip = (vIn_atPeak - vOut_atPeak) / R;
    double iErr = std::abs(iAtClip - expectedI) / expectedI;
    QVERIFY2(iErr < currentTolerance,
        qPrintable(QString("I at clip: got %1mA, expected %2mA (err %3%)")
            .arg(iAtClip * 1000, 0, 'f', 3).arg(expectedI * 1000, 0, 'f', 3).arg(iErr * 100, 0, 'f', 1)));

    // Verify linear region: at t ≈ 0, V_out ≈ V_in ≈ 0
    // Check a frame in the linear region (small voltage, below clipping)
    // Use a frame around t = T/20 where V_in ≈ 5*sin(2π/20) ≈ 1.545V
    int linearIdx = static_cast<int>(result.frames.size()) / 20;
    double tLinear = linearIdx * dt;
    double vIn_linear = vPeak * std::sin(2.0 * M_PI * freq * tLinear);
    double vOut_linear = 0.0;
    {
        auto it = result.frames[linearIdx].nodeVoltages.find(outNode);
        if (it != result.frames[linearIdx].nodeVoltages.end())
            vOut_linear = it->second;
    }
    // In linear region (below clip threshold), V_out ≈ V_in
    // Only check if we're below the clip threshold
    if (std::abs(vIn_linear) < std::abs(expectedClipPos) - 0.2) {
        double linearErr = std::abs(vOut_linear - vIn_linear);
        QVERIFY2(linearErr < 0.15,
            qPrintable(QString("Linear region: V_out=%1V, V_in=%2V (diff %3V)")
                .arg(vOut_linear, 0, 'f', 4).arg(vIn_linear, 0, 'f', 4).arg(linearErr, 0, 'f', 4)));
    }

    qDebug() << QString("bias=%1V: V_out=[%2V, %3V] clip=[%4V, %5V] I_clip=%6mA")
        .arg(biasVoltage, 0, 'f', 1)
        .arg(vMin, 0, 'f', 4).arg(vMax, 0, 'f', 4)
        .arg(expectedClipNeg, 0, 'f', 1).arg(expectedClipPos, 0, 'f', 1)
        .arg(iAtClip * 1000, 0, 'f', 3);
}

void TestDiodeClipper::noBias()
{
    // Vbias=0V: clips at ±0.7V, I_clip = (5-0.7)/1000 = 4.30mA
    runTest(0.0, 0.7, -0.7, 0.10, 0.10);
}

void TestDiodeClipper::withBias()
{
    // Vbias=2V: clips at ±2.7V, I_clip = (5-2.7)/1000 = 2.30mA
    runTest(2.0, 2.7, -2.7, 0.10, 0.10);
}

QTEST_GUILESS_MAIN(TestDiodeClipper)
#include "test_diode_clipper.moc"
