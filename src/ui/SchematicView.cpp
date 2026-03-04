#include "SchematicView.h"
#include "SchematicScene.h"
#include "graphics/GraphicWire.h"
#include "graphics/GraphicPin.h"
#include "graphics/GraphicComponent.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QScrollBar>
#include <QMenu>
#include <QDebug>
#include <cmath>

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

    // Right-click: if a wire is selected, handle it entirely here
    if (event->button() == Qt::RightButton) {
        // Find selected wire
        GraphicWire* selectedWire = nullptr;
        if (auto* ss = dynamic_cast<SchematicScene*>(scene())) {
            for (auto* selItem : ss->selectedItems()) {
                selectedWire = dynamic_cast<GraphicWire*>(selItem);
                if (selectedWire) break;
            }
        }

        if (selectedWire && selectedWire->startPin() && selectedWire->endPin()) {
            // Wire is selected: cycle if in circle, do nothing otherwise
            QPointF scenePos = mapToScene(event->pos());
            QPointF p1 = selectedWire->startPin()->scenePos();
            QPointF p2 = selectedWire->endPin()->scenePos();
            QPointF center = (p1 + p2) / 2.0;
            qreal dx = p2.x() - p1.x(), dy = p2.y() - p1.y();
            qreal radius = std::sqrt(dx*dx + dy*dy) / 2.0;
            QPointF d = scenePos - center;
            qreal dist = std::sqrt(d.x()*d.x() + d.y()*d.y());
            qDebug() << "[View::PRESS] wire selected, dist=" << dist << "radius=" << radius;
            if (dist <= radius) {
                if (auto* ss = dynamic_cast<SchematicScene*>(scene()))
                    ss->notifyAboutToModify();
                selectedWire->cycleRoute();
                qDebug() << "[View::PRESS] CYCLED";
            } else {
                qDebug() << "[View::PRESS] outside circle, ignoring";
            }
            // Always consume — never forward to QGraphicsView when wire is selected
            m_handledRightClick = true;
            event->accept();
            return;
        }

        // No wire selected — pan on empty space, forward to item if present
        QGraphicsItem* item = itemAt(event->pos());
        if (!item) {
            m_panning = true;
            m_lastPanPoint = event->pos();
            setCursor(Qt::ClosedHandCursor);
            qDebug() << "[View::PRESS] right-click on empty space, panning";
            event->accept();
            return;
        }

        // Check if right-click is on a pin → oscilloscope probe menu
        if (auto* pin = dynamic_cast<GraphicPin*>(item)) {
            auto* parentComp = pin->parentComponent();
            if (parentComp) {
                qDebug() << "[View::PRESS] right-click on pin" << pin->pinIndex();
                showPinProbeMenu(event->globalPosition().toPoint(), parentComp, pin);
                event->accept();
                return;
            }
        }

        // Item found (component etc.) — save selection, forward, then restore if needed
        qDebug() << "[View::PRESS] right-click on item, forwarding:" << item;
        auto savedSelection = scene()->selectedItems();
        QGraphicsView::mousePressEvent(event);
        // Restore selection if Qt deselected
        if (scene()->selectedItems().isEmpty() && !savedSelection.isEmpty()) {
            for (auto* it : savedSelection)
                it->setSelected(true);
            qDebug() << "[View::PRESS] restored selection after right-click forward";
        }
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void SchematicView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Right double-click: treat exactly like a single right-click press
    // (Qt fires double-click on rapid clicks, bypassing mousePressEvent)
    if (event->button() == Qt::RightButton) {
        qDebug() << "[View::DBLCLICK] right double-click -> forwarding to mousePressEvent";
        mousePressEvent(event);
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(event);
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

    // Right-click release: never forward to QGraphicsView (prevents deselection)
    if (event->button() == Qt::RightButton) {
        m_handledRightClick = false;
        qDebug() << "[View::RELEASE] right-click consumed";
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void SchematicView::contextMenuEvent(QContextMenuEvent* event)
{
    // Block all context menu events to prevent deselection on right-click
    qDebug() << "[View::CONTEXT] context menu blocked";
    event->accept();
}

void SchematicView::showPinProbeMenu(const QPoint& globalPos, GraphicComponent* comp, GraphicPin* pin)
{
    auto* ss = dynamic_cast<SchematicScene*>(scene());
    if (!ss) return;

    int componentId = comp->coreComponent()->id();
    int pinIndex = pin->pinIndex();
    QString pinLabel = comp->coreComponent()->name() + "." + QString::number(pinIndex + 1);

    QMenu menu;
    menu.setTitle(pinLabel);

    auto* ch1Pos = menu.addAction("CH1 + : " + pinLabel);
    auto* ch1Neg = menu.addAction("CH1 \xe2\x88\x92 : " + pinLabel);
    menu.addSeparator();
    auto* ch2Pos = menu.addAction("CH2 + : " + pinLabel);
    auto* ch2Neg = menu.addAction("CH2 \xe2\x88\x92 : " + pinLabel);

    // Style the actions with channel colors
    ch1Pos->setIcon(QIcon());
    ch1Neg->setIcon(QIcon());
    ch2Pos->setIcon(QIcon());
    ch2Neg->setIcon(QIcon());

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    int channel = 0;
    bool positive = true;
    if (chosen == ch1Pos)      { channel = 1; positive = true; }
    else if (chosen == ch1Neg) { channel = 1; positive = false; }
    else if (chosen == ch2Pos) { channel = 2; positive = true; }
    else if (chosen == ch2Neg) { channel = 2; positive = false; }

    qDebug() << "[View::PROBE] assigned" << pinLabel << "to CH" << channel
             << (positive ? "+" : "-");

    emit ss->probeAssigned(componentId, pinIndex, channel, positive);
}
