#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "core/Circuit.h"
#include "core/components/PulseSource.h"
#include "core/components/Resistor.h"
#include "core/components/Capacitor.h"
#include "core/components/Inductor.h"
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

static double frameVoltage(const TransientResult& result, int frameIdx, int nodeId)
{
    auto it = result.frames[frameIdx].nodeVoltages.find(nodeId);
    if (it != result.frames[frameIdx].nodeVoltages.end())
        return it->second;
    return 0.0;
}

// ═══════════════════════════════════════════════════════
//  Pulse Source — 4 real-circuit tests
// ═══════════════════════════════════════════════════════

class TestPulseSource : public QObject
{
    Q_OBJECT

private slots:
    void squareWaveResistiveLoad();
    void asymmetricDutyVoltageDivider();
    void rcLowPassDcAverage();
    void rlTransientResponse();
};

// ─── Test 1 ─────────────────────────────────────────────
//  Circuit file: pulse_square_r.esim
//  PulseSource(5 V, 1 kHz, 50%) → R(1 kΩ) → GND
//
//  Pure resistive load — voltage across R equals source voltage.
//
//  At t = T/4 (middle of HIGH phase):  V_R = +5 V, I_R = +5 mA
//  At t = 3T/4 (middle of LOW phase):  V_R = −5 V, I_R = −5 mA
// ─────────────────────────────────────────────────────────
void TestPulseSource::squareWaveResistiveLoad()
{
    QString path = QString(TEST_DATA_DIR) + "/pulse_square_r.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load pulse_square_r.esim");

    Component* v1 = findByName(*circuit, "V1");
    Component* r1 = findByName(*circuit, "R1");
    QVERIFY(v1 && r1);

    auto* pulse = static_cast<PulseSource*>(v1);
    double V = pulse->value();        // 5 V
    double R = r1->value();           // 1 kΩ
    double freq = pulse->frequency(); // 1 kHz
    double period = 1.0 / freq;       // 1 ms

    // T = 1 ms, dt = 5 µs → 200 frames/period
    double dt = period / 200.0;

    auto result = MNASolver::solveTransientFull(*circuit, dt, period);
    QVERIFY2(result.success, qPrintable("Transient solve failed: " + result.errorMessage));
    QVERIFY(!result.frames.empty());

    int outNode = r1->pin(0).nodeId;
    QVERIFY2(outNode > 0, "R1 pin0 not connected");

    int nFrames = static_cast<int>(result.frames.size());

    // ── At t = T/4: expect +V ──
    int hiIdx = nFrames / 4;
    double vHi = frameVoltage(result, hiIdx, outNode);
    QVERIFY2(std::abs(vHi - V) < 0.1, qPrintable(
        QString("HIGH phase: got %1 V, expected +%2 V").arg(vHi, 0, 'f', 4).arg(V, 0, 'f', 1)));

    // ── At t = 3T/4: expect −V ──
    int loIdx = 3 * nFrames / 4;
    double vLo = frameVoltage(result, loIdx, outNode);
    QVERIFY2(std::abs(vLo + V) < 0.1, qPrintable(
        QString("LOW phase: got %1 V, expected -%2 V").arg(vLo, 0, 'f', 4).arg(V, 0, 'f', 1)));

    // ── Current check: I = V/R ──
    double iHi = vHi / R;
    double iLo = vLo / R;
    double expectedIHi = V / R;
    QVERIFY2(std::abs(iHi - expectedIHi) / expectedIHi < 0.02, qPrintable(
        QString("I_HI: got %1 A, expected %2 A").arg(iHi, 0, 'e', 4).arg(expectedIHi, 0, 'e', 4)));
    QVERIFY2(std::abs(iLo + expectedIHi) / expectedIHi < 0.02, qPrintable(
        QString("I_LO: got %1 A, expected -%2 A").arg(iLo, 0, 'e', 4).arg(expectedIHi, 0, 'e', 4)));

    qDebug() << QString("Test 1 — 50%% square wave: V_HI = %1 V, V_LO = %2 V")
        .arg(vHi, 0, 'f', 4).arg(vLo, 0, 'f', 4);
}

// ─── Test 2 ─────────────────────────────────────────────
//  Circuit file: pulse_25duty_divider.esim
//  PulseSource(10 V, 500 Hz, 25%) → R1(1 kΩ) → R2(2 kΩ) → GND
//
//  Resistive voltage divider:
//      V_B = V_source × R2 / (R1 + R2) = V_source × 2/3
//
//  HIGH (first 25% of T): V_B = +10 × 2/3 = +6.667 V
//  LOW  (remaining 75%):  V_B = −10 × 2/3 = −6.667 V
// ─────────────────────────────────────────────────────────
void TestPulseSource::asymmetricDutyVoltageDivider()
{
    QString path = QString(TEST_DATA_DIR) + "/pulse_25duty_divider.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load pulse_25duty_divider.esim");

    Component* v1 = findByName(*circuit, "V1");
    Component* r1 = findByName(*circuit, "R1");
    Component* r2 = findByName(*circuit, "R2");
    QVERIFY(v1 && r1 && r2);

    auto* pulse = static_cast<PulseSource*>(v1);
    double V = pulse->value();         // 10 V
    double R1 = r1->value();           // 1 kΩ
    double R2 = r2->value();           // 2 kΩ
    double freq = pulse->frequency();  // 500 Hz
    double period = 1.0 / freq;        // 2 ms

    // T = 2 ms, dt = 5 µs → 400 frames/period
    double dt = period / 400.0;

    auto result = MNASolver::solveTransientFull(*circuit, dt, period);
    QVERIFY2(result.success, qPrintable("Transient solve failed: " + result.errorMessage));
    QVERIFY(!result.frames.empty());

    int nodeB = r1->pin(1).nodeId;
    QVERIFY2(nodeB > 0, "R1 pin1 not connected");

    int nFrames = static_cast<int>(result.frames.size());
    double dividerRatio = R2 / (R1 + R2);
    double expectedHi = V * dividerRatio;     // +6.667 V
    double expectedLo = -V * dividerRatio;    // −6.667 V

    // ── Sample at 12.5% of period (middle of HIGH phase) ──
    int hiIdx = nFrames / 8;
    double vHi = frameVoltage(result, hiIdx, nodeB);
    double relErrHi = std::abs(vHi - expectedHi) / expectedHi;
    QVERIFY2(relErrHi < 0.02, qPrintable(
        QString("V_B (HIGH): got %1 V, expected %2 V (err %3%)")
            .arg(vHi, 0, 'f', 4).arg(expectedHi, 0, 'f', 4)
            .arg(relErrHi * 100, 0, 'f', 2)));

    // ── Sample at 62.5% of period (middle of LOW phase) ──
    int loIdx = 5 * nFrames / 8;
    double vLo = frameVoltage(result, loIdx, nodeB);
    double relErrLo = std::abs(vLo - expectedLo) / std::abs(expectedLo);
    QVERIFY2(relErrLo < 0.02, qPrintable(
        QString("V_B (LOW): got %1 V, expected %2 V (err %3%)")
            .arg(vLo, 0, 'f', 4).arg(expectedLo, 0, 'f', 4)
            .arg(relErrLo * 100, 0, 'f', 2)));

    qDebug() << QString("Test 2 — 25%% duty divider: V_HI = %1 V, V_LO = %2 V")
        .arg(vHi, 0, 'f', 4).arg(vLo, 0, 'f', 4);
}

// ─── Test 3 ─────────────────────────────────────────────
//  Circuit file: pulse_75duty_rc.esim
//  PulseSource(5 V, 100 Hz, 75%) → R(1 kΩ) → C(100 µF) → GND
//
//  RC low-pass filter extracts the DC component.
//      τ = R × C = 1 kΩ × 100 µF = 100 ms
//      T = 10 ms  (T << τ → good filtering)
//
//  DC average:
//      V_DC = V × (2D − 1) = 5 × 0.5 = 2.5 V
//
//  After 5τ = 500 ms, capacitor voltage settles to V_DC.
//  Ripple: ΔV ≈ V_pp × T / (2τ) = 10 × 0.01/0.2 = 0.5 V_pp
// ─────────────────────────────────────────────────────────
void TestPulseSource::rcLowPassDcAverage()
{
    QString path = QString(TEST_DATA_DIR) + "/pulse_75duty_rc.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load pulse_75duty_rc.esim");

    Component* v1 = findByName(*circuit, "V1");
    Component* r1 = findByName(*circuit, "R1");
    Component* c1 = findByName(*circuit, "C1");
    QVERIFY(v1 && r1 && c1);

    auto* pulse = static_cast<PulseSource*>(v1);
    double V = pulse->value();         // 5 V
    double D = pulse->dutyCycle();     // 0.75
    double freq = pulse->frequency();  // 100 Hz
    double R = r1->value();            // 1 kΩ
    double C = c1->value();            // 100 µF
    double tau = R * C;                // 100 ms

    // Run 5τ = 500 ms with dt = 100 µs
    double dt = 1e-4;
    double totalTime = 5.0 * tau;

    auto result = MNASolver::solveTransientFull(*circuit, dt, totalTime);
    QVERIFY2(result.success, qPrintable("Transient solve failed: " + result.errorMessage));
    QVERIFY(!result.frames.empty());

    int capNode = c1->pin(0).nodeId;
    QVERIFY2(capNode > 0, "C1 pin0 not connected");

    // ── Average capacitor voltage over the last full period ──
    int nFrames = static_cast<int>(result.frames.size());
    int periodFrames = static_cast<int>(1.0 / (freq * dt));
    double sumV = 0.0;
    int count = 0;
    for (int i = nFrames - periodFrames; i < nFrames; ++i) {
        sumV += frameVoltage(result, i, capNode);
        ++count;
    }
    double avgV = sumV / count;

    double expectedDC = V * (2.0 * D - 1.0);  // 2.5 V
    double relErr = std::abs(avgV - expectedDC) / expectedDC;
    QVERIFY2(relErr < 0.10, qPrintable(
        QString("DC average: got %1 V, expected %2 V (err %3%)")
            .arg(avgV, 0, 'f', 4).arg(expectedDC, 0, 'f', 4)
            .arg(relErr * 100, 0, 'f', 1)));

    qDebug() << QString("Test 3 — 75%% duty RC filter: V_avg = %1 V (expected %2 V, err %3%)")
        .arg(avgV, 0, 'f', 4).arg(expectedDC, 0, 'f', 4)
        .arg(relErr * 100, 0, 'f', 1);
}

// ─── Test 4 ─────────────────────────────────────────────
//  Circuit file: pulse_rl_transient.esim
//  PulseSource(10 V, 1 kHz, 50%) → R(100 Ω) → L(10 mH) → GND
//
//  Series RL, τ = L/R = 10 mH / 100 Ω = 100 µs, half-period = 5τ
//
//  During first half-period (+10 V), starting from i(0) = 0:
//      V_L(t) = V × e^(−t/τ)
//
//  At t = τ:   V_L = 10 × e^(−1) ≈ 3.679 V
//  At t = 3τ:  V_L = 10 × e^(−3) ≈ 0.498 V
//  At t → 5τ:  V_L → 0 V (inductor fully energized)
// ─────────────────────────────────────────────────────────
void TestPulseSource::rlTransientResponse()
{
    QString path = QString(TEST_DATA_DIR) + "/pulse_rl_transient.esim";
    auto circuit = loadCircuit(path);
    QVERIFY2(circuit != nullptr, "Failed to load pulse_rl_transient.esim");

    Component* v1 = findByName(*circuit, "V1");
    Component* r1 = findByName(*circuit, "R1");
    Component* l1 = findByName(*circuit, "L1");
    QVERIFY(v1 && r1 && l1);

    auto* pulse = static_cast<PulseSource*>(v1);
    double V = pulse->value();   // 10 V
    double R = r1->value();      // 100 Ω
    double L = l1->value();      // 10 mH
    double tau = L / R;          // 100 µs

    // dt = 2 µs (dt/τ = 0.02), simulate slightly less than half-period
    // to stay within the +V phase (avoid inductor voltage spike at transition)
    double dt = 2e-6;
    double totalTime = 4.8 * tau;   // 0.48 ms

    auto result = MNASolver::solveTransientFull(*circuit, dt, totalTime);
    QVERIFY2(result.success, qPrintable("Transient solve failed: " + result.errorMessage));
    QVERIFY(!result.frames.empty());

    // Node B = voltage across inductor (L pin0, other terminal at GND)
    int nodeB = r1->pin(1).nodeId;
    QVERIFY2(nodeB > 0, "R1 pin1 not connected");

    // ── V_L at t = τ ──
    int idx1tau = static_cast<int>(tau / dt);
    double vL_1tau = frameVoltage(result, idx1tau, nodeB);
    double expected_1tau = V * std::exp(-1.0);   // 3.679 V

    double relErr1 = std::abs(vL_1tau - expected_1tau) / expected_1tau;
    QVERIFY2(relErr1 < 0.05, qPrintable(
        QString("V_L at t=τ: got %1 V, expected %2 V (err %3%)")
            .arg(vL_1tau, 0, 'f', 4).arg(expected_1tau, 0, 'f', 4)
            .arg(relErr1 * 100, 0, 'f', 1)));

    // ── V_L at t = 3τ ──
    int idx3tau = static_cast<int>(3.0 * tau / dt);
    double vL_3tau = frameVoltage(result, idx3tau, nodeB);
    double expected_3tau = V * std::exp(-3.0);   // 0.498 V

    double relErr3 = std::abs(vL_3tau - expected_3tau) / expected_3tau;
    QVERIFY2(relErr3 < 0.10, qPrintable(
        QString("V_L at t=3τ: got %1 V, expected %2 V (err %3%)")
            .arg(vL_3tau, 0, 'f', 4).arg(expected_3tau, 0, 'f', 4)
            .arg(relErr3 * 100, 0, 'f', 1)));

    // ── V_L → 0 at end (t ≈ 4.8τ) ──
    int lastIdx = static_cast<int>(result.frames.size()) - 1;
    double vL_end = frameVoltage(result, lastIdx, nodeB);
    double expected_end = V * std::exp(-totalTime / tau);
    QVERIFY2(std::abs(vL_end) < 0.5, qPrintable(
        QString("V_L at t=4.8τ: got %1 V, expected ~%2 V")
            .arg(vL_end, 0, 'f', 4).arg(expected_end, 0, 'f', 4)));

    qDebug() << QString("Test 4 — RL transient: V_L(τ) = %1 V, V_L(3τ) = %2 V, V_L(end) = %3 V")
        .arg(vL_1tau, 0, 'f', 4).arg(vL_3tau, 0, 'f', 4).arg(vL_end, 0, 'f', 4);
}

QTEST_GUILESS_MAIN(TestPulseSource)
#include "test_pulse_source.moc"
