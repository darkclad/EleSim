#include "GraphicWire.h"
#include "GraphicPin.h"
#include "GraphicComponent.h"
#include "../../core/Wire.h"
#include "../SchematicScene.h"

#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <cmath>
#include <algorithm>

static constexpr qreal GRID_SIZE = 20.0;

static QPointF snapToGrid(const QPointF& pos)
{
    return QPointF(
        std::round(pos.x() / GRID_SIZE) * GRID_SIZE,
        std::round(pos.y() / GRID_SIZE) * GRID_SIZE
    );
}

GraphicWire::GraphicWire(GraphicPin* startPin, GraphicPin* endPin, Wire* coreWire)
    : m_startPin(startPin)
    , m_endPin(endPin)
    , m_coreWire(coreWire)
    , m_prevStartPos(startPin ? startPin->scenePos() : QPointF())
    , m_prevEndPos(endPin ? endPin->scenePos() : QPointF())
{
    setZValue(1); // Wires below components
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    updatePath();
}

// --- Path building ---

void GraphicWire::updatePath()
{
    if (!m_startPin || !m_endPin) return;

    QPointF start = m_startPin->scenePos();
    QPointF end = m_endPin->scenePos();

    if (m_userRouted && !m_waypoints.isEmpty()) {
        // Shift waypoints when an endpoint moves (component drag)
        QPointF deltaStart = start - m_prevStartPos;
        QPointF deltaEnd = end - m_prevEndPos;
        bool startMoved = std::abs(deltaStart.x()) > 0.5 || std::abs(deltaStart.y()) > 0.5;
        bool endMoved = std::abs(deltaEnd.x()) > 0.5 || std::abs(deltaEnd.y()) > 0.5;

        if (startMoved || endMoved) {
            QPointF delta;
            if (startMoved && !endMoved) delta = deltaStart;
            else if (endMoved && !startMoved) delta = deltaEnd;
            else delta = (deltaStart + deltaEnd) / 2.0;

            for (auto& wp : m_waypoints) {
                wp += delta;
            }
            if (m_coreWire)
                m_coreWire->setWaypoints(m_waypoints);
        }
    } else {
        m_waypoints = autoRoute(start, end);
    }

    m_prevStartPos = start;
    m_prevEndPos = end;
    setPath(buildPath(start, m_waypoints, end));
}

QPainterPath GraphicWire::buildPath(QPointF start, const QVector<QPointF>& waypoints, QPointF end)
{
    QPainterPath path;
    path.moveTo(start);

    QPointF prev = start;
    auto addSegmentTo = [&](const QPointF& next) {
        bool sameX = std::abs(prev.x() - next.x()) < 0.5;
        bool sameY = std::abs(prev.y() - next.y()) < 0.5;

        if (sameX || sameY) {
            path.lineTo(next);
        } else {
            // Manhattan jog: horizontal first, then vertical
            path.lineTo(QPointF(next.x(), prev.y()));
            path.lineTo(next);
        }
        prev = next;
    };

    for (const auto& wp : waypoints) {
        addSegmentTo(wp);
    }
    addSegmentTo(end);

    return path;
}

QPainterPath GraphicWire::manhattanRoute(QPointF start, QPointF end)
{
    // Legacy: simple L-route
    return buildPath(start, {}, end);
}

void GraphicWire::setWaypoints(const QVector<QPointF>& waypoints)
{
    m_waypoints = waypoints;
    m_userRouted = !waypoints.isEmpty();
    updatePath();
}

// --- Auto-routing ---

QVector<QPointF> GraphicWire::autoRoute(QPointF start, QPointF end) const
{
    // Straight line - no waypoints needed
    if (std::abs(start.x() - end.x()) < 0.5 || std::abs(start.y() - end.y()) < 0.5) {
        return {};
    }

    QVector<QRectF> obstacles = getObstacles();

    // L-route A: horizontal first (corner at end.x, start.y)
    QVector<QPointF> routeA = {QPointF(end.x(), start.y())};
    int scoreA = countRouteIntersections(start, routeA, end, obstacles);

    // L-route B: vertical first (corner at start.x, end.y)
    QVector<QPointF> routeB = {QPointF(start.x(), end.y())};
    int scoreB = countRouteIntersections(start, routeB, end, obstacles);

    if (scoreA == 0) return routeA;
    if (scoreB == 0) return routeB;

    // Both intersect - return the one with fewer intersections
    return (scoreA <= scoreB) ? routeA : routeB;
}

QVector<QRectF> GraphicWire::getObstacles() const
{
    QVector<QRectF> obstacles;
    auto* s = scene();
    if (!s) return obstacles;

    for (auto* item : s->items()) {
        auto* gc = dynamic_cast<GraphicComponent*>(item);
        if (!gc) continue;
        // Exclude the wire's own endpoint components
        if (gc == m_startPin->parentComponent()) continue;
        if (gc == m_endPin->parentComponent()) continue;
        // Skip ghost components (opacity < 1)
        if (gc->opacity() < 0.9) continue;

        QRectF rect = gc->sceneBoundingRect();
        rect.adjust(-5, -5, 5, 5); // small padding
        obstacles.append(rect);
    }
    return obstacles;
}

bool GraphicWire::segmentIntersectsRect(const QPointF& a, const QPointF& b, const QRectF& rect)
{
    if (std::abs(a.y() - b.y()) < 0.5) {
        // Horizontal segment
        qreal y = a.y();
        qreal x1 = std::min(a.x(), b.x());
        qreal x2 = std::max(a.x(), b.x());
        return y >= rect.top() && y <= rect.bottom() &&
               x2 >= rect.left() && x1 <= rect.right();
    }
    if (std::abs(a.x() - b.x()) < 0.5) {
        // Vertical segment
        qreal x = a.x();
        qreal y1 = std::min(a.y(), b.y());
        qreal y2 = std::max(a.y(), b.y());
        return x >= rect.left() && x <= rect.right() &&
               y2 >= rect.top() && y1 <= rect.bottom();
    }
    return false; // Non-manhattan segment - skip
}

int GraphicWire::countRouteIntersections(QPointF start, const QVector<QPointF>& waypoints,
                                          QPointF end, const QVector<QRectF>& obstacles)
{
    int count = 0;
    QVector<QPointF> allPoints;
    allPoints << start << waypoints << end;

    for (int i = 0; i < allPoints.size() - 1; ++i) {
        for (const auto& rect : obstacles) {
            if (segmentIntersectsRect(allPoints[i], allPoints[i + 1], rect))
                ++count;
        }
    }
    return count;
}

// --- Highlighting ---

void GraphicWire::setHighlighted(bool highlighted)
{
    if (m_highlighted == highlighted) return;
    m_highlighted = highlighted;
    update();
}

void GraphicWire::setPowered(bool powered)
{
    if (m_powered == powered) return;
    m_powered = powered;
    update();
}

// --- Painting ---

void GraphicWire::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* /*widget*/)
{
    QPen wirePen;
    if (m_highlighted) {
        wirePen = QPen(QColor(255, 165, 0), 3.5, Qt::DashLine);
    } else if (option->state & QStyle::State_Selected) {
        wirePen = QPen(QColor(0, 120, 215), 2.5);
    } else {
        QColor wireColor = m_powered ? QColor(0, 160, 0) : QColor(200, 0, 0);
        wirePen = QPen(wireColor, 2.0);
    }

    painter->setPen(wirePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());
}

// --- Segment dragging ---

qreal GraphicWire::distToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    QPointF ab = b - a;
    QPointF ap = p - a;
    qreal lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq < 1e-9) {
        return std::sqrt(ap.x() * ap.x() + ap.y() * ap.y());
    }
    qreal t = (ap.x() * ab.x() + ap.y() * ab.y()) / lenSq;
    t = std::clamp(t, 0.0, 1.0);
    QPointF closest = a + t * ab;
    QPointF diff = p - closest;
    return std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
}

int GraphicWire::segmentAt(const QPointF& scenePos) const
{
    QVector<QPointF> allPoints;
    allPoints << m_startPin->scenePos() << m_waypoints << m_endPin->scenePos();

    for (int i = 0; i < allPoints.size() - 1; ++i) {
        qreal dist = distToSegment(scenePos, allPoints[i], allPoints[i + 1]);
        if (dist < 10.0) {
            return i;
        }
    }
    return -1;
}

void GraphicWire::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isSelected()) {
        // Wire is already selected - start segment drag
        int seg = segmentAt(event->scenePos());
        if (seg >= 0) {
            // Save undo state before modifying wire route
            if (auto* ss = dynamic_cast<SchematicScene*>(scene()))
                ss->notifyAboutToModify();
            m_dragSegment = seg;
            m_dragOrigWaypoints = m_waypoints;
            event->accept();
            return;
        }
    }
    QGraphicsPathItem::mousePressEvent(event);
}

void GraphicWire::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragSegment < 0) {
        QGraphicsPathItem::mouseMoveEvent(event);
        return;
    }

    QPointF start = m_startPin->scenePos();
    QPointF end = m_endPin->scenePos();

    QVector<QPointF> origAll;
    origAll << start << m_dragOrigWaypoints << end;

    int i = m_dragSegment;
    int j = i + 1;
    QPointF pi = origAll[i];
    QPointF pj = origAll[j];

    bool isHorizontal = std::abs(pi.y() - pj.y()) < 0.5;
    QPointF mousePos = event->scenePos();

    if (isHorizontal) {
        qreal newY = std::round(mousePos.y() / GRID_SIZE) * GRID_SIZE;

        if (i == 0 && j == origAll.size() - 1) {
            // Only segment (start → end, no waypoints originally)
            m_waypoints = {QPointF(pi.x(), newY), QPointF(pj.x(), newY)};
        } else if (i == 0) {
            // First segment - pin is fixed, insert new waypoints
            m_waypoints.clear();
            m_waypoints << QPointF(pi.x(), newY) << QPointF(pj.x(), newY);
            m_waypoints << m_dragOrigWaypoints;
        } else if (j == origAll.size() - 1) {
            // Last segment - end pin is fixed
            m_waypoints = m_dragOrigWaypoints;
            m_waypoints << QPointF(pi.x(), newY) << QPointF(pj.x(), newY);
        } else {
            // Interior segment - move both waypoints
            m_waypoints = m_dragOrigWaypoints;
            m_waypoints[i - 1].setY(newY);
            m_waypoints[j - 1].setY(newY);
        }
    } else {
        // Vertical segment - drag horizontally
        qreal newX = std::round(mousePos.x() / GRID_SIZE) * GRID_SIZE;

        if (i == 0 && j == origAll.size() - 1) {
            m_waypoints = {QPointF(newX, pi.y()), QPointF(newX, pj.y())};
        } else if (i == 0) {
            m_waypoints.clear();
            m_waypoints << QPointF(newX, pi.y()) << QPointF(newX, pj.y());
            m_waypoints << m_dragOrigWaypoints;
        } else if (j == origAll.size() - 1) {
            m_waypoints = m_dragOrigWaypoints;
            m_waypoints << QPointF(newX, pi.y()) << QPointF(newX, pj.y());
        } else {
            m_waypoints = m_dragOrigWaypoints;
            m_waypoints[i - 1].setX(newX);
            m_waypoints[j - 1].setX(newX);
        }
    }

    setPath(buildPath(start, m_waypoints, end));
    event->accept();
}

void GraphicWire::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragSegment >= 0) {
        m_dragSegment = -1;
        m_userRouted = true;

        // Sync waypoints to core wire
        if (m_coreWire) {
            m_coreWire->setWaypoints(m_waypoints);
        }

        event->accept();
        return;
    }
    QGraphicsPathItem::mouseReleaseEvent(event);
}
