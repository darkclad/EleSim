#pragma once

#include <QGraphicsView>

class SchematicScene;

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
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static constexpr double MIN_ZOOM = 0.1;
    static constexpr double MAX_ZOOM = 10.0;
    static constexpr double ZOOM_STEP = 1.15;

    double m_currentZoom = 1.0;
    bool   m_panning = false;
    QPoint m_lastPanPoint;
};
