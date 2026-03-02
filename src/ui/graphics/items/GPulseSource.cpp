#include "GPulseSource.h"
#include "../GraphicPin.h"
#include "../../../core/components/PulseSource.h"

#include <QPainter>

GPulseSource::GPulseSource(PulseSource* source)
    : GraphicComponent(source)
{
    m_symbolRect = QRectF(-20, -20, 40, 40);
    setupPins();
}

void GPulseSource::setupPins()
{
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(0, -30);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(0, 30);
    m_pins.push_back(pin1);
}

void GPulseSource::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Circle
    painter->drawEllipse(QRectF(-16, -16, 32, 32));

    // Lead lines
    painter->drawLine(QPointF(0, -30), QPointF(0, -16));
    painter->drawLine(QPointF(0, 16), QPointF(0, 30));

    // Square/pulse wave inside circle
    painter->setPen(QPen(Qt::black, 1.5));
    QPainterPath pulsePath;
    // Draw a pulse wave: low → high → low
    pulsePath.moveTo(-10, 5);   // start low
    pulsePath.lineTo(-5, 5);    // low segment
    pulsePath.lineTo(-5, -5);   // rise
    pulsePath.lineTo(5, -5);    // high segment
    pulsePath.lineTo(5, 5);     // fall
    pulsePath.lineTo(10, 5);    // low segment
    painter->drawPath(pulsePath);
}
