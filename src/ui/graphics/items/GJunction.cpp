#include "GJunction.h"
#include "../GraphicPin.h"
#include "../../../core/components/Junction.h"

#include <QPainter>

GJunction::GJunction(Junction* junction)
    : GraphicComponent(junction)
{
    m_symbolRect = QRectF(-6, -6, 12, 12);
    setupPins();
}

void GJunction::setupPins()
{
    // All 3 pins at center - junction is a single electrical point
    for (int i = 0; i < 3; ++i) {
        auto* pin = new GraphicPin(i, this);
        pin->setPos(0, 0);
        m_pins.push_back(pin);
    }
}

void GJunction::drawSymbol(QPainter* painter)
{
    painter->setPen(symbolPen(Qt::darkGreen, 1.5));
    painter->setBrush(QBrush(Qt::darkGreen));
    painter->drawEllipse(QPointF(0, 0), 4.0, 4.0);
}
