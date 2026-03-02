#pragma once

#include <QGraphicsPathItem>
#include <QVector>
#include <QPointF>

class GraphicPin;
class Wire;

class GraphicWire : public QGraphicsPathItem
{
public:
    GraphicWire(GraphicPin* startPin, GraphicPin* endPin, Wire* coreWire);

    Wire* coreWire() const { return m_coreWire; }
    GraphicPin* startPin() const { return m_startPin; }
    GraphicPin* endPin() const { return m_endPin; }

    void updatePath();

    void setHighlighted(bool highlighted);
    bool isHighlighted() const { return m_highlighted; }

    void setPowered(bool powered);
    bool isPowered() const { return m_powered; }

    // Waypoint management
    void setWaypoints(const QVector<QPointF>& waypoints);
    const QVector<QPointF>& waypoints() const { return m_waypoints; }
    bool isUserRouted() const { return m_userRouted; }

    // Legacy compatibility
    static QPainterPath manhattanRoute(QPointF start, QPointF end);

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    // Path building
    static QPainterPath buildPath(QPointF start, const QVector<QPointF>& waypoints, QPointF end);

    // Auto-routing
    QVector<QPointF> autoRoute(QPointF start, QPointF end) const;
    QVector<QRectF> getObstacles() const;
    static bool segmentIntersectsRect(const QPointF& a, const QPointF& b, const QRectF& rect);
    static int countRouteIntersections(QPointF start, const QVector<QPointF>& waypoints,
                                       QPointF end, const QVector<QRectF>& obstacles);

    // Segment dragging
    int segmentAt(const QPointF& scenePos) const;
    static qreal distToSegment(const QPointF& p, const QPointF& a, const QPointF& b);

    GraphicPin* m_startPin;
    GraphicPin* m_endPin;
    Wire* m_coreWire;
    bool m_highlighted = false;
    bool m_powered = false;

    // Waypoint routing
    QVector<QPointF> m_waypoints;
    bool m_userRouted = false;

    // Previous pin positions for waypoint shifting on component move
    QPointF m_prevStartPos;
    QPointF m_prevEndPos;

    // Segment dragging state
    int m_dragSegment = -1;
    QVector<QPointF> m_dragOrigWaypoints;
};
