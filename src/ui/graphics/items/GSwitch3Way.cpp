#include "GSwitch3Way.h"
#include "../GraphicPin.h"
#include "../../../core/components/Switch3Way.h"

#include <QPainter>

GSwitch3Way::GSwitch3Way(Switch3Way* sw)
    : GraphicComponent(sw)
{
    m_symbolRect = QRectF(-30, -20, 60, 40);
    setupPins();
}

void GSwitch3Way::updateSwitchVisual()
{
    update();
}

void GSwitch3Way::setupPins()
{
    // Pin 0: common (left)
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    // Pin 1: position A (right top)
    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(30, -15);
    m_pins.push_back(pin1);

    // Pin 2: position B (right bottom)
    auto* pin2 = new GraphicPin(2, this);
    pin2->setPos(30, 15);
    m_pins.push_back(pin2);
}

void GSwitch3Way::drawSymbol(QPainter* painter)
{
    auto* sw = static_cast<Switch3Way*>(m_coreComponent);

    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Lead lines
    painter->drawLine(QPointF(-30, 0), QPointF(-12, 0));
    painter->drawLine(QPointF(12, -15), QPointF(30, -15));
    painter->drawLine(QPointF(12, 15), QPointF(30, 15));

    // Contact dots
    painter->setBrush(Qt::black);
    painter->drawEllipse(QPointF(-12, 0), 3, 3);
    painter->drawEllipse(QPointF(12, -15), 3, 3);
    painter->drawEllipse(QPointF(12, 15), 3, 3);

    // Switch arm from common to active position
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(Qt::darkGreen, 2.5));
    if (sw->activePosition() == 0) {
        // Connected to pin1 (top)
        painter->drawLine(QPointF(-12, 0), QPointF(12, -15));
    } else {
        // Connected to pin2 (bottom)
        painter->drawLine(QPointF(-12, 0), QPointF(12, 15));
    }
}
