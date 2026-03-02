#include "GResistor.h"
#include "../GraphicPin.h"
#include "../../../core/components/Resistor.h"

#include <QPainter>

GResistor::GResistor(Resistor* resistor)
    : GraphicComponent(resistor)
{
    m_symbolRect = QRectF(-30, -10, 60, 20);
    setupPins();
}

void GResistor::setupPins()
{
    // Pin 0 at left, Pin 1 at right
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(30, 0);
    m_pins.push_back(pin1);
}

void GResistor::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Zigzag resistor symbol
    // Lead lines
    painter->drawLine(QPointF(-30, 0), QPointF(-20, 0));
    painter->drawLine(QPointF(20, 0), QPointF(30, 0));

    // Zigzag body
    QPolygonF zigzag;
    zigzag << QPointF(-20, 0)
           << QPointF(-16, -8)
           << QPointF(-8, 8)
           << QPointF(0, -8)
           << QPointF(8, 8)
           << QPointF(16, -8)
           << QPointF(20, 0);
    painter->drawPolyline(zigzag);
}
