#include "GCapacitor.h"
#include "../GraphicPin.h"
#include "../../../core/components/Capacitor.h"

#include <QPainter>

GCapacitor::GCapacitor(Capacitor* capacitor)
    : GraphicComponent(capacitor)
{
    m_symbolRect = QRectF(-30, -12, 60, 24);
    setupPins();
}

void GCapacitor::setupPins()
{
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(30, 0);
    m_pins.push_back(pin1);
}

void GCapacitor::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Lead lines
    painter->drawLine(QPointF(-30, 0), QPointF(-4, 0));
    painter->drawLine(QPointF(4, 0), QPointF(30, 0));

    // Two parallel plates
    painter->setPen(QPen(Qt::black, 3));
    painter->drawLine(QPointF(-4, -10), QPointF(-4, 10));
    painter->drawLine(QPointF(4, -10), QPointF(4, 10));
}
