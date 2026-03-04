#include "GACSource.h"
#include "../GraphicPin.h"
#include "../../../core/components/ACSource.h"

#include <QPainter>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GACSource::GACSource(ACSource* source)
    : GraphicComponent(source)
{
    m_symbolRect = QRectF(-20, -20, 40, 40);
    setupPins();
}

void GACSource::setupPins()
{
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(0, -30);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(0, 30);
    m_pins.push_back(pin1);
}

void GACSource::drawSymbol(QPainter* painter)
{
    painter->setPen(symbolPen());
    painter->setBrush(Qt::NoBrush);

    // Circle
    painter->drawEllipse(QRectF(-16, -16, 32, 32));

    // Lead lines
    painter->drawLine(QPointF(0, -30), QPointF(0, -16));
    painter->drawLine(QPointF(0, 16), QPointF(0, 30));

    // Sine wave inside circle
    QPainterPath sinePath;
    bool first = true;
    for (int i = 0; i <= 40; ++i) {
        double t = static_cast<double>(i) / 40.0;
        double x = -10 + t * 20;
        double y = -6 * std::sin(t * 2 * M_PI);
        if (first) {
            sinePath.moveTo(x, y);
            first = false;
        } else {
            sinePath.lineTo(x, y);
        }
    }
    painter->setPen(symbolPen(Qt::black, 1.5));
    painter->drawPath(sinePath);
}
