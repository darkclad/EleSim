#include "OscilloscopeWidget.h"
#include "../core/Circuit.h"

#include <QPainter>
#include <QPainterPath>
#include <QComboBox>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>

// --- Time formatting helpers ---

// Choose time unit based on the visible span (so all labels on one axis share the same unit)
struct TimeUnit {
    double divisor;     // divide seconds by this to get display value
    const char* suffix; // label suffix
};

static TimeUnit timeUnitForSpan(double spanSeconds)
{
    double absSpan = std::abs(spanSeconds);
    if (absSpan < 1e-9)  return {1e-12, "ps"};
    if (absSpan < 1e-6)  return {1e-9,  "ns"};
    if (absSpan < 1e-3)  return {1e-6,  "\xC2\xB5s"};  // µs in UTF-8
    if (absSpan < 1.0)   return {1e-3,  "ms"};
    return {1.0, "s"};
}

// Format a single time value — picks unit automatically from the value itself
static QString formatTime(double seconds)
{
    double a = std::abs(seconds);
    if (a < 1e-9)
        return QString::number(seconds * 1e12, 'f', 1) + "ps";
    if (a < 1e-6)
        return QString::number(seconds * 1e9, 'f', 1) + "ns";
    if (a < 1e-3)
        return QString::number(seconds * 1e6, 'f', 1) + QString::fromUtf8("\xC2\xB5s");
    if (a < 1.0)
        return QString::number(seconds * 1e3, 'f', 1) + "ms";
    return QString::number(seconds, 'f', 3) + "s";
}

// Format a time value using a pre-chosen unit (for consistent axis labels)
static QString formatTimeWithUnit(double seconds, const TimeUnit& unit)
{
    double val = seconds / unit.divisor;
    // Choose decimal places based on magnitude
    int decimals = (std::abs(val) < 10.0) ? 2 : (std::abs(val) < 100.0) ? 1 : 0;
    return QString::number(val, 'f', decimals) + QString::fromUtf8(unit.suffix);
}

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
    m_viewStart = 0.0;
    m_viewEnd = -1.0; // show all
    update();
}

void OscilloscopeScreen::setDataKeepView(const TransientResult& result)
{
    m_result = result;
    // Don't reset m_viewStart / m_viewEnd — preserve zoom/pan
    update();
}

void OscilloscopeScreen::clearData()
{
    m_result.frames.clear();
    m_result.success = false;
    m_streaming = false;
    update();
}

void OscilloscopeScreen::appendFrames(const std::vector<TransientFrame>& newFrames)
{
    if (newFrames.empty()) return;

    if (m_result.frames.empty())
        m_result.success = true;

    m_result.frames.insert(m_result.frames.end(), newFrames.begin(), newFrames.end());
    update();
}

void OscilloscopeScreen::setStreaming(bool streaming)
{
    m_streaming = streaming;
    update();
}

void OscilloscopeScreen::setAutoScale(double frequency)
{
    if (frequency <= 0.0) {
        // No AC frequency — show all
        m_viewStart = 0.0;
        m_viewEnd = -1.0;
        return;
    }

    // Standard oscilloscope time/div steps (in seconds)
    // 10 divisions total, pick step so full span shows ~2-5 periods
    static const double steps[] = {
        1e-12, 2e-12, 5e-12,   // ps
        1e-11, 2e-11, 5e-11,   // 10ps
        1e-10, 2e-10, 5e-10,   // 100ps
        1e-9,  2e-9,  5e-9,    // ns
        1e-8,  2e-8,  5e-8,    // 10ns
        1e-7,  2e-7,  5e-7,    // 100ns
        1e-6,  2e-6,  5e-6,    // µs
        1e-5,  2e-5,  5e-5,    // 10µs
        1e-4,  2e-4,  5e-4,    // 100µs
        1e-3,  2e-3,  5e-3,    // ms
        1e-2,  2e-2,  5e-2,    // 10ms
        1e-1,  2e-1,  5e-1,    // 100ms
        1.0,   2.0,   5.0,     // s
    };

    double period = 1.0 / frequency;
    // We want the total span (10 * step) to show ~3 periods
    double targetSpan = period * 3.0;

    // Find smallest step where 10*step >= targetSpan
    double chosenStep = steps[sizeof(steps)/sizeof(steps[0]) - 1];
    for (double s : steps) {
        if (s * 10.0 >= targetSpan) {
            chosenStep = s;
            break;
        }
    }

    m_viewStart = 0.0;
    m_viewEnd = chosenStep * 10.0;
}

void OscilloscopeScreen::setViewCenter(double time)
{
    if (m_result.frames.empty()) return;

    // If in show-all mode, switch to explicit view
    if (m_viewEnd < 0) {
        m_viewStart = 0.0;
        m_viewEnd = m_result.frames.back().time;
    }

    double span = m_viewEnd - m_viewStart;
    m_viewStart = time - span / 2.0;
    m_viewEnd = m_viewStart + span;

    if (m_viewStart < 0.0) {
        m_viewEnd -= m_viewStart;
        m_viewStart = 0.0;
    }

    update();

    if (!m_result.frames.empty() && m_viewEnd > m_result.frames.back().time)
        emit viewRangeExceedsData(m_viewEnd);

    emit viewChanged(m_viewStart, m_viewEnd,
                     m_result.frames.empty() ? 0.0 : m_result.frames.back().time);
}

double OscilloscopeScreen::dataEndTime() const
{
    if (m_result.frames.empty()) return 0.0;
    return m_result.frames.back().time;
}

void OscilloscopeScreen::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect area = plotArea();

    // Dark background
    p.fillRect(rect(), QColor(0x1a, 0x1a, 0x2e));

    if ((!m_result.success && !m_streaming) || m_result.frames.empty()) {
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
        double tStart = visibleStart();
        double tEnd = visibleEnd();
        TimeUnit unit = timeUnitForSpan(tEnd - tStart);

        p.setPen(QColor(150, 150, 180));
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);

        for (int i = 0; i <= 10; i += 2) {
            double t = tStart + (tEnd - tStart) * i / 10.0;
            int x = area.left() + i * area.width() / 10;
            QString label = formatTimeWithUnit(t, unit);
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

    double tStart = visibleStart();
    double tEnd = visibleEnd();
    double tSpan = tEnd - tStart;
    if (tSpan < 1e-15) return;

    QPainterPath path;
    bool first = true;

    for (size_t i = 0; i < times.size(); ++i) {
        double nx = (times[i] - tStart) / tSpan;
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

    // Determine value unit based on the visible range
    double valRange = vMax - vMin;
    auto valUnit = [&](double val) -> QString {
        double a = std::abs(val);
        if (valRange < 1e-6)
            return QString::number(val * 1e9, 'f', 1) + "n" + unit;
        if (valRange < 1e-3)
            return QString::number(val * 1e6, 'f', 1) + QString::fromUtf8("\xC2\xB5") + unit;
        if (valRange < 1.0)
            return QString::number(val * 1e3, 'f', 1) + "m" + unit;
        return QString::number(val, 'f', 2) + unit;
    };

    for (int i = 0; i <= 4; ++i) {
        double val = vMax - i * (vMax - vMin) / 4.0;
        int y = area.top() + i * area.height() / 4;
        p.drawText(2, y + 4, valUnit(val));
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
    int idx = frameIndexAtX(mx);
    if (idx < 0) return;
    double t = m_result.frames[idx].time;

    // Format time
    QString timeStr = formatTime(t);

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

QRect OscilloscopeScreen::plotArea() const
{
    return rect().adjusted(50, 10, -10, -30);
}

double OscilloscopeScreen::visibleStart() const
{
    return m_viewStart;
}

double OscilloscopeScreen::visibleEnd() const
{
    if (m_viewEnd < 0 && !m_result.frames.empty())
        return m_result.frames.back().time;
    return (m_viewEnd < 0) ? 1.0 : m_viewEnd;
}

double OscilloscopeScreen::timeAtX(int x) const
{
    QRect area = plotArea();
    double nx = static_cast<double>(x - area.left()) / area.width();
    return visibleStart() + nx * (visibleEnd() - visibleStart());
}

int OscilloscopeScreen::frameIndexAtX(int x) const
{
    QRect area = plotArea();
    if (m_result.frames.empty() || x < area.left() || x > area.right())
        return -1;

    double t = timeAtX(x);
    // Binary search for closest frame
    const auto& frames = m_result.frames;
    auto it = std::lower_bound(frames.begin(), frames.end(), t,
        [](const TransientFrame& f, double val) { return f.time < val; });

    if (it == frames.end()) return static_cast<int>(frames.size()) - 1;
    if (it == frames.begin()) return 0;

    // Pick closest of the two neighbors
    auto prev = std::prev(it);
    if (std::abs(it->time - t) < std::abs(prev->time - t))
        return static_cast<int>(it - frames.begin());
    return static_cast<int>(prev - frames.begin());
}

void OscilloscopeScreen::wheelEvent(QWheelEvent* event)
{
    if (m_result.frames.empty()) return;

    // Initialize explicit range from show-all on first zoom
    if (m_viewEnd < 0) {
        m_viewStart = 0.0;
        m_viewEnd = m_result.frames.back().time;
    }

    double span = m_viewEnd - m_viewStart;

    // Mouse position as fraction of plot area
    QRect area = plotArea();
    double mx = event->position().x();
    double frac = (mx - area.left()) / area.width();
    frac = std::clamp(frac, 0.0, 1.0);

    // Time under cursor
    double tCursor = m_viewStart + frac * span;

    // Zoom factor: scroll up = zoom in, scroll down = zoom out
    double factor = (event->angleDelta().y() > 0) ? 0.8 : 1.25;
    double newSpan = span * factor;

    // Clamp: min 1ps, no max limit
    static constexpr double MIN_SPAN = 1e-12; // 1ps
    if (newSpan < MIN_SPAN) newSpan = MIN_SPAN;

    // Keep cursor at same screen fraction
    m_viewStart = tCursor - frac * newSpan;
    m_viewEnd = m_viewStart + newSpan;

    // Clamp start >= 0
    if (m_viewStart < 0.0) {
        m_viewEnd -= m_viewStart;
        m_viewStart = 0.0;
    }

    update();

    // Request more simulation data if view extends beyond existing data
    if (!m_result.frames.empty() && m_viewEnd > m_result.frames.back().time)
        emit viewRangeExceedsData(m_viewEnd);

    emit viewChanged(m_viewStart, m_viewEnd,
                     m_result.frames.empty() ? 0.0 : m_result.frames.back().time);
}

void OscilloscopeScreen::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Initialize explicit range from show-all on first pan
        if (m_viewEnd < 0 && !m_result.frames.empty()) {
            m_viewStart = 0.0;
            m_viewEnd = m_result.frames.back().time;
        }
        m_isPanning = true;
        m_panStartPos = event->pos();
        m_panStartViewStart = m_viewStart;
        setCursor(Qt::ClosedHandCursor);
    }
}

void OscilloscopeScreen::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    m_viewStart = 0.0;
    m_viewEnd = -1.0;
    update();

    double dataEnd = m_result.frames.empty() ? 0.0 : m_result.frames.back().time;
    emit viewChanged(0.0, dataEnd, dataEnd);
}

void OscilloscopeScreen::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isPanning) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void OscilloscopeScreen::mouseMoveEvent(QMouseEvent* event)
{
    m_mousePos = event->position();
    m_showCrosshair = true;

    if (m_isPanning) {
        QRect area = plotArea();
        double span = m_viewEnd - m_viewStart;
        double dx = m_panStartPos.x() - event->pos().x();
        double dt = dx * span / area.width();
        double newStart = m_panStartViewStart + dt;

        // Clamp: can't pan before 0
        if (newStart < 0.0) newStart = 0.0;

        m_viewEnd = newStart + span;
        m_viewStart = newStart;

        // Request more simulation data if view extends beyond existing data
        if (!m_result.frames.empty() && m_viewEnd > m_result.frames.back().time)
            emit viewRangeExceedsData(m_viewEnd);

        emit viewChanged(m_viewStart, m_viewEnd,
                         m_result.frames.empty() ? 0.0 : m_result.frames.back().time);
    }

    update();

    int idx = frameIndexAtX(static_cast<int>(m_mousePos.x()));
    if (idx >= 0 && idx != m_lastFrameIndex) {
        m_lastFrameIndex = idx;
        emit crosshairFrameChanged(idx);
    }
}

void OscilloscopeScreen::leaveEvent(QEvent* /*event*/)
{
    m_showCrosshair = false;
    m_lastFrameIndex = -1;
    if (m_isPanning) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
    update();
    emit crosshairLeft();
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

    // Time slider
    m_timeSlider = new QSlider(Qt::Horizontal);
    m_timeSlider->setRange(0, 1000);
    m_timeSlider->setValue(0);
    m_timeSlider->setFixedHeight(16);
    layout->addWidget(m_timeSlider);

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

    // Forward crosshair signals with frame data
    connect(m_screen, &OscilloscopeScreen::crosshairFrameChanged, this, [this](int idx) {
        const auto& frames = m_screen->data().frames;
        if (idx >= 0 && idx < static_cast<int>(frames.size()))
            emit timePointHovered(frames[idx]);
    });
    connect(m_screen, &OscilloscopeScreen::crosshairLeft, this, &OscilloscopeWidget::timePointLeft);

    // Forward view range signal for re-simulation
    connect(m_screen, &OscilloscopeScreen::viewRangeExceedsData, this, &OscilloscopeWidget::simulationRangeNeeded);

    // Time slider: user drags to jump to a time position
    connect(m_timeSlider, &QSlider::valueChanged, this, [this](int value) {
        double dataEnd = m_screen->dataEndTime();
        if (dataEnd <= 0.0) return;
        double time = (value / 1000.0) * dataEnd;
        m_screen->setViewCenter(time);
    });

    // Update slider position when view changes (zoom/pan)
    connect(m_screen, &OscilloscopeScreen::viewChanged, this,
            [this](double viewStart, double viewEnd, double dataEnd) {
        if (dataEnd <= 0.0) return;
        double center = (viewStart + viewEnd) / 2.0;
        int sliderVal = static_cast<int>((center / dataEnd) * 1000.0);
        sliderVal = std::clamp(sliderVal, 0, 1000);
        m_timeSlider->blockSignals(true);
        m_timeSlider->setValue(sliderVal);
        m_timeSlider->blockSignals(false);
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

const TransientResult& OscilloscopeWidget::transientResult() const
{
    return m_screen->data();
}

void OscilloscopeWidget::setTransientResultKeepView(const TransientResult& result)
{
    m_screen->setDataKeepView(result);
    if (result.success && !result.frames.empty()) {
        int numFrames = static_cast<int>(result.frames.size());
        m_infoLabel->setText(tr("%1 samples, %2").arg(numFrames).arg(formatTime(result.frames.back().time)));
    }
}

void OscilloscopeWidget::setTransientResult(const TransientResult& result)
{
    m_screen->setData(result);
    if (result.success && !result.frames.empty()) {
        int numFrames = static_cast<int>(result.frames.size());
        m_infoLabel->setText(tr("%1 samples, %2").arg(numFrames).arg(formatTime(result.frames.back().time)));
    }
}

void OscilloscopeWidget::clearData()
{
    m_screen->clearData();
    m_infoLabel->setText("");
}

void OscilloscopeWidget::appendFrames(const std::vector<TransientFrame>& newFrames)
{
    m_screen->appendFrames(newFrames);
    updateInfoLabel();
}

void OscilloscopeWidget::setStreaming(bool streaming)
{
    m_screen->setStreaming(streaming);
}

void OscilloscopeWidget::setAutoScale(double frequency)
{
    m_screen->setAutoScale(frequency);
}

void OscilloscopeWidget::updateInfoLabel()
{
    const auto& frames = m_screen->data().frames;
    if (frames.empty()) return;

    int numFrames = static_cast<int>(frames.size());
    m_infoLabel->setText(tr("%1 samples, %2").arg(numFrames).arg(formatTime(frames.back().time)));
}

QJsonObject OscilloscopeWidget::saveSettings() const
{
    QJsonObject obj;
    obj["ch1Enabled"] = m_ch1EnableCheck->isChecked();
    obj["ch2Enabled"] = m_ch2EnableCheck->isChecked();
    obj["ch1Mode"] = m_ch1ModeCombo->currentData().toInt();
    obj["ch2Mode"] = m_ch2ModeCombo->currentData().toInt();
    obj["ch1Pos"] = m_ch1PosCombo->currentData().toInt();
    obj["ch1Neg"] = m_ch1NegCombo->currentData().toInt();
    obj["ch2Pos"] = m_ch2PosCombo->currentData().toInt();
    obj["ch2Neg"] = m_ch2NegCombo->currentData().toInt();
    return obj;
}

void OscilloscopeWidget::restoreSettings(const QJsonObject& obj)
{
    if (obj.isEmpty()) return;

    m_ch1EnableCheck->setChecked(obj["ch1Enabled"].toBool(true));
    m_ch2EnableCheck->setChecked(obj["ch2Enabled"].toBool(true));

    // Restore mode combos
    int ch1Mode = obj["ch1Mode"].toInt(static_cast<int>(ProbeMode::Voltage));
    int ch2Mode = obj["ch2Mode"].toInt(static_cast<int>(ProbeMode::Voltage));
    int idx1 = m_ch1ModeCombo->findData(ch1Mode);
    if (idx1 >= 0) m_ch1ModeCombo->setCurrentIndex(idx1);
    int idx2 = m_ch2ModeCombo->findData(ch2Mode);
    if (idx2 >= 0) m_ch2ModeCombo->setCurrentIndex(idx2);

    // Restore probe selections
    auto restore = [](QComboBox* combo, int data) {
        int idx = combo->findData(data);
        if (idx >= 0) combo->setCurrentIndex(idx);
    };
    restore(m_ch1PosCombo, obj["ch1Pos"].toInt(-1));
    restore(m_ch1NegCombo, obj["ch1Neg"].toInt(-1));
    restore(m_ch2PosCombo, obj["ch2Pos"].toInt(-1));
    restore(m_ch2NegCombo, obj["ch2Neg"].toInt(-1));
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

void OscilloscopeWidget::setProbeFromScene(int componentId, int pinIndex, int channel, bool positive)
{
    int data = componentId * 100 + pinIndex;

    QComboBox* combo = nullptr;
    if (channel == 1)
        combo = positive ? m_ch1PosCombo : m_ch1NegCombo;
    else
        combo = positive ? m_ch2PosCombo : m_ch2NegCombo;

    int idx = combo->findData(data);
    if (idx >= 0) {
        combo->setCurrentIndex(idx); // triggers onProbeChanged via signal
    }
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
