#include "GSwitch4Way.h"
#include "../GraphicPin.h"
#include "../../../core/components/Switch4Way.h"

#include <QPainter>

GSwitch4Way::GSwitch4Way(Switch4Way* sw)
    : GraphicComponent(sw)
{
    m_symbolRect = QRectF(-30, -20, 60, 40);
    setupPins();
}

void GSwitch4Way::updateSwitchVisual()
{
    update();
}

void GSwitch4Way::setupPins()
{
    // Left side: pin0 (top), pin1 (bottom)
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(-30, -15);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(-30, 15);
    m_pins.push_back(pin1);

    // Right side: pin2 (top), pin3 (bottom)
    auto* pin2 = new GraphicPin(2, this);
    pin2->setPos(30, -15);
    m_pins.push_back(pin2);

    auto* pin3 = new GraphicPin(3, this);
    pin3->setPos(30, 15);
    m_pins.push_back(pin3);
}

void GSwitch4Way::drawSymbol(QPainter* painter)
{
    auto* sw = static_cast<Switch4Way*>(m_coreComponent);

    painter->setPen(symbolPen());
    painter->setBrush(Qt::NoBrush);

    // Lead lines
    painter->drawLine(QPointF(-30, -15), QPointF(-12, -15));
    painter->drawLine(QPointF(-30, 15), QPointF(-12, 15));
    painter->drawLine(QPointF(12, -15), QPointF(30, -15));
    painter->drawLine(QPointF(12, 15), QPointF(30, 15));

    // Contact dots
    painter->setBrush(Qt::black);
    painter->drawEllipse(QPointF(-12, -15), 3, 3);
    painter->drawEllipse(QPointF(-12, 15), 3, 3);
    painter->drawEllipse(QPointF(12, -15), 3, 3);
    painter->drawEllipse(QPointF(12, 15), 3, 3);

    // Connection lines
    painter->setBrush(Qt::NoBrush);
    painter->setPen(symbolPen(Qt::darkGreen, 2.5));
    if (!sw->isCrossed()) {
        // Straight: 0-2, 1-3
        painter->drawLine(QPointF(-12, -15), QPointF(12, -15));
        painter->drawLine(QPointF(-12, 15), QPointF(12, 15));
    } else {
        // Crossed: 0-3, 1-2
        painter->drawLine(QPointF(-12, -15), QPointF(12, 15));
        painter->drawLine(QPointF(-12, 15), QPointF(12, -15));
    }
}
