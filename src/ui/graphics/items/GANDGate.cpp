#include "GANDGate.h"
#include "../GraphicPin.h"
#include "../../../core/components/ANDGate.h"

#include <QPainter>
#include <QPainterPath>

GANDGate::GANDGate(ANDGate* gate)
    : GraphicComponent(gate)
{
    m_symbolRect = QRectF(-35, -30, 70, 60);
    setupPins();
}

void GANDGate::setupPins()
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

void GANDGate::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Input leads
    painter->drawLine(QPointF(-30, -10), QPointF(-15, -10));
    painter->drawLine(QPointF(-30, 10), QPointF(-15, 10));

    // AND body: flat left, curved right (D-shape)
    QPainterPath path;
    path.moveTo(-15, -18);
    path.lineTo(0, -18);
    path.arcTo(QRectF(-2, -18, 36, 36), 90, -180);
    path.lineTo(-15, 18);
    path.closeSubpath();
    painter->drawPath(path);

    // Output lead
    painter->drawLine(QPointF(16, 0), QPointF(30, 0));

    // VDD lead
    painter->drawLine(QPointF(0, -25), QPointF(0, -18));
    // GND lead
    painter->drawLine(QPointF(0, 25), QPointF(0, 18));

    // Labels
    painter->setFont(QFont("sans-serif", 6));
    painter->drawText(QPointF(3, -19), "V+");
    painter->drawText(QPointF(3, 25), "G");
}
