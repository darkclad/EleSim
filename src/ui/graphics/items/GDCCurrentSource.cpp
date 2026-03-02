#include "GDCCurrentSource.h"
#include "../GraphicPin.h"
#include "../../../core/components/DCCurrentSource.h"

#include <QPainter>

GDCCurrentSource::GDCCurrentSource(DCCurrentSource* source)
    : GraphicComponent(source)
{
    m_symbolRect = QRectF(-20, -20, 40, 40);
    setupPins();
}

void GDCCurrentSource::setupPins()
{
    // Pin 0 (positive / current source) at top, Pin 1 (negative / return) at bottom
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(0, -30);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(0, 30);
    m_pins.push_back(pin1);
}

void GDCCurrentSource::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Circle
    painter->drawEllipse(QRectF(-16, -16, 32, 32));

    // Lead lines
    painter->drawLine(QPointF(0, -30), QPointF(0, -16));
    painter->drawLine(QPointF(0, 16), QPointF(0, 30));

    // Arrow pointing up (current direction: from pin1 to pin0 through source)
    painter->setPen(QPen(Qt::black, 2));
    painter->drawLine(QPointF(0, 10), QPointF(0, -10));
    // Arrowhead
    painter->drawLine(QPointF(0, -10), QPointF(-4, -4));
    painter->drawLine(QPointF(0, -10), QPointF(4, -4));
}
