#include "SchematicView.h"
#include "SchematicScene.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>

SchematicView::SchematicView(SchematicScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    // RubberBandDrag enables drag-to-select on empty space
    // Ctrl+click toggle selection is handled natively by Qt
    setDragMode(QGraphicsView::RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);

    centerOn(0, 0);
}

QPointF SchematicView::viewCenter() const
{
    return mapToScene(viewport()->rect().center());
}

void SchematicView::setViewState(double zoom, const QPointF& center)
{
    // Reset transform and apply saved zoom
    resetTransform();
    m_currentZoom = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
    scale(m_currentZoom, m_currentZoom);
    centerOn(center);
}

void SchematicView::wheelEvent(QWheelEvent* event)
{
    double angle = event->angleDelta().y();

    if (angle > 0 && m_currentZoom < MAX_ZOOM) {
        double factor = ZOOM_STEP;
        m_currentZoom *= factor;
        if (m_currentZoom > MAX_ZOOM) {
            factor = MAX_ZOOM / (m_currentZoom / factor);
            m_currentZoom = MAX_ZOOM;
        }
        scale(factor, factor);
    } else if (angle < 0 && m_currentZoom > MIN_ZOOM) {
        double factor = 1.0 / ZOOM_STEP;
        m_currentZoom *= factor;
        if (m_currentZoom < MIN_ZOOM) {
            factor = MIN_ZOOM / (m_currentZoom * ZOOM_STEP);
            m_currentZoom = MIN_ZOOM;
        }
        scale(factor, factor);
    }

    event->accept();
}

void SchematicView::mousePressEvent(QMouseEvent* event)
{
    // Middle-click to pan
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    // Right-click on empty space to pan; on item let it through (e.g. rotate)
    if (event->button() == Qt::RightButton) {
        QGraphicsItem* item = itemAt(event->pos());
        if (!item) {
            m_panning = true;
            m_lastPanPoint = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void SchematicView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());

        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void SchematicView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}
