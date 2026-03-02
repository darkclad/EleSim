#pragma once

#include <QGraphicsItem>
#include <vector>

class Component;
class GraphicPin;

class GraphicComponent : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit GraphicComponent(Component* coreComponent);
    ~GraphicComponent() override = default;

    Component* coreComponent() const { return m_coreComponent; }
    GraphicPin* graphicPin(int index) const;
    const std::vector<GraphicPin*>& pins() const { return m_pins; }

    void rotateBy(double degrees);
    void rotateClockwise();

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

signals:
    void componentMoved(GraphicComponent* comp);
    void componentDoubleClicked(Component* comp);
    void dragStarted();
    void dropCompleted(GraphicComponent* comp);
    void aboutToRotate();

protected:
    virtual void drawSymbol(QPainter* painter) = 0;
    virtual void setupPins() = 0;

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

    Component* m_coreComponent;
    std::vector<GraphicPin*> m_pins;
    QRectF m_symbolRect; // Bounding rect of the symbol (set by subclass)
    QPointF m_dragStartPos;  // Position at mouse press, for grid-snap-by-delta
    QPoint  m_pressScreenPos; // Mouse screen position at press, for threshold check
    bool m_mouseDown = false;  // Left button is held
    bool m_dragging = false;   // Actual drag started (past min distance threshold)
};
