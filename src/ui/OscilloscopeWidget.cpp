#include "OscilloscopeWidget.h"
#include "../core/Circuit.h"

#include <QPainter>
#include <QPainterPath>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QMouseEvent>
#include <cmath>
#include <algorithm>

// --- OscilloscopeScreen ---

OscilloscopeScreen::OscilloscopeScreen(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void OscilloscopeScreen::setData(const TransientResult& result)
{
    m_result = result;
    update();
}

void OscilloscopeScreen::clearData()
{
    m_result.frames.clear();
    m_result.success = false;
    update();
}

void OscilloscopeScreen::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect area = rect().adjusted(50, 10, -10, -30);

    // Dark background
    p.fillRect(rect(), QColor(0x1a, 0x1a, 0x2e));

    if (!m_result.success || m_result.frames.empty()) {
        p.setPen(QColor(100, 100, 100));
        p.drawText(rect(), Qt::AlignCenter, "No transient data");
        return;
    }

    drawGrid(p, area);

    // Reset channel activity
    m_chActive[0] = m_chActive[1] = false;

    // Extract time values
    std::vector<double> times;
    for (auto& f : m_result.frames) {
        times.push_back(f.time);
    }

    // CH1 - yellow
    if (m_ch1Enabled && m_ch1Mode == ProbeMode::Voltage && m_ch1PosNode >= 0) {
        std::vector<double> values;
        for (auto& f : m_result.frames) {
            double vPos = 0.0, vNeg = 0.0;
            auto itP = f.nodeVoltages.find(m_ch1PosNode);
            if (itP != f.nodeVoltages.end()) vPos = itP->second;
            auto itN = f.nodeVoltages.find(m_ch1NegNode);
            if (itN != f.nodeVoltages.end()) vNeg = itN->second;
            values.push_back(vPos - vNeg);
        }
        drawTrace(p, area, times, values, QColor(255, 255, 50), "V", 0);

        p.setPen(QColor(255, 255, 50));
        p.drawText(area.left() + 5, area.top() + 15, "CH1 (V)");
    } else if (m_ch1Enabled && m_ch1Mode == ProbeMode::Current && m_ch1CurrentCompId >= 0) {
        std::vector<double> values;
        for (auto& f : m_result.frames) {
            double cur = 0.0;
            auto it = f.branchCurrents.find(m_ch1CurrentCompId);
            if (it != f.branchCurrents.end()) cur = it->second;
            values.push_back(cur);
        }
        drawTrace(p, area, times, values, QColor(255, 255, 50), "A", 0);

        p.setPen(QColor(255, 255, 50));
        p.drawText(area.left() + 5, area.top() + 15, "CH1 (A)");
    }

    // CH2 - cyan
    if (m_ch2Enabled && m_ch2Mode == ProbeMode::Voltage && m_ch2PosNode >= 0) {
        std::vector<double> values;
        for (auto& f : m_result.frames) {
            double vPos = 0.0, vNeg = 0.0;
            auto itP = f.nodeVoltages.find(m_ch2PosNode);
            if (itP != f.nodeVoltages.end()) vPos = itP->second;
            auto itN = f.nodeVoltages.find(m_ch2NegNode);
            if (itN != f.nodeVoltages.end()) vNeg = itN->second;
            values.push_back(vPos - vNeg);
        }
        drawTrace(p, area, times, values, QColor(50, 255, 255), "V", 1);

        p.setPen(QColor(50, 255, 255));
        p.drawText(area.left() + 5, area.top() + 30, "CH2 (V)");
    } else if (m_ch2Enabled && m_ch2Mode == ProbeMode::Current && m_ch2CurrentCompId >= 0) {
        std::vector<double> values;
        for (auto& f : m_result.frames) {
            double cur = 0.0;
            auto it = f.branchCurrents.find(m_ch2CurrentCompId);
            if (it != f.branchCurrents.end()) cur = it->second;
            values.push_back(cur);
        }
        drawTrace(p, area, times, values, QColor(50, 255, 255), "A", 1);

        p.setPen(QColor(50, 255, 255));
        p.drawText(area.left() + 5, area.top() + 30, "CH2 (A)");
    }

    // Draw crosshair overlay
    if (m_showCrosshair)
        drawCrosshair(p, area);
}

void OscilloscopeScreen::drawGrid(QPainter& p, const QRect& area)
{
    p.setPen(QPen(QColor(40, 40, 70), 1));

    // Vertical grid lines (10 divisions)
    for (int i = 0; i <= 10; ++i) {
        int x = area.left() + i * area.width() / 10;
        p.drawLine(x, area.top(), x, area.bottom());
    }

    // Horizontal grid lines (8 divisions)
    for (int i = 0; i <= 8; ++i) {
        int y = area.top() + i * area.height() / 8;
        p.drawLine(area.left(), y, area.right(), y);
    }

    // Center lines (brighter)
    p.setPen(QPen(QColor(60, 60, 100), 1));
    int midY = area.top() + area.height() / 2;
    p.drawLine(area.left(), midY, area.right(), midY);
    int midX = area.left() + area.width() / 2;
    p.drawLine(midX, area.top(), midX, area.bottom());

    // Border
    p.setPen(QPen(QColor(80, 80, 120), 1));
    p.drawRect(area);

    // Time axis labels
    if (!m_result.frames.empty()) {
        double tMax = m_result.frames.back().time;
        p.setPen(QColor(150, 150, 180));
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);

        for (int i = 0; i <= 10; i += 2) {
            double t = tMax * i / 10.0;
            int x = area.left() + i * area.width() / 10;
            QString label;
            if (t < 0.001)
                label = QString::number(t * 1e6, 'f', 0) + QString::fromUtf8("\xC2\xB5s");
            else if (t < 1.0)
                label = QString::number(t * 1000, 'f', 1) + "ms";
            else
                label = QString::number(t, 'f', 2) + "s";
            p.drawText(x - 20, area.bottom() + 15, label);
        }
    }
}

void OscilloscopeScreen::drawTrace(QPainter& p, const QRect& area,
                                    const std::vector<double>& times,
                                    const std::vector<double>& values,
                                    const QColor& color,
                                    const QString& unit,
                                    int channel)
{
    if (times.empty() || values.empty()) return;

    // Find value range for auto-scaling
    double vMin = *std::min_element(values.begin(), values.end());
    double vMax = *std::max_element(values.begin(), values.end());

    // Add some margin
    double range = vMax - vMin;
    if (range < 1e-12) range = 1.0;
    vMin -= range * 0.1;
    vMax += range * 0.1;

    // Cache range for crosshair inverse mapping
    if (channel >= 0 && channel < 2) {
        m_chRange[channel] = {vMin, vMax};
        m_chActive[channel] = true;
        m_chUnit[channel] = unit;
    }

    double tMin = times.front();
    double tMax = times.back();
    if (std::abs(tMax - tMin) < 1e-15) return;

    QPainterPath path;
    bool first = true;

    for (size_t i = 0; i < times.size(); ++i) {
        double nx = (times[i] - tMin) / (tMax - tMin);
        double ny = 1.0 - (values[i] - vMin) / (vMax - vMin);

        double px = area.left() + nx * area.width();
        double py = area.top() + ny * area.height();

        if (first) {
            path.moveTo(px, py);
            first = false;
        } else {
            path.lineTo(px, py);
        }
    }

    p.setPen(QPen(color, 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Y-axis labels with unit suffix
    p.setPen(color);
    QFont f = p.font();
    f.setPixelSize(10);
    p.setFont(f);

    for (int i = 0; i <= 4; ++i) {
        double val = vMax - i * (vMax - vMin) / 4.0;
        int y = area.top() + i * area.height() / 4;
        QString label;
        if (std::abs(val) < 0.001)
            label = QString::number(val * 1e6, 'f', 1) + QString::fromUtf8("\xC2\xB5") + unit;
        else if (std::abs(val) < 1.0)
            label = QString::number(val * 1000, 'f', 1) + "m" + unit;
        else
            label = QString::number(val, 'f', 2) + unit;
        p.drawText(2, y + 4, label);
    }
}

double OscilloscopeScreen::getChannelValue(int frameIdx, int channel) const
{
    if (frameIdx < 0 || frameIdx >= static_cast<int>(m_result.frames.size()))
        return 0.0;
    const auto& f = m_result.frames[frameIdx];

    if (channel == 0) {
        if (m_ch1Mode == ProbeMode::Voltage && m_ch1PosNode >= 0) {
            double vPos = 0.0, vNeg = 0.0;
            auto itP = f.nodeVoltages.find(m_ch1PosNode);
            if (itP != f.nodeVoltages.end()) vPos = itP->second;
            auto itN = f.nodeVoltages.find(m_ch1NegNode);
            if (itN != f.nodeVoltages.end()) vNeg = itN->second;
            return vPos - vNeg;
        } else if (m_ch1Mode == ProbeMode::Current && m_ch1CurrentCompId >= 0) {
            auto it = f.branchCurrents.find(m_ch1CurrentCompId);
            return (it != f.branchCurrents.end()) ? it->second : 0.0;
        }
    } else {
        if (m_ch2Mode == ProbeMode::Voltage && m_ch2PosNode >= 0) {
            double vPos = 0.0, vNeg = 0.0;
            auto itP = f.nodeVoltages.find(m_ch2PosNode);
            if (itP != f.nodeVoltages.end()) vPos = itP->second;
            auto itN = f.nodeVoltages.find(m_ch2NegNode);
            if (itN != f.nodeVoltages.end()) vNeg = itN->second;
            return vPos - vNeg;
        } else if (m_ch2Mode == ProbeMode::Current && m_ch2CurrentCompId >= 0) {
            auto it = f.branchCurrents.find(m_ch2CurrentCompId);
            return (it != f.branchCurrents.end()) ? it->second : 0.0;
        }
    }
    return 0.0;
}

void OscilloscopeScreen::drawCrosshair(QPainter& p, const QRect& area)
{
    int mx = static_cast<int>(m_mousePos.x());
    int my = static_cast<int>(m_mousePos.y());

    if (mx < area.left() || mx > area.right() || my < area.top() || my > area.bottom())
        return;

    // Vertical line
    p.setPen(QPen(QColor(200, 200, 200, 120), 1, Qt::DashLine));
    p.drawLine(mx, area.top(), mx, area.bottom());

    // Horizontal line
    p.drawLine(area.left(), my, area.right(), my);

    // Compute time from X position
    double nx = static_cast<double>(mx - area.left()) / area.width();
    int numFrames = static_cast<int>(m_result.frames.size());
    int idx = std::clamp(static_cast<int>(nx * (numFrames - 1) + 0.5), 0, numFrames - 1);
    double t = m_result.frames[idx].time;

    // Format time
    QString timeStr;
    if (t < 0.001)
        timeStr = QString::number(t * 1e6, 'f', 1) + QString::fromUtf8("\xC2\xB5s");
    else if (t < 1.0)
        timeStr = QString::number(t * 1000, 'f', 2) + "ms";
    else
        timeStr = QString::number(t, 'f', 4) + "s";

    // Build readout text
    QString text = "t=" + timeStr;

    QColor ch1Color(255, 255, 50);
    QColor ch2Color(50, 255, 255);

    double ch1Val = 0, ch2Val = 0;
    if (m_chActive[0]) {
        ch1Val = getChannelValue(idx, 0);
        text += QString("  CH1: %1%2").arg(QString::number(ch1Val, 'g', 4), m_chUnit[0]);
    }
    if (m_chActive[1]) {
        ch2Val = getChannelValue(idx, 1);
        text += QString("  CH2: %1%2").arg(QString::number(ch2Val, 'g', 4), m_chUnit[1]);
    }

    // Draw dots on traces at the crosshair position
    for (int ch = 0; ch < 2; ++ch) {
        if (!m_chActive[ch]) continue;
        double val = (ch == 0) ? ch1Val : ch2Val;
        double ny = 1.0 - (val - m_chRange[ch].vMin) / (m_chRange[ch].vMax - m_chRange[ch].vMin);
        double py = area.top() + ny * area.height();
        QColor c = (ch == 0) ? ch1Color : ch2Color;
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(mx, py), 4, 4);
    }

    // Draw text readout background
    QFont f = p.font();
    f.setPixelSize(11);
    p.setFont(f);
    QRect textRect = p.fontMetrics().boundingRect(text);
    int tx = mx + 12;
    int ty = area.top() + 5;
    // Flip to left side if too close to right edge
    if (tx + textRect.width() + 8 > area.right())
        tx = mx - textRect.width() - 16;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 40, 200));
    p.drawRoundedRect(tx - 4, ty - 2, textRect.width() + 8, textRect.height() + 4, 3, 3);

    p.setPen(QColor(220, 220, 240));
    p.drawText(tx, ty + textRect.height() - 2, text);
}

void OscilloscopeScreen::mouseMoveEvent(QMouseEvent* event)
{
    m_mousePos = event->position();
    m_showCrosshair = true;
    update();
}

void OscilloscopeScreen::leaveEvent(QEvent* /*event*/)
{
    m_showCrosshair = false;
    update();
}

// --- OscilloscopeWidget ---

OscilloscopeWidget::OscilloscopeWidget(QWidget* parent)
    : QDockWidget(tr("Oscilloscope"), parent)
{
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    // Controls row
    auto* controls = new QHBoxLayout;

    m_ch1EnableCheck = new QCheckBox("CH1");
    m_ch1EnableCheck->setChecked(true);
    m_ch1EnableCheck->setStyleSheet("QCheckBox { color: #ffff32; }");
    controls->addWidget(m_ch1EnableCheck);
    m_ch1ModeCombo = new QComboBox;
    m_ch1ModeCombo->addItem("V", static_cast<int>(ProbeMode::Voltage));
    m_ch1ModeCombo->addItem("I", static_cast<int>(ProbeMode::Current));
    m_ch1ModeCombo->setFixedWidth(45);
    controls->addWidget(m_ch1ModeCombo);

    controls->addWidget(new QLabel("+:"));
    m_ch1PosCombo = new QComboBox;
    m_ch1PosCombo->setMinimumWidth(100);
    controls->addWidget(m_ch1PosCombo);

    m_ch1NegLabel = new QLabel(QString::fromUtf8("\xe2\x88\x92:"));
    controls->addWidget(m_ch1NegLabel);
    m_ch1NegCombo = new QComboBox;
    m_ch1NegCombo->setMinimumWidth(100);
    controls->addWidget(m_ch1NegCombo);

    controls->addSpacing(16);

    m_ch2EnableCheck = new QCheckBox("CH2");
    m_ch2EnableCheck->setChecked(true);
    m_ch2EnableCheck->setStyleSheet("QCheckBox { color: #32ffff; }");
    controls->addWidget(m_ch2EnableCheck);
    m_ch2ModeCombo = new QComboBox;
    m_ch2ModeCombo->addItem("V", static_cast<int>(ProbeMode::Voltage));
    m_ch2ModeCombo->addItem("I", static_cast<int>(ProbeMode::Current));
    m_ch2ModeCombo->setFixedWidth(45);
    controls->addWidget(m_ch2ModeCombo);

    controls->addWidget(new QLabel("+:"));
    m_ch2PosCombo = new QComboBox;
    m_ch2PosCombo->setMinimumWidth(100);
    controls->addWidget(m_ch2PosCombo);

    m_ch2NegLabel = new QLabel(QString::fromUtf8("\xe2\x88\x92:"));
    controls->addWidget(m_ch2NegLabel);
    m_ch2NegCombo = new QComboBox;
    m_ch2NegCombo->setMinimumWidth(100);
    controls->addWidget(m_ch2NegCombo);

    controls->addStretch();

    m_infoLabel = new QLabel;
    m_infoLabel->setStyleSheet("color: #888;");
    controls->addWidget(m_infoLabel);

    layout->addLayout(controls);

    // Screen
    m_screen = new OscilloscopeScreen;
    layout->addWidget(m_screen);

    setWidget(container);

    // Connect combo changes — all combos trigger the same update
    auto onComboChanged = [this](int) { onProbeChanged(); };

    connect(m_ch1PosCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChanged);
    connect(m_ch1NegCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChanged);
    connect(m_ch2PosCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChanged);
    connect(m_ch2NegCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChanged);

    // Channel enable checkboxes
    connect(m_ch1EnableCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_ch1ModeCombo->setEnabled(on);
        m_ch1PosCombo->setEnabled(on);
        m_ch1NegCombo->setEnabled(on);
        m_screen->setCh1Enabled(on);
        emit probeSelectionChanged();
    });
    connect(m_ch2EnableCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_ch2ModeCombo->setEnabled(on);
        m_ch2PosCombo->setEnabled(on);
        m_ch2NegCombo->setEnabled(on);
        m_screen->setCh2Enabled(on);
        emit probeSelectionChanged();
    });

    // Mode combo changes
    connect(m_ch1ModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        bool isVoltage = m_ch1ModeCombo->currentData().toInt() == static_cast<int>(ProbeMode::Voltage);
        m_ch1NegCombo->setVisible(isVoltage);
        m_ch1NegLabel->setVisible(isVoltage);
        onProbeChanged();
    });
    connect(m_ch2ModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        bool isVoltage = m_ch2ModeCombo->currentData().toInt() == static_cast<int>(ProbeMode::Voltage);
        m_ch2NegCombo->setVisible(isVoltage);
        m_ch2NegLabel->setVisible(isVoltage);
        onProbeChanged();
    });
}

void OscilloscopeWidget::onProbeChanged()
{
    auto ch1Mode = static_cast<ProbeMode>(m_ch1ModeCombo->currentData().toInt());
    auto ch2Mode = static_cast<ProbeMode>(m_ch2ModeCombo->currentData().toInt());

    m_screen->setCh1Mode(ch1Mode);
    m_screen->setCh2Mode(ch2Mode);

    if (ch1Mode == ProbeMode::Voltage) {
        int ch1Pos = resolveNodeId(m_ch1PosCombo);
        int ch1Neg = resolveNodeId(m_ch1NegCombo);
        m_screen->setCh1Probes(ch1Pos, ch1Neg);
    } else {
        int compId = resolveComponentId(m_ch1PosCombo);
        m_screen->setCh1CurrentComp(compId);
    }

    if (ch2Mode == ProbeMode::Voltage) {
        int ch2Pos = resolveNodeId(m_ch2PosCombo);
        int ch2Neg = resolveNodeId(m_ch2NegCombo);
        m_screen->setCh2Probes(ch2Pos, ch2Neg);
    } else {
        int compId = resolveComponentId(m_ch2PosCombo);
        m_screen->setCh2CurrentComp(compId);
    }

    emit probeSelectionChanged();
}

int OscilloscopeWidget::resolveNodeId(QComboBox* combo) const
{
    int data = combo->currentData().toInt();
    if (data == -1) return 0; // GND = node 0

    // Decode: componentId * 100 + pinIndex
    int compId = data / 100;
    int pinIdx = data % 100;

    if (!m_circuit) return -1;

    auto& comps = m_circuit->components();
    auto it = comps.find(compId);
    if (it == comps.end()) return -1;

    Component* comp = it->second.get();
    if (pinIdx < 0 || pinIdx >= comp->pinCount()) return -1;

    int nodeId = comp->pin(pinIdx).nodeId;
    return (nodeId > 0) ? nodeId : -1; // unconnected pins return -1
}

int OscilloscopeWidget::resolveComponentId(QComboBox* combo) const
{
    int data = combo->currentData().toInt();
    if (data < 0) return -1;
    return data / 100; // extract componentId from encoded data
}

void OscilloscopeWidget::setTransientResult(const TransientResult& result)
{
    m_screen->setData(result);
    if (result.success && !result.frames.empty()) {
        double totalTime = result.frames.back().time;
        int numFrames = static_cast<int>(result.frames.size());

        // Format time nicely
        QString timeStr;
        if (totalTime < 0.001)
            timeStr = QString::number(totalTime * 1e6, 'f', 0) + QString::fromUtf8("\xC2\xB5s");
        else if (totalTime < 1.0)
            timeStr = QString::number(totalTime * 1000, 'f', 1) + "ms";
        else
            timeStr = QString::number(totalTime, 'f', 3) + "s";

        m_infoLabel->setText(tr("%1 samples, %2").arg(numFrames).arg(timeStr));
    }
}

void OscilloscopeWidget::clearData()
{
    m_screen->clearData();
    m_infoLabel->setText("");
}

void OscilloscopeWidget::updateComponentList(Circuit* circuit)
{
    m_circuit = circuit;

    // Remember current selections
    int prevCh1Pos = m_ch1PosCombo->currentData().toInt();
    int prevCh1Neg = m_ch1NegCombo->currentData().toInt();
    int prevCh2Pos = m_ch2PosCombo->currentData().toInt();
    int prevCh2Neg = m_ch2NegCombo->currentData().toInt();

    // Block signals during repopulation
    m_ch1PosCombo->blockSignals(true);
    m_ch1NegCombo->blockSignals(true);
    m_ch2PosCombo->blockSignals(true);
    m_ch2NegCombo->blockSignals(true);

    m_ch1PosCombo->clear();
    m_ch1NegCombo->clear();
    m_ch2PosCombo->clear();
    m_ch2NegCombo->clear();

    // First entry: GND
    m_ch1PosCombo->addItem("GND", -1);
    m_ch1NegCombo->addItem("GND", -1);
    m_ch2PosCombo->addItem("GND", -1);
    m_ch2NegCombo->addItem("GND", -1);

    if (circuit) {
        for (auto& [id, comp] : circuit->components()) {
            if (comp->type() == ComponentType::Junction || comp->type() == ComponentType::Ground) continue;
            for (int p = 0; p < comp->pinCount(); ++p) {
                QString label = comp->name() + "." + QString::number(p + 1);
                int data = id * 100 + p;
                m_ch1PosCombo->addItem(label, data);
                m_ch1NegCombo->addItem(label, data);
                m_ch2PosCombo->addItem(label, data);
                m_ch2NegCombo->addItem(label, data);
            }
        }
    }

    // Restore selections if still valid
    auto restore = [](QComboBox* combo, int prevData, int defaultIdx) {
        int idx = combo->findData(prevData);
        combo->setCurrentIndex(idx >= 0 ? idx : defaultIdx);
    };

    restore(m_ch1PosCombo, prevCh1Pos, 0);
    restore(m_ch1NegCombo, prevCh1Neg, 0); // default to GND
    restore(m_ch2PosCombo, prevCh2Pos, 0);
    restore(m_ch2NegCombo, prevCh2Neg, 0);

    m_ch1PosCombo->blockSignals(false);
    m_ch1NegCombo->blockSignals(false);
    m_ch2PosCombo->blockSignals(false);
    m_ch2NegCombo->blockSignals(false);

    // Trigger an update with restored selections
    onProbeChanged();
}

ProbePin OscilloscopeWidget::probeSelection(int channel, bool positive) const
{
    QComboBox* combo = nullptr;
    if (channel == 1)
        combo = positive ? m_ch1PosCombo : m_ch1NegCombo;
    else
        combo = positive ? m_ch2PosCombo : m_ch2NegCombo;

    int data = combo->currentData().toInt();
    if (data < 0) return {-1, -1}; // GND or none

    return {data / 100, data % 100};
}
