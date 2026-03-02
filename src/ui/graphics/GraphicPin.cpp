#include "GraphicPin.h"
#include "GraphicComponent.h"
#include "GraphicWire.h"

#include <QBrush>
#include <QPen>
#include <QGraphicsSceneHoverEvent>
#include <cmath>

static constexpr qreal GRID_SIZE = 20.0;

GraphicPin::GraphicPin(int pinIndex, GraphicComponent* parent)
    : QGraphicsEllipseItem(-PIN_RADIUS, -PIN_RADIUS, PIN_RADIUS * 2, PIN_RADIUS * 2, parent)
    , m_pinIndex(pinIndex)
    , m_parentComponent(parent)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setZValue(10); // Pins above components
    updateAppearance();
}

void GraphicPin::setConnected(bool connected)
{
    m_connected = connected;
    updateAppearance();
}

void GraphicPin::addWire(GraphicWire* wire)
{
    m_connectedWires.push_back(wire);
    setConnected(true);
}

void GraphicPin::removeWire(GraphicWire* wire)
{
    m_connectedWires.erase(
        std::remove(m_connectedWires.begin(), m_connectedWires.end(), wire),
        m_connectedWires.end());
    setConnected(!m_connectedWires.empty());
}

void GraphicPin::updateConnectedWires()
{
    for (auto* wire : m_connectedWires) {
        wire->updatePath();
    }
}

QPointF GraphicPin::sceneSnapPos() const
{
    QPointF sp = scenePos();
    return QPointF(
        std::round(sp.x() / GRID_SIZE) * GRID_SIZE,
        std::round(sp.y() / GRID_SIZE) * GRID_SIZE
    );
}

void GraphicPin::setHighlighted(bool highlighted)
{
    if (m_highlighted == highlighted) return;
    m_highlighted = highlighted;
    updateAppearance();
}

void GraphicPin::setProbeHighlight(const QColor& color)
{
    m_probeColor = color;
    updateAppearance();
}

void GraphicPin::clearProbeHighlight()
{
    m_probeColor = QColor(); // invalid
    updateAppearance();
}

void GraphicPin::hoverEnterEvent(QGraphicsSceneHoverEvent* /*event*/)
{
    if (!m_highlighted) {
        setPen(QPen(Qt::blue, 2));
        setBrush(QBrush(Qt::cyan));
    }
}

void GraphicPin::hoverLeaveEvent(QGraphicsSceneHoverEvent* /*event*/)
{
    updateAppearance();
}

void GraphicPin::updateAppearance()
{
    if (m_highlighted) {
        // Bright glow when a wire is being drawn nearby
        setPen(QPen(QColor(0, 200, 255), 3));
        setBrush(QBrush(QColor(0, 255, 200, 200)));
        setRect(-PIN_RADIUS * 1.8, -PIN_RADIUS * 1.8,
                PIN_RADIUS * 3.6, PIN_RADIUS * 3.6);
    } else if (m_probeColor.isValid()) {
        // Oscilloscope probe highlight — enlarged with probe color
        setPen(QPen(m_probeColor, 2.5));
        QColor fill = m_probeColor;
        fill.setAlpha(180);
        setBrush(QBrush(fill));
        setRect(-PIN_RADIUS * 1.5, -PIN_RADIUS * 1.5,
                PIN_RADIUS * 3.0, PIN_RADIUS * 3.0);
    } else {
        // Normal size
        setRect(-PIN_RADIUS, -PIN_RADIUS,
                PIN_RADIUS * 2, PIN_RADIUS * 2);
        if (m_connected) {
            setPen(QPen(Qt::darkGreen, 1.5));
            setBrush(QBrush(QColor(0, 180, 0)));
        } else {
            setPen(QPen(Qt::darkRed, 1.5));
            setBrush(QBrush(QColor(220, 60, 60)));
        }
    }
}
