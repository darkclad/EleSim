#include "GZenerDiode.h"
#include "../GraphicPin.h"
#include "../../../core/components/ZenerDiode.h"

#include <QPainter>
#include <QPolygonF>

GZenerDiode::GZenerDiode(ZenerDiode* zener)
    : GraphicComponent(zener)
{
    m_symbolRect = QRectF(-30, -10, 60, 20);
    setupPins();
}

void GZenerDiode::setupPins()
{
    auto* pin0 = new GraphicPin(0, this); // Anode
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this); // Cathode
    pin1->setPos(30, 0);
    m_pins.push_back(pin1);
}

void GZenerDiode::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Lead lines
    painter->drawLine(QPointF(-30, 0), QPointF(-8, 0));
    painter->drawLine(QPointF(8, 0), QPointF(30, 0));

    // Triangle (anode side, pointing right) — same as regular diode
    QPolygonF triangle;
    triangle << QPointF(-8, -10) << QPointF(8, 0) << QPointF(-8, 10);
    painter->setBrush(Qt::black);
    painter->drawPolygon(triangle);

    // Zener cathode bar: bent ends (Z-shape)
    // Main vertical bar
    painter->setPen(QPen(Qt::black, 3));
    painter->drawLine(QPointF(8, -10), QPointF(8, 10));
    // Top bend (goes left)
    painter->drawLine(QPointF(8, -10), QPointF(5, -10));
    // Bottom bend (goes right)
    painter->drawLine(QPointF(8, 10), QPointF(11, 10));
}
