#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <complex>

#include "core/Circuit.h"
#include "core/components/ACSource.h"
#include "core/components/Resistor.h"
#include "core/components/Inductor.h"
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

// ─── Helper: find component by name ───
static Component* findByName(Circuit& circuit, const QString& name)
{
    for (auto& [id, comp] : circuit.components()) {
        if (comp->name() == name) return comp.get();
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════
//  Parallel LC Tank Test
//
//  Circuit: AC(5Vpk) → Rs(50Ω) → [L(10mH) || C(100nF) || Rp(10kΩ)] → GND
//
//  f₀ = 1/(2π√(LC)) = 1/(2π√(0.01 × 1e-7)) ≈ 5033 Hz
//  At resonance: Z_tank = Rp (imaginary parts cancel)
//  V_out = V_source × |Z_tank| / |Rs + Z_tank|
//  I_source = V_source / |Rs + Z_tank|
//
//  Parallel tank impedance:
//    1/Z_tank = 1/(jωL) + jωC + 1/Rp
//    Z_tank = 1 / (1/Rp + j(ωC - 1/(ωL)))
// ═══════════════════════════════════════════════════════

struct TankExpected {
    double frequency;
    double xL;        // inductive reactance ωL
    double xC;        // capacitive reactance 1/(ωC)
    double zTankMag;  // |Z_tank|
    double vOutPeak;  // V_out across tank
    double iSource;   // current from source
};

static TankExpected computeExpected(double vPeak, double Rs, double L, double C, double Rp, double freq)
{
    TankExpected e;
    e.frequency = freq;
    double omega = 2.0 * M_PI * freq;
    e.xL = omega * L;
    e.xC = 1.0 / (omega * C);

    // Parallel impedance: Z_tank = 1 / (1/Rp + j(ωC - 1/(ωL)))
    double realPart = 1.0 / Rp;
    double imagPart = omega * C - 1.0 / (omega * L);
    std::complex<double> yTank(realPart, imagPart);
    std::complex<double> zTank = 1.0 / yTank;
    e.zTankMag = std::abs(zTank);

    // Voltage divider: V_out = V_source × Z_tank / (Rs + Z_tank)
    std::complex<double> zTotal = std::complex<double>(Rs, 0.0) + zTank;
    std::complex<double> vOut = vPeak * zTank / zTotal;
    e.vOutPeak = std::abs(vOut);

    // Source current
    e.iSource = vPeak / std::abs(zTotal);

    return e;
}

class TestParallelLCTank : public QObject
{
    Q_OBJECT

private slots:
    void freq3kHz();
    void freq5kHz();
    void freq8kHz();

private:
    void runTest(double frequency, double tolerance);
};

void TestParallelLCTank::runTest(double frequency, double tolerance)
{
    QString path = QString(TEST_DATA_DIR) + "/parallel_lc_tank.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load parallel_lc_tank.esim");

    // Set test frequency on the AC source
    Component* acComp = findByName(*circuit, "V1");
    QVERIFY2(acComp != nullptr, "ACSource V1 not found");
    auto* ac = static_cast<ACSource*>(acComp);
    ac->setFrequency(frequency);

    // Get component values
    Component* rs = findByName(*circuit, "Rs");
    Component* l1 = findByName(*circuit, "L1");
    Component* c1 = findByName(*circuit, "C1");
    Component* rp = findByName(*circuit, "Rp");
    QVERIFY(rs && l1 && c1 && rp);

    double Rs = rs->value();   // 50Ω
    double L  = l1->value();   // 10mH
    double C  = c1->value();   // 100nF
    double Rp = rp->value();   // 10kΩ
    double vPeak = ac->value(); // 5V

    TankExpected expected = computeExpected(vPeak, Rs, L, C, Rp, frequency);

    // Run AC analysis
    SimulationResult result = MNASolver::solveAC(*circuit, frequency);
    QVERIFY2(result.success, qPrintable("AC solve failed: " + result.errorMessage));

    // Find output node: Rs.pin1 = top of tank
    int outNode = rs->pin(1).nodeId;
    QVERIFY2(outNode > 0, qPrintable(QString("Rs.pin1 nodeId invalid: %1").arg(outNode)));

    // V_out from node results
    auto nodeIt = result.nodeResults.find(outNode);
    QVERIFY2(nodeIt != result.nodeResults.end(), "Output node not found in results");
    double vOut = nodeIt->second.voltage;

    double vOutErr = std::abs(vOut - expected.vOutPeak) / expected.vOutPeak;
    QVERIFY2(vOutErr < tolerance,
        qPrintable(QString("V_out: got %1V, expected %2V (err %3%)")
            .arg(vOut, 0, 'f', 4).arg(expected.vOutPeak, 0, 'f', 4).arg(vOutErr * 100, 0, 'f', 2)));

    // Source current from branch results (AC source)
    BranchResult brAC;
    bool foundAC = false;
    for (auto& [id, br] : result.branchResults) {
        Component* comp = circuit->component(id);
        if (comp && comp->name() == "V1") { brAC = br; foundAC = true; break; }
    }
    QVERIFY2(foundAC, "AC source V1 not found in branch results");

    double iErr = std::abs(brAC.current - expected.iSource) / expected.iSource;
    QVERIFY2(iErr < tolerance,
        qPrintable(QString("I_source: got %1A, expected %2A (err %3%)")
            .arg(brAC.current, 0, 'e', 4).arg(expected.iSource, 0, 'e', 4).arg(iErr * 100, 0, 'f', 2)));

    // Verify tank impedance indirectly: Z_tank = V_out / I_source
    double zTank = (brAC.current > 1e-15) ? vOut / brAC.current : 0.0;
    double zErr = std::abs(zTank - expected.zTankMag) / expected.zTankMag;
    // More relaxed tolerance for impedance since it compounds errors
    QVERIFY2(zErr < tolerance * 2,
        qPrintable(QString("|Z_tank|: got %1Ω, expected %2Ω (err %3%)")
            .arg(zTank, 0, 'f', 1).arg(expected.zTankMag, 0, 'f', 1).arg(zErr * 100, 0, 'f', 2)));

    qDebug() << QString("f=%1Hz: X_L=%2Ω X_C=%3Ω |Z_tank|=%4Ω V_out=%5V I_src=%6mA")
        .arg(frequency, 0, 'f', 0)
        .arg(expected.xL, 0, 'f', 1)
        .arg(expected.xC, 0, 'f', 1)
        .arg(zTank, 0, 'f', 1)
        .arg(vOut, 0, 'f', 4)
        .arg(brAC.current * 1000, 0, 'f', 3);
}

void TestParallelLCTank::freq3kHz()
{
    // Below resonance: X_L=188.5Ω, X_C=531Ω, |Z|≈292Ω
    runTest(3000.0, 0.03);
}

void TestParallelLCTank::freq5kHz()
{
    // Near resonance: X_L≈X_C≈316Ω, |Z|≈10kΩ (max)
    runTest(5000.0, 0.03);
}

void TestParallelLCTank::freq8kHz()
{
    // Above resonance: X_L=503Ω, X_C=199Ω, |Z|≈329Ω
    runTest(8000.0, 0.03);
}

QTEST_GUILESS_MAIN(TestParallelLCTank)
#include "test_parallel_lc_tank.moc"
