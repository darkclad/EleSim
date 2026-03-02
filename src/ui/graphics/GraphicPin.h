#pragma once

#include <QGraphicsEllipseItem>
#include <vector>

class GraphicComponent;
class GraphicWire;

class GraphicPin : public QGraphicsEllipseItem
{
public:
    static constexpr qreal PIN_RADIUS = 4.0;

    GraphicPin(int pinIndex, GraphicComponent* parent);

    int pinIndex() const { return m_pinIndex; }
    GraphicComponent* parentComponent() const { return m_parentComponent; }

    void setConnected(bool connected);
    bool isConnected() const { return m_connected; }

    // Wire tracking
    void addWire(GraphicWire* wire);
    void removeWire(GraphicWire* wire);
    void updateConnectedWires();
    const std::vector<GraphicWire*>& connectedWires() const { return m_connectedWires; }

    // Snap position in scene coordinates
    QPointF sceneSnapPos() const;

    // Wire-drawing proximity highlight (called by SchematicScene)
    void setHighlighted(bool highlighted);
    bool isHighlighted() const { return m_highlighted; }

    // Oscilloscope probe highlight
    void setProbeHighlight(const QColor& color);
    void clearProbeHighlight();
    bool hasProbeHighlight() const { return m_probeColor.isValid(); }

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    int m_pinIndex;
    GraphicComponent* m_parentComponent;
    bool m_connected = false;
    bool m_highlighted = false;
    QColor m_probeColor;  // invalid = no probe highlight
    std::vector<GraphicWire*> m_connectedWires;

    void updateAppearance();
};
