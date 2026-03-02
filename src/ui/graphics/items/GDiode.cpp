#include "GDiode.h"
#include "../GraphicPin.h"
#include "../../../core/components/Diode.h"

#include <QPainter>
#include <QPolygonF>

GDiode::GDiode(Diode* diode)
    : GraphicComponent(diode)
{
    m_symbolRect = QRectF(-30, -10, 60, 20);
    setupPins();
}

void GDiode::setupPins()
{
    auto* pin0 = new GraphicPin(0, this); // Anode
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this); // Cathode
    pin1->setPos(30, 0);
    m_pins.push_back(pin1);
}

void GDiode::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Lead lines
    painter->drawLine(QPointF(-30, 0), QPointF(-8, 0));
    painter->drawLine(QPointF(8, 0), QPointF(30, 0));

    // Triangle (anode side, pointing right)
    QPolygonF triangle;
    triangle << QPointF(-8, -10) << QPointF(8, 0) << QPointF(-8, 10);
    painter->setBrush(Qt::black);
    painter->drawPolygon(triangle);

    // Cathode bar
    painter->setPen(QPen(Qt::black, 3));
    painter->drawLine(QPointF(8, -10), QPointF(8, 10));
}
