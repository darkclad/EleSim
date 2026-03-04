#include "GNOTGate.h"
#include "../GraphicPin.h"
#include "../../../core/components/NOTGate.h"

#include <QPainter>
#include <QPolygonF>

GNOTGate::GNOTGate(NOTGate* gate)
    : GraphicComponent(gate)
{
    m_symbolRect = QRectF(-35, -25, 70, 50);
    setupPins();
}

void GNOTGate::setupPins()
{
    auto* pin0 = new GraphicPin(0, this); // A (input)
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this); // Y (output)
    pin1->setPos(30, 0);
    m_pins.push_back(pin1);

    auto* pin2 = new GraphicPin(2, this); // VDD
    pin2->setPos(0, -20);
    m_pins.push_back(pin2);

    auto* pin3 = new GraphicPin(3, this); // GND
    pin3->setPos(0, 20);
    m_pins.push_back(pin3);
}

void GNOTGate::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Input lead
    painter->drawLine(QPointF(-30, 0), QPointF(-15, 0));

    // Triangle body
    QPolygonF tri;
    tri << QPointF(-15, -15) << QPointF(-15, 15) << QPointF(15, 0);
    painter->drawPolygon(tri);

    // Inversion bubble
    painter->drawEllipse(QPointF(19, 0), 4, 4);

    // Output lead
    painter->drawLine(QPointF(23, 0), QPointF(30, 0));

    // VDD lead
    painter->drawLine(QPointF(0, -20), QPointF(0, -15));
    // GND lead
    painter->drawLine(QPointF(0, 20), QPointF(0, 15));

    // Labels
    painter->setFont(QFont("sans-serif", 6));
    painter->drawText(QPointF(3, -16), "V+");
    painter->drawText(QPointF(3, 22), "G");
}
