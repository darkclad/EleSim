#pragma once

#include <QGraphicsView>

class SchematicScene;
class GraphicComponent;
class GraphicPin;

class SchematicView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit SchematicView(SchematicScene* scene, QWidget* parent = nullptr);
    ~SchematicView() override = default;

    // View state for save/load
    double zoom() const { return m_currentZoom; }
    QPointF viewCenter() const;
    void setViewState(double zoom, const QPointF& center);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void showPinProbeMenu(const QPoint& globalPos, GraphicComponent* comp, GraphicPin* pin);

    static constexpr double MIN_ZOOM = 0.1;
    static constexpr double MAX_ZOOM = 10.0;
    static constexpr double ZOOM_STEP = 1.15;

    double m_currentZoom = 1.0;
    bool   m_panning = false;
    bool   m_handledRightClick = false;  // Blocks release after wire cycle
    QPoint m_lastPanPoint;
};
