#include "GraphicComponent.h"
#include "GraphicPin.h"
#include "../../core/Component.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QFont>
#include <QDebug>
#include <cmath>

static constexpr qreal GRID_SIZE = 20.0;
static constexpr qreal DRAG_THRESHOLD_PX = 5.0; // min screen-pixels before drag starts

GraphicComponent::GraphicComponent(Component* coreComponent)
    : m_coreComponent(coreComponent)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setZValue(5); // Components above background, below pins
}

GraphicPin* GraphicComponent::graphicPin(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_pins.size()))
        return m_pins[index];
    return nullptr;
}

void GraphicComponent::rotateBy(double degrees)
{
    setRotation(std::fmod(rotation() + degrees, 360.0));
    if (m_coreComponent)
        m_coreComponent->setRotation(rotation());
}

void GraphicComponent::rotateClockwise()
{
    rotateBy(90.0);
}

QRectF GraphicComponent::boundingRect() const
{
    // Add margin for selection highlight and labels
    return m_symbolRect.adjusted(-5, -15, 5, 15);
}

void GraphicComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                              QWidget* /*widget*/)
{
    // Selection highlight
    if (option->state & QStyle::State_Selected) {
        QPen selPen(QColor(0, 120, 215), 1.5, Qt::DashLine);
        painter->setPen(selPen);
        painter->setBrush(QBrush(QColor(0, 120, 215, 30)));
        painter->drawRect(m_symbolRect.adjusted(-3, -3, 3, 3));
    }

    // Draw the component symbol (implemented by subclass)
    drawSymbol(painter);

    // Draw component name and value labels
    if (m_coreComponent) {
        QFont font;
        font.setPixelSize(10);
        painter->setFont(font);
        painter->setPen(Qt::black);

        // Name above
        painter->drawText(m_symbolRect.left(), m_symbolRect.top() - 4,
                          m_coreComponent->name());
        // Value below
        painter->drawText(m_symbolRect.left(), m_symbolRect.bottom() + 12,
                          m_coreComponent->valueString());
    }
}

QVariant GraphicComponent::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange) {
        QPointF proposed = value.toPointF();
        qDebug() << "itemChange POSITION_CHANGE proposed=" << proposed
                 << "current=" << pos() << "dragStart=" << m_dragStartPos
                 << "mouseDown=" << m_mouseDown << "dragging=" << m_dragging;

        if (m_mouseDown) {
            if (!m_dragging) {
                // Not yet dragging — stay at original position
                // (threshold is checked in mouseMoveEvent using screen pixels)
                return m_dragStartPos;
            }

            // Snap the delta from drag start to grid
            QPointF delta = proposed - m_dragStartPos;
            delta.setX(std::round(delta.x() / GRID_SIZE) * GRID_SIZE);
            delta.setY(std::round(delta.y() / GRID_SIZE) * GRID_SIZE);
            return m_dragStartPos + delta;
        }

        // Non-drag position change (e.g., setPos) — absolute grid snap
        QPointF newPos = proposed;
        newPos.setX(std::round(newPos.x() / GRID_SIZE) * GRID_SIZE);
        newPos.setY(std::round(newPos.y() / GRID_SIZE) * GRID_SIZE);
        return newPos;
    }

    if (change == ItemPositionHasChanged) {
        // Sync position to core model
        if (m_coreComponent)
            m_coreComponent->setPosition(pos());
        // Update connected wires
        for (auto* pin : m_pins) {
            pin->updateConnectedWires();
        }
        emit componentMoved(this);
    }

    return QGraphicsObject::itemChange(change, value);
}

void GraphicComponent::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    qDebug() << "mousePressEvent button=" << event->button() << "pos=" << pos()
             << "scenePos=" << event->scenePos();
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = pos();
        m_pressScreenPos = event->screenPos();
        m_mouseDown = true;
        m_dragging = false;
    } else if (event->button() == Qt::RightButton) {
        emit aboutToRotate();
        double angle = (event->modifiers() & Qt::ShiftModifier) ? 45.0 : 90.0;
        rotateBy(angle);
        emit componentMoved(this); // trigger wire update & sim recalc
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void GraphicComponent::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_mouseDown && !m_dragging) {
        // Check screen-pixel distance to decide if drag should start
        QPoint screenDelta = event->screenPos() - m_pressScreenPos;
        double dist = std::sqrt(screenDelta.x() * screenDelta.x()
                                + screenDelta.y() * screenDelta.y());
        qDebug() << "mouseMoveEvent screenDist=" << dist
                 << "threshold=" << DRAG_THRESHOLD_PX;
        if (dist < DRAG_THRESHOLD_PX) {
            // Not enough movement — swallow the event, don't move
            event->accept();
            return;
        }
        // Threshold exceeded — start dragging
        m_dragging = true;
        emit dragStarted();
    }
    QGraphicsObject::mouseMoveEvent(event);
}

void GraphicComponent::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    qDebug() << "mouseReleaseEvent button=" << event->button() << "pos=" << pos();
    if (event->button() == Qt::LeftButton) {
        bool wasDragging = m_dragging;
        m_mouseDown = false;
        m_dragging = false;
        if (wasDragging)
            emit dropCompleted(this);
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void GraphicComponent::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_coreComponent)
        emit componentDoubleClicked(m_coreComponent);
    QGraphicsObject::mouseDoubleClickEvent(event);
}
