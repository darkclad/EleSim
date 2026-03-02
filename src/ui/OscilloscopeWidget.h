#pragma once

#include <QDockWidget>
#include "../simulation/MNASolver.h"
#include <map>

class QCheckBox;
class QComboBox;
class QLabel;
class Circuit;

enum class ProbeMode { Voltage, Current };

class OscilloscopeScreen : public QWidget
{
    Q_OBJECT
public:
    explicit OscilloscopeScreen(QWidget* parent = nullptr);

    void setData(const TransientResult& result);
    void clearData();

    void setCh1Probes(int posNode, int negNode) { m_ch1PosNode = posNode; m_ch1NegNode = negNode; update(); }
    void setCh2Probes(int posNode, int negNode) { m_ch2PosNode = posNode; m_ch2NegNode = negNode; update(); }

    void setCh1Mode(ProbeMode mode) { m_ch1Mode = mode; update(); }
    void setCh2Mode(ProbeMode mode) { m_ch2Mode = mode; update(); }

    void setCh1CurrentComp(int compId) { m_ch1CurrentCompId = compId; update(); }
    void setCh2CurrentComp(int compId) { m_ch2CurrentCompId = compId; update(); }

    void setCh1Enabled(bool on) { m_ch1Enabled = on; update(); }
    void setCh2Enabled(bool on) { m_ch2Enabled = on; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void drawGrid(QPainter& p, const QRect& area);
    void drawTrace(QPainter& p, const QRect& area, const std::vector<double>& times,
                   const std::vector<double>& values, const QColor& color,
                   const QString& unit, int channel);
    void drawCrosshair(QPainter& p, const QRect& area);
    double getChannelValue(int frameIdx, int channel) const;

    TransientResult m_result;

    // Crosshair state
    QPointF m_mousePos;
    bool m_showCrosshair = false;
    struct TraceRange { double vMin = 0, vMax = 0; };
    TraceRange m_chRange[2];
    bool m_chActive[2] = {false, false};
    QString m_chUnit[2];
    int m_ch1PosNode = -1;  // -1 = none selected
    int m_ch1NegNode = 0;   // 0 = GND (node 0)
    int m_ch2PosNode = -1;
    int m_ch2NegNode = 0;

    ProbeMode m_ch1Mode = ProbeMode::Voltage;
    ProbeMode m_ch2Mode = ProbeMode::Voltage;
    int m_ch1CurrentCompId = -1;
    int m_ch2CurrentCompId = -1;
    bool m_ch1Enabled = true;
    bool m_ch2Enabled = true;
};

struct ProbePin {
    int componentId = -1;
    int pinIndex = -1;
};

class OscilloscopeWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit OscilloscopeWidget(QWidget* parent = nullptr);

    void setTransientResult(const TransientResult& result);
    void clearData();
    void updateComponentList(Circuit* circuit);

    // Probe pin info for schematic highlighting
    ProbePin probeSelection(int channel, bool positive) const;

signals:
    void probeSelectionChanged();

private:
    void onProbeChanged();
    int resolveNodeId(QComboBox* combo) const;
    int resolveComponentId(QComboBox* combo) const;

    OscilloscopeScreen* m_screen = nullptr;
    QComboBox* m_ch1ModeCombo = nullptr;
    QComboBox* m_ch1PosCombo = nullptr;
    QComboBox* m_ch1NegCombo = nullptr;
    QComboBox* m_ch2ModeCombo = nullptr;
    QComboBox* m_ch2PosCombo = nullptr;
    QComboBox* m_ch2NegCombo = nullptr;
    QCheckBox* m_ch1EnableCheck = nullptr;
    QCheckBox* m_ch2EnableCheck = nullptr;
    QLabel* m_ch1NegLabel = nullptr;
    QLabel* m_ch2NegLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    Circuit* m_circuit = nullptr;
};
