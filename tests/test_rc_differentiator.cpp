#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/PulseSource.h"
#include "core/components/Resistor.h"
#include "core/components/Capacitor.h"
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
//  RC Differentiator Test
//
//  Circuit: SQR(5Vpk) → C(1nF) → R(10kΩ) → GND
//  tau = RC = 10kΩ × 1nF = 10µs
//  V_out measured across R (output node)
//
//  Square wave starts at +5V with capacitor uncharged.
//  V_out at step = 5V (full step appears across R)
//  V_out at end of half = 5 × exp(-halfPeriod / tau)
// ═══════════════════════════════════════════════════════

class TestRCDifferentiator : public QObject
{
    Q_OBJECT

private slots:
    void freq1kHz();
    void freq10kHz();
    void freq100kHz();

private:
    void runTest(double frequency, double expectedEndV, double endTolerance);
};

void TestRCDifferentiator::runTest(double frequency, double expectedEndV, double endTolerance)
{
    QString path = QString(TEST_DATA_DIR) + "/rc_differentiator.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load rc_differentiator.esim");

    // Set test frequency on the square wave source
    Component* sqComp = findByName(*circuit, "V1");
    QVERIFY2(sqComp != nullptr, "SquareWaveSource V1 not found");
    auto* sq = static_cast<PulseSource*>(sqComp);
    sq->setFrequency(frequency);

    Component* resistor = findByName(*circuit, "R1");
    Component* capacitor = findByName(*circuit, "C1");
    QVERIFY(resistor != nullptr);
    QVERIFY(capacitor != nullptr);

    double R = resistor->value();   // 10kΩ
    double C = capacitor->value();  // 1nF
    double tau = R * C;             // 10µs
    double halfPeriod = 0.5 / frequency;

    // Run transient for one full half-period with fine resolution
    // Stop one step before the transition edge to stay within the first half
    double dt = halfPeriod / 500.0;
    double totalTime = halfPeriod - dt;

    auto result = MNASolver::solveTransientFull(*circuit, dt, totalTime);
    QVERIFY2(result.success, "Transient solve failed");
    QVERIFY2(!result.frames.empty(), "No transient frames");

    // Find R1's pin0 node (output node = junction of C and R)
    int outNode = resistor->pin(0).nodeId;
    QVERIFY2(outNode > 0, qPrintable(QString("R1.pin0 nodeId invalid: %1").arg(outNode)));

    // ── V_out at step (near t=0) ──
    // Use a frame slightly after t=0 to let the solver settle (frame ~5)
    int spikeIdx = std::min(5, static_cast<int>(result.frames.size()) - 1);
    double vSpike = 0.0;
    {
        auto it = result.frames[spikeIdx].nodeVoltages.find(outNode);
        if (it != result.frames[spikeIdx].nodeVoltages.end())
            vSpike = it->second;
    }

    // Spike should be close to 5V (some decay already happened in 5 timesteps)
    double expectedSpike = 5.0 * std::exp(-spikeIdx * dt / tau);
    double spikeTol = 0.15; // 15% tolerance for numerical effects near discontinuity
    double spikeErr = std::abs(vSpike - expectedSpike) / expectedSpike;
    QVERIFY2(spikeErr < spikeTol,
        qPrintable(QString("V_spike: got %1V, expected ~%2V (err %3%)")
            .arg(vSpike, 0, 'f', 4).arg(expectedSpike, 0, 'f', 4).arg(spikeErr * 100, 0, 'f', 1)));

    // ── V_out at end of half-period ──
    int lastIdx = static_cast<int>(result.frames.size()) - 1;
    double vEnd = 0.0;
    {
        auto it = result.frames[lastIdx].nodeVoltages.find(outNode);
        if (it != result.frames[lastIdx].nodeVoltages.end())
            vEnd = it->second;
    }

    if (expectedEndV > 0.01) {
        // For non-negligible end values, check relative error
        double endErr = std::abs(vEnd - expectedEndV) / expectedEndV;
        QVERIFY2(endErr < endTolerance,
            qPrintable(QString("V_end: got %1V, expected %2V (err %3%)")
                .arg(vEnd, 0, 'f', 6).arg(expectedEndV, 0, 'f', 6).arg(endErr * 100, 0, 'f', 1)));
    } else {
        // For values near zero, check absolute
        QVERIFY2(std::abs(vEnd) < 0.01,
            qPrintable(QString("V_end: got %1V, expected ~0V").arg(vEnd, 0, 'f', 6)));
    }

    qDebug() << QString("f=%1Hz tau=%2us halfT=%3us: V_spike=%4V V_end=%5V (expected %6V)")
        .arg(frequency, 0, 'f', 0)
        .arg(tau * 1e6, 0, 'f', 1)
        .arg(halfPeriod * 1e6, 0, 'f', 1)
        .arg(vSpike, 0, 'f', 4)
        .arg(vEnd, 0, 'f', 6)
        .arg(expectedEndV, 0, 'f', 6);
}

void TestRCDifferentiator::freq1kHz()
{
    // T >> tau: sharp spike, fully decays to ~0
    // halfPeriod = 500µs, tau = 10µs, exp(-50) ≈ 0
    runTest(1000.0, 0.0, 0.0);
}

void TestRCDifferentiator::freq10kHz()
{
    // T ≈ 5*tau: visible exponential decay
    // halfPeriod = 50µs, tau = 10µs, exp(-5) = 0.00674
    // V_end = 5 * 0.00674 = 0.0337V
    runTest(10000.0, 5.0 * std::exp(-5.0), 0.10);
}

void TestRCDifferentiator::freq100kHz()
{
    // T ≈ tau/2: most of the signal passes through
    // halfPeriod = 5µs, tau = 10µs, exp(-0.5) = 0.6065
    // V_end = 5 * 0.6065 = 3.033V
    runTest(100000.0, 5.0 * std::exp(-0.5), 0.05);
}

QTEST_GUILESS_MAIN(TestRCDifferentiator)
#include "test_rc_differentiator.moc"
