#include "GSwitch.h"
#include "../GraphicPin.h"
#include "../../../core/components/Switch.h"

#include <QPainter>
#include <cmath>

GSwitch::GSwitch(Switch* sw)
    : GraphicComponent(sw)
{
    m_symbolRect = QRectF(-30, -15, 60, 30);
    setupPins();
}

void GSwitch::updateSwitchVisual()
{
    update();
}

void GSwitch::setupPins()
{
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(30, 0);
    m_pins.push_back(pin1);
}

void GSwitch::drawSymbol(QPainter* painter)
{
    auto* sw = static_cast<Switch*>(m_coreComponent);

    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Lead lines
    painter->drawLine(QPointF(-30, 0), QPointF(-12, 0));
    painter->drawLine(QPointF(12, 0), QPointF(30, 0));

    // Contact dots
    painter->setBrush(Qt::black);
    painter->drawEllipse(QPointF(-12, 0), 3, 3);
    painter->drawEllipse(QPointF(12, 0), 3, 3);

    // Switch arm
    painter->setBrush(Qt::NoBrush);
    if (sw->isClosed()) {
        // Closed: arm horizontal
        painter->setPen(QPen(Qt::darkGreen, 2.5));
        painter->drawLine(QPointF(-12, 0), QPointF(12, 0));
    } else {
        // Open: arm tilted up
        painter->setPen(QPen(Qt::darkRed, 2.5));
        painter->drawLine(QPointF(-12, 0), QPointF(10, -12));
    }
}
