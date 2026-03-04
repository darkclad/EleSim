#include "GDCSource.h"
#include "../GraphicPin.h"
#include "../../../core/components/DCSource.h"

#include <QPainter>
#include <QFont>

GDCSource::GDCSource(DCSource* source)
    : GraphicComponent(source)
{
    m_symbolRect = QRectF(-20, -20, 40, 40);
    setupPins();
}

void GDCSource::setupPins()
{
    // Pin 0 (positive) at top, Pin 1 (negative) at bottom
    auto* pin0 = new GraphicPin(0, this);
    pin0->setPos(0, -30);
    m_pins.push_back(pin0);

    auto* pin1 = new GraphicPin(1, this);
    pin1->setPos(0, 30);
    m_pins.push_back(pin1);
}

void GDCSource::drawSymbol(QPainter* painter)
{
    painter->setPen(symbolPen());
    painter->setBrush(Qt::NoBrush);

    // Circle
    painter->drawEllipse(QRectF(-16, -16, 32, 32));

    // Lead lines
    painter->drawLine(QPointF(0, -30), QPointF(0, -16));
    painter->drawLine(QPointF(0, 16), QPointF(0, 30));

    // + and - signs
    QFont font;
    font.setPixelSize(12);
    font.setBold(true);
    painter->setFont(font);
    painter->drawText(QRectF(-10, -14, 20, 14), Qt::AlignCenter, "+");
    painter->drawText(QRectF(-10, 0, 20, 14), Qt::AlignCenter, "\xe2\x80\x93"); // en-dash as minus
}
