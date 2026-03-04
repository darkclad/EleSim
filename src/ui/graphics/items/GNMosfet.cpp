#include "GNMosfet.h"
#include "../GraphicPin.h"
#include "../../../core/components/NMosfet.h"

#include <QPainter>
#include <QPolygonF>

GNMosfet::GNMosfet(NMosfet* mosfet)
    : GraphicComponent(mosfet)
{
    m_symbolRect = QRectF(-30, -30, 60, 60);
    setupPins();
}

void GNMosfet::setupPins()
{
    auto* pin0 = new GraphicPin(0, this); // Gate
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this); // Drain
    pin1->setPos(15, -30);
    m_pins.push_back(pin1);

    auto* pin2 = new GraphicPin(2, this); // Source
    pin2->setPos(15, 30);
    m_pins.push_back(pin2);
}

void GNMosfet::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Gate lead
    painter->drawLine(QPointF(-30, 0), QPointF(-8, 0));

    // Gate plate (vertical line)
    painter->drawLine(QPointF(-8, -15), QPointF(-8, 15));

    // Channel body (vertical line, slight gap from gate)
    painter->drawLine(QPointF(-2, -15), QPointF(-2, 15));

    // Drain connection: from channel to pin
    painter->drawLine(QPointF(-2, -10), QPointF(15, -10));
    painter->drawLine(QPointF(15, -10), QPointF(15, -30));

    // Source connection: from channel to pin
    painter->drawLine(QPointF(-2, 10), QPointF(15, 10));
    painter->drawLine(QPointF(15, 10), QPointF(15, 30));

    // Body connection (center of channel to source)
    painter->drawLine(QPointF(-2, 0), QPointF(15, 0));
    painter->drawLine(QPointF(15, 0), QPointF(15, 10));

    // Arrow on source (pointing inward toward channel = N-channel)
    painter->setBrush(Qt::black);
    QPolygonF arrow;
    arrow << QPointF(3, 0) << QPointF(10, -4) << QPointF(10, 4);
    painter->drawPolygon(arrow);
}
