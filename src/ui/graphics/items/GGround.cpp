#include "GGround.h"
#include "../GraphicPin.h"
#include "../../../core/components/Ground.h"

#include <QPainter>

GGround::GGround(Ground* ground)
    : GraphicComponent(ground)
{
    m_symbolRect = QRectF(-10, -4, 20, 20);
    setupPins();
}

void GGround::setupPins()
{
    auto* pin = new GraphicPin(0, this);
    pin->setPos(0, 0);
    m_pins.push_back(pin);
}

void GGround::drawSymbol(QPainter* painter)
{
    painter->setPen(symbolPen());

    // Vertical line from pin down
    painter->drawLine(QPointF(0, 0), QPointF(0, 6));

    // Three horizontal lines of decreasing width
    painter->drawLine(QPointF(-10, 6), QPointF(10, 6));
    painter->drawLine(QPointF(-6, 10), QPointF(6, 10));
    painter->drawLine(QPointF(-3, 14), QPointF(3, 14));
}
