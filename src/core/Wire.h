#pragma once

#include <QPointF>
#include <QVector>

class Wire
{
public:
    struct Endpoint {
        int componentId;
        int pinIndex;
    };

    Wire(int comp1, int pin1, int comp2, int pin2);

    int id() const { return m_id; }

    Endpoint from() const { return m_from; }
    Endpoint to() const { return m_to; }

    int nodeId() const { return m_nodeId; }
    void setNodeId(int id) { m_nodeId = id; }

    // Visual routing waypoints (corner points between start/end pins)
    const QVector<QPointF>& waypoints() const { return m_waypoints; }
    void setWaypoints(const QVector<QPointF>& waypoints) { m_waypoints = waypoints; }

private:
    static int s_nextId;
    int m_id;
    Endpoint m_from;
    Endpoint m_to;
    int m_nodeId = -1;
    QVector<QPointF> m_waypoints;
};
