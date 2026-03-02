#include "GInductor.h"
#include "../GraphicPin.h"
#include "../../../core/components/Inductor.h"

#include <QPainter>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GInductor::GInductor(Inductor* inductor)
    : GraphicComponent(inductor)
{
    m_symbolRect = QRectF(-30, -10, 60, 20);
    setupPins();
}

void GInductor::setupPins()
{
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(-30, 0);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(30, 0);
    m_pins.push_back(pin1);
}

void GInductor::drawSymbol(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 2));
    painter->setBrush(Qt::NoBrush);

    // Lead lines
    painter->drawLine(QPointF(-30, 0), QPointF(-18, 0));
    painter->drawLine(QPointF(18, 0), QPointF(30, 0));

    // Four coil humps (arcs)
    const double humpWidth = 9.0;
    double x = -18.0;
    for (int i = 0; i < 4; ++i) {
        QRectF arcRect(x, -8, humpWidth, 16);
        painter->drawArc(arcRect, 0 * 16, 180 * 16); // top half arc
        x += humpWidth;
    }
}
