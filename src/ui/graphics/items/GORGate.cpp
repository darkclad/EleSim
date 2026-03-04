#include "GORGate.h"
#include "../GraphicPin.h"
#include "../../../core/components/ORGate.h"

#include <QPainter>
#include <QPainterPath>

GORGate::GORGate(ORGate* gate)
    : GraphicComponent(gate)
{
    m_symbolRect = QRectF(-35, -30, 70, 60);
    setupPins();
}

void GORGate::setupPins()
{
    auto* pin0 = new GraphicPin(0, this); // A
    pin0->setPos(-30, -10);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this); // B
    pin1->setPos(-30, 10);
    m_pins.push_back(pin1);

    auto* pin2 = new GraphicPin(2, this); // Y
    pin2->setPos(30, 0);
    m_pins.push_back(pin2);

    auto* pin3 = new GraphicPin(3, this); // VDD
    pin3->setPos(0, -25);
    m_pins.push_back(pin3);

    auto* pin4 = new GraphicPin(4, this); // GND
    pin4->setPos(0, 25);
    m_pins.push_back(pin4);
}

void GORGate::drawSymbol(QPainter* painter)
{
    painter->setPen(symbolPen());
    painter->setBrush(Qt::NoBrush);

    // Input leads
    painter->drawLine(QPointF(-30, -10), QPointF(-12, -10));
    painter->drawLine(QPointF(-30, 10), QPointF(-12, 10));

    // OR body: curved input side + pointed output
    QPainterPath path;
    path.moveTo(-15, -18);
    path.quadTo(15, -18, 18, 0);   // top curve to output point
    path.quadTo(15, 18, -15, 18);  // bottom curve from output
    path.quadTo(-5, 0, -15, -18);  // input curve
    painter->drawPath(path);

    // Output lead
    painter->drawLine(QPointF(18, 0), QPointF(30, 0));

    // VDD lead
    painter->drawLine(QPointF(0, -25), QPointF(0, -15));
    // GND lead
    painter->drawLine(QPointF(0, 25), QPointF(0, 15));

    // Labels
    painter->setFont(QFont("sans-serif", 6));
    painter->drawText(QPointF(3, -16), "V+");
    painter->drawText(QPointF(3, 25), "G");
}
