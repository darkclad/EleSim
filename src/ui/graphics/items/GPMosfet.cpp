#include "GPMosfet.h"
#include "../GraphicPin.h"
#include "../../../core/components/PMosfet.h"

#include <QPainter>
#include <QPolygonF>

GPMosfet::GPMosfet(PMosfet* mosfet)
    : GraphicComponent(mosfet)
{
    m_symbolRect = QRectF(-30, -30, 60, 60);
    setupPins();
}

void GPMosfet::setupPins()
{
    auto* pin0 = new GraphicPin(0, this); // Gate
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this); // Source (at top for PMOS)
    pin1->setPos(15, -30);
    m_pins.push_back(pin1);

    auto* pin2 = new GraphicPin(2, this); // Drain (at bottom for PMOS)
    pin2->setPos(15, 30);
    m_pins.push_back(pin2);
}

void GPMosfet::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Gate lead
    painter->drawLine(QPointF(-30, 0), QPointF(-12, 0));

    // Inversion bubble on gate
    painter->setBrush(Qt::white);
    painter->drawEllipse(QPointF(-10, 0), 3, 3);
    painter->setBrush(Qt::NoBrush);

    // Gate plate (vertical line)
    painter->drawLine(QPointF(-6, -15), QPointF(-6, 15));

    // Channel body (vertical line)
    painter->drawLine(QPointF(0, -15), QPointF(0, 15));

    // Source connection (top): from channel to pin
    painter->drawLine(QPointF(0, -10), QPointF(15, -10));
    painter->drawLine(QPointF(15, -10), QPointF(15, -30));

    // Drain connection (bottom): from channel to pin
    painter->drawLine(QPointF(0, 10), QPointF(15, 10));
    painter->drawLine(QPointF(15, 10), QPointF(15, 30));

    // Body connection (center of channel to source)
    painter->drawLine(QPointF(0, 0), QPointF(15, 0));
    painter->drawLine(QPointF(15, 0), QPointF(15, -10));

    // Arrow on source (pointing outward from channel = P-channel)
    painter->setBrush(Qt::black);
    QPolygonF arrow;
    arrow << QPointF(10, 0) << QPointF(3, -4) << QPointF(3, 4);
    painter->drawPolygon(arrow);
}
