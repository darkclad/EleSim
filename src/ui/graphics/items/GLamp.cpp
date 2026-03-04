#include "GLamp.h"
#include "../GraphicPin.h"
#include "../../../core/components/Lamp.h"

#include <QPainter>
#include <QRadialGradient>
#include <cmath>
#include <algorithm>

GLamp::GLamp(Lamp* lamp)
    : GraphicComponent(lamp)
{
    m_symbolRect = QRectF(-20, -20, 40, 40);
    setupPins();
}

void GLamp::setCurrent(double amps)
{
    m_current = std::abs(amps);
    m_hasSimData = true;
    update();
}

void GLamp::clearCurrent()
{
    m_current = 0.0;
    m_hasSimData = false;
    update();
}

void GLamp::setupPins()
{
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(30, 0);
    m_pins.push_back(pin1);
}

void GLamp::drawSymbol(QPainter* painter)
{
    auto* lamp = static_cast<Lamp*>(m_coreComponent);

    // Compute brightness: 0.0 (off) to 1.0 (full glow)
    double brightness = 0.0;
    if (m_hasSimData && m_current > 0.0) {
        double threshold = lamp->currentThreshold(); // 10mA default
        // Brightness ramps from 0 at zero current to 1.0 at 2x threshold
        // Use sqrt for a more natural curve (dim light visible early)
        double ratio = m_current / threshold;
        brightness = std::clamp(std::sqrt(ratio / 2.0), 0.0, 1.0);
    }

    // Outer glow when bright
    if (brightness > 0.1) {
        int glowAlpha = static_cast<int>(brightness * 100);
        double glowRadius = 20.0 + brightness * 12.0;
        QRadialGradient glow(QPointF(0, 0), glowRadius);
        glow.setColorAt(0.0, QColor(255, 255, 100, glowAlpha));
        glow.setColorAt(1.0, QColor(255, 200, 50, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(QPointF(0, 0), glowRadius, glowRadius);
    }

    // Lamp body fill: gray (off) -> warm yellow -> bright white-yellow
    QColor fillColor;
    if (!m_hasSimData) {
        fillColor = QColor(240, 240, 240); // No sim data: light gray
    } else if (brightness < 0.01) {
        fillColor = QColor(180, 180, 180); // Off: darker gray
    } else {
        // Interpolate from dim amber to bright yellow-white
        int r = 180 + static_cast<int>(brightness * 75);   // 180 -> 255
        int g = 140 + static_cast<int>(brightness * 115);  // 140 -> 255
        int b = 40  + static_cast<int>(brightness * 180);  // 40  -> 220
        fillColor = QColor(std::min(r, 255), std::min(g, 255), std::min(b, 255));
    }

    painter->setPen(symbolPen());
    painter->setBrush(QBrush(fillColor));

    // Circle
    painter->drawEllipse(QRectF(-14, -14, 28, 28));

    // X inside (lamp symbol)
    painter->drawLine(QPointF(-10, -10), QPointF(10, 10));
    painter->drawLine(QPointF(-10, 10), QPointF(10, -10));

    // Lead lines
    painter->drawLine(QPointF(-30, 0), QPointF(-14, 0));
    painter->drawLine(QPointF(14, 0), QPointF(30, 0));
}
