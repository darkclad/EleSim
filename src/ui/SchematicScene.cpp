#include "SchematicScene.h"
#include <QGraphicsView>
#include "../core/Circuit.h"
#include "../simulation/MNASolver.h"
#include "../core/components/Resistor.h"
#include "../core/components/Capacitor.h"
#include "../core/components/DCSource.h"
#include "../core/components/ACSource.h"
#include "../core/components/Lamp.h"
#include "../core/components/Junction.h"
#include "../core/components/Switch.h"
#include "../core/components/Switch3Way.h"
#include "../core/components/Switch4Way.h"
#include "../core/components/Inductor.h"
#include "../core/components/Diode.h"
#include "../core/components/ZenerDiode.h"
#include "../core/components/NMosfet.h"
#include "../core/components/PMosfet.h"
#include "../core/components/NOTGate.h"
#include "../core/components/ANDGate.h"
#include "../core/components/ORGate.h"
#include "../core/components/XORGate.h"
#include "../core/components/Ground.h"
#include "../core/components/DCCurrentSource.h"
#include "../core/components/PulseSource.h"
#include "graphics/GraphicComponent.h"
#include "graphics/GraphicPin.h"
#include "graphics/GraphicWire.h"
#include "graphics/items/GResistor.h"
#include "graphics/items/GCapacitor.h"
#include "graphics/items/GDCSource.h"
#include "graphics/items/GACSource.h"
#include "graphics/items/GLamp.h"
#include "graphics/items/GJunction.h"
#include "graphics/items/GSwitch.h"
#include "graphics/items/GSwitch3Way.h"
#include "graphics/items/GSwitch4Way.h"
#include "graphics/items/GInductor.h"
#include "graphics/items/GDiode.h"
#include "graphics/items/GZenerDiode.h"
#include "graphics/items/GNMosfet.h"
#include "graphics/items/GPMosfet.h"
#include "graphics/items/GNOTGate.h"
#include "graphics/items/GANDGate.h"
#include "graphics/items/GORGate.h"
#include "graphics/items/GXORGate.h"
#include "graphics/items/GGround.h"
#include "graphics/items/GDCCurrentSource.h"
#include "graphics/items/GPulseSource.h"

#include <QPainter>
#include <QPen>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QKeyEvent>
#include <QMimeData>
#include <cmath>
#include <algorithm>
#include <set>
#include <map>

SchematicScene::SchematicScene(QObject* parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-2000, -1500, 4000, 3000);

    connect(this, &QGraphicsScene::selectionChanged,
            this, &SchematicScene::onSelectionChanged);
}

SchematicScene::~SchematicScene()
{
    disconnect(this, &QGraphicsScene::selectionChanged,
               this, &SchematicScene::onSelectionChanged);
    destroyGhostComponent();
}

void SchematicScene::setMode(SceneMode mode)
{
    if (m_mode == mode) return;

    // Clean up previous mode
    if (m_mode == SceneMode::DrawWire) {
        cancelWire();
    } else if (m_mode == SceneMode::PlaceComponent) {
        cancelPlacement();
    }

    m_mode = mode;
    emit modeChanged(mode);
}

GraphicComponent* SchematicScene::createComponent(ComponentType type, QPointF pos)
{
    if (!m_circuit) return nullptr;

    pos = snapToGrid(pos);

    std::unique_ptr<Component> comp;
    GraphicComponent* graphic = nullptr;

    switch (type) {
        case ComponentType::Resistor: {
            auto r = std::make_unique<Resistor>();
            graphic = new GResistor(r.get());
            comp = std::move(r);
            break;
        }
        case ComponentType::Capacitor: {
            auto c = std::make_unique<Capacitor>();
            graphic = new GCapacitor(c.get());
            comp = std::move(c);
            break;
        }
        case ComponentType::DCSource: {
            auto v = std::make_unique<DCSource>();
            graphic = new GDCSource(v.get());
            comp = std::move(v);
            break;
        }
        case ComponentType::ACSource: {
            auto v = std::make_unique<ACSource>();
            graphic = new GACSource(v.get());
            comp = std::move(v);
            break;
        }
        case ComponentType::Lamp: {
            auto l = std::make_unique<Lamp>();
            graphic = new GLamp(l.get());
            comp = std::move(l);
            break;
        }
        case ComponentType::Junction: {
            auto j = std::make_unique<Junction>();
            graphic = new GJunction(j.get());
            comp = std::move(j);
            break;
        }
        case ComponentType::Switch2Way: {
            auto s = std::make_unique<Switch>();
            graphic = new GSwitch(s.get());
            comp = std::move(s);
            break;
        }
        case ComponentType::Switch3Way: {
            auto s = std::make_unique<Switch3Way>();
            graphic = new GSwitch3Way(s.get());
            comp = std::move(s);
            break;
        }
        case ComponentType::Switch4Way: {
            auto s = std::make_unique<Switch4Way>();
            graphic = new GSwitch4Way(s.get());
            comp = std::move(s);
            break;
        }
        case ComponentType::Inductor: {
            auto l = std::make_unique<Inductor>();
            graphic = new GInductor(l.get());
            comp = std::move(l);
            break;
        }
        case ComponentType::Diode: {
            auto d = std::make_unique<Diode>();
            graphic = new GDiode(d.get());
            comp = std::move(d);
            break;
        }
        case ComponentType::ZenerDiode: {
            auto zd = std::make_unique<ZenerDiode>();
            graphic = new GZenerDiode(zd.get());
            comp = std::move(zd);
            break;
        }
        case ComponentType::Ground: {
            auto g = std::make_unique<Ground>();
            graphic = new GGround(g.get());
            comp = std::move(g);
            break;
        }
        case ComponentType::DCCurrentSource: {
            auto i = std::make_unique<DCCurrentSource>();
            graphic = new GDCCurrentSource(i.get());
            comp = std::move(i);
            break;
        }
        case ComponentType::PulseSource: {
            auto p = std::make_unique<PulseSource>();
            graphic = new GPulseSource(p.get());
            comp = std::move(p);
            break;
        }
        case ComponentType::NMosfet: {
            auto m = std::make_unique<NMosfet>();
            graphic = new GNMosfet(m.get());
            comp = std::move(m);
            break;
        }
        case ComponentType::PMosfet: {
            auto m = std::make_unique<PMosfet>();
            graphic = new GPMosfet(m.get());
            comp = std::move(m);
            break;
        }
        case ComponentType::NOTGate: {
            auto g = std::make_unique<NOTGate>();
            graphic = new GNOTGate(g.get());
            comp = std::move(g);
            break;
        }
        case ComponentType::ANDGate: {
            auto g = std::make_unique<ANDGate>();
            graphic = new GANDGate(g.get());
            comp = std::move(g);
            break;
        }
        case ComponentType::ORGate: {
            auto g = std::make_unique<ORGate>();
            graphic = new GORGate(g.get());
            comp = std::move(g);
            break;
        }
        case ComponentType::XORGate: {
            auto g = std::make_unique<XORGate>();
            graphic = new GXORGate(g.get());
            comp = std::move(g);
            break;
        }
    }

    if (!graphic || !comp) return nullptr;

    comp->setPosition(pos);
    int id = m_circuit->addComponent(std::move(comp));

    graphic->setPos(pos);
    addItem(graphic);
    m_graphicComponents[id] = graphic;

    // Forward double-click signal
    connect(graphic, &GraphicComponent::componentDoubleClicked,
            this, &SchematicScene::componentDoubleClicked);
    // Recalculate simulation when component moves (queued to avoid scene mods during itemChange)
    connect(graphic, &GraphicComponent::componentMoved,
            this, [this]() { emit simulationRecalcNeeded(); }, Qt::QueuedConnection);
    // Forward undo-related signals
    connect(graphic, &GraphicComponent::dragStarted,
            this, &SchematicScene::aboutToModifyCircuit);
    connect(graphic, &GraphicComponent::aboutToRotate,
            this, &SchematicScene::aboutToModifyCircuit);
    // Auto-connect overlapping pins after drop or rotation
    connect(graphic, &GraphicComponent::dropCompleted,
            this, &SchematicScene::autoConnectPins, Qt::QueuedConnection);

    emit circuitStructureChanged();
    return graphic;
}

GraphicComponent* SchematicScene::graphicForComponent(int componentId) const
{
    auto it = m_graphicComponents.find(componentId);
    return (it != m_graphicComponents.end()) ? it->second : nullptr;
}

void SchematicScene::deleteSelectedItems()
{
    if (!m_circuit) return;

    emit aboutToModifyCircuit();

    // Collect items to delete (can't modify while iterating)
    std::vector<GraphicWire*> wiresToDelete;
    std::vector<GraphicComponent*> compsToDelete;

    for (auto* item : selectedItems()) {
        if (auto* gw = dynamic_cast<GraphicWire*>(item)) {
            wiresToDelete.push_back(gw);
        } else if (auto* gc = dynamic_cast<GraphicComponent*>(item)) {
            compsToDelete.push_back(gc);
        }
    }

    // Delete wires first
    for (auto* gw : wiresToDelete) {
        // Remove from pins
        gw->startPin()->removeWire(gw);
        gw->endPin()->removeWire(gw);
        // Remove from core model
        m_circuit->removeWire(gw->coreWire()->id());
        // Remove from our tracking
        m_graphicWires.erase(
            std::remove(m_graphicWires.begin(), m_graphicWires.end(), gw),
            m_graphicWires.end());
        // Remove from scene
        removeItem(gw);
        delete gw;
    }

    // Delete components (also removes their connected wires)
    for (auto* gc : compsToDelete) {
        int compId = gc->coreComponent()->id();

        // Clear lamp visual for deleted component
        if (auto* gl = dynamic_cast<GLamp*>(gc)) {
            gl->clearCurrent();
        }

        // Remove measurement label for this component
        auto labelIt = m_measurementLabels.find(compId);
        if (labelIt != m_measurementLabels.end()) {
            removeItem(labelIt->second);
            delete labelIt->second;
            m_measurementLabels.erase(labelIt);
        }
        m_simResult.branchResults.erase(compId);

        // Find and remove all wires connected to this component
        std::vector<GraphicWire*> connectedWires;
        for (auto* pin : gc->pins()) {
            for (auto* wire : pin->connectedWires()) {
                connectedWires.push_back(wire);
            }
        }
        for (auto* gw : connectedWires) {
            gw->startPin()->removeWire(gw);
            gw->endPin()->removeWire(gw);
            m_graphicWires.erase(
                std::remove(m_graphicWires.begin(), m_graphicWires.end(), gw),
                m_graphicWires.end());
            removeItem(gw);
            delete gw;
        }

        // Remove from core model (also removes its wires internally)
        m_circuit->removeComponent(compId);
        // Remove from tracking
        m_graphicComponents.erase(compId);
        // Remove from scene
        removeItem(gc);
        delete gc;
    }

    updateWireColors();
    emit simulationRecalcNeeded();
    if (!compsToDelete.empty())
        emit circuitStructureChanged();
}

void SchematicScene::cancelOperation()
{
    if (m_mode == SceneMode::DrawWire) {
        cancelWire();
        setMode(SceneMode::Select);
    } else if (m_mode == SceneMode::PlaceComponent) {
        cancelPlacement();
        setMode(SceneMode::Select);
    }
}

// --- Background drawing ---

void SchematicScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, QColor(255, 255, 255));

    QPen dotPen(QColor(200, 200, 200), 1.5);
    painter->setPen(dotPen);

    qreal left   = std::floor(rect.left()   / GRID_SIZE) * GRID_SIZE;
    qreal top    = std::floor(rect.top()    / GRID_SIZE) * GRID_SIZE;
    qreal right  = rect.right();
    qreal bottom = rect.bottom();

    for (qreal x = left; x <= right; x += GRID_SIZE) {
        for (qreal y = top; y <= bottom; y += GRID_SIZE) {
            painter->drawPoint(QPointF(x, y));
        }
    }

    QPen majorDotPen(QColor(160, 160, 160), 2.5);
    painter->setPen(majorDotPen);

    qreal majorGrid = GRID_SIZE * 5.0;
    qreal majorLeft = std::floor(rect.left()  / majorGrid) * majorGrid;
    qreal majorTop  = std::floor(rect.top()  / majorGrid) * majorGrid;

    for (qreal x = majorLeft; x <= right; x += majorGrid) {
        for (qreal y = majorTop; y <= bottom; y += majorGrid) {
            painter->drawPoint(QPointF(x, y));
        }
    }

    QPen originPen(QColor(180, 180, 220), 1.0, Qt::DashLine);
    painter->setPen(originPen);
    if (rect.left() <= 0 && rect.right() >= 0)
        painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.bottom()));
    if (rect.top() <= 0 && rect.bottom() >= 0)
        painter->drawLine(QPointF(rect.left(), 0), QPointF(rect.right(), 0));
}

// --- Drag-and-drop ---

void SchematicScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-elesim-component")) {
        event->acceptProposedAction();
    } else {
        QGraphicsScene::dragEnterEvent(event);
    }
}

void SchematicScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-elesim-component")) {
        // Highlight wire under cursor for potential split
        QPointF pos = snapToGrid(event->scenePos());
        GraphicWire* candidate = wireAt(pos);
        if (candidate != m_splitTargetWire) {
            clearSplitHighlight();
            if (candidate) {
                candidate->setHighlighted(true);
                m_splitTargetWire = candidate;
            }
        }
        event->acceptProposedAction();
    } else {
        QGraphicsScene::dragMoveEvent(event);
    }
}

void SchematicScene::dropEvent(QGraphicsSceneDragDropEvent* event)
{
    if (!event->mimeData()->hasFormat("application/x-elesim-component")) {
        QGraphicsScene::dropEvent(event);
        return;
    }

    QByteArray data = event->mimeData()->data("application/x-elesim-component");
    auto type = static_cast<ComponentType>(data.toInt());

    GraphicWire* targetWire = m_splitTargetWire;
    clearSplitHighlight();

    emit aboutToModifyCircuit();
    auto* comp = createComponent(type, event->scenePos());
    if (comp && targetWire) {
        insertComponentOntoWire(comp, targetWire);
    }
    event->acceptProposedAction();
}

// --- Mouse events for wire drawing ---

void SchematicScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    // PlaceComponent: right-click rotates ghost (Shift = 45°, else 90°)
    if (m_mode == SceneMode::PlaceComponent && event->button() == Qt::RightButton) {
        double angle = (event->modifiers() & Qt::ShiftModifier) ? 45.0 : 90.0;
        m_ghostRotation = std::fmod(m_ghostRotation + angle, 360.0);
        if (m_ghostComponent) {
            m_ghostComponent->setRotation(m_ghostRotation);
        }
        event->accept();
        return;
    }

    // PlaceComponent: left-click places component (or selects existing one)
    if (m_mode == SceneMode::PlaceComponent && event->button() == Qt::LeftButton) {
        // If clicking on an existing component, exit place mode and select it
        for (auto* item : items(event->scenePos(), Qt::IntersectsItemShape, Qt::DescendingOrder)) {
            auto* gc = dynamic_cast<GraphicComponent*>(item);
            if (gc && gc != m_ghostComponent) {
                cancelPlacement();
                setMode(SceneMode::Select);
                clearSelection();
                gc->setSelected(true);
                event->accept();
                return;
            }
        }

        emit aboutToModifyCircuit();
        QPointF pos = snapToGrid(event->scenePos());
        auto* placed = createComponent(m_placeType, pos);
        if (placed) {
            placed->setRotation(m_ghostRotation);
            placed->coreComponent()->setRotation(m_ghostRotation);

            // If hovering over a wire, insert component onto it
            if (m_splitTargetWire) {
                GraphicWire* targetWire = m_splitTargetWire;
                clearSplitHighlight();
                insertComponentOntoWire(placed, targetWire);
            }

            // Auto-connect any overlapping pins
            autoConnectPins(placed);
        }
        // Stay in PlaceComponent mode - recreate ghost for next placement
        destroyGhostComponent();
        createGhostComponent();
        if (m_ghostComponent) {
            m_ghostComponent->setPos(pos);
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    if (m_mode == SceneMode::DrawWire) {
        // Use the highlighted pin if available, otherwise try exact hit
        GraphicPin* targetPin = m_highlightedPin;
        if (!targetPin)
            targetPin = pinAt(event->scenePos());

        if (targetPin && targetPin != m_wireStartPin) {
            if (targetPin->parentComponent() != m_wireStartPin->parentComponent()) {
                clearWireHighlight();
                clearSplitHighlight();
                finishWire(targetPin);
            }
        } else if (!targetPin) {
            // Check if clicking near a wire for T-junction
            GraphicWire* targetWire = wireAt(event->scenePos());
            if (targetWire) {
                clearWireHighlight();
                clearSplitHighlight();
                emit aboutToModifyCircuit();
                createTJunction(m_wireStartPin, event->scenePos(), targetWire);
                cancelWire();
                setMode(SceneMode::Select);
            } else {
                // Pivot mode: click empty space to set a waypoint
                QPointF lastAnchor = m_wirePivots.isEmpty()
                    ? m_wireStartPin->scenePos()
                    : m_wirePivots.last();

                QPointF pivot;
                if (event->modifiers() & Qt::ShiftModifier) {
                    pivot = constrainTo45(lastAnchor, event->scenePos());
                } else {
                    pivot = snapToGrid(event->scenePos());
                }

                QPen commitPen(QColor(0, 150, 0), 2, Qt::SolidLine);
                auto* seg = addLine(QLineF(lastAnchor, pivot), commitPen);
                seg->setZValue(100);
                m_tempWireSegments.push_back(seg);

                m_wirePivots.append(pivot);

                // Move rubber-band start to the new pivot
                if (m_tempWireLine) {
                    m_tempWireLine->setLine(QLineF(pivot, pivot));
                }
            }
        }
        event->accept();
        return;
    }

    // In Select mode: clicking directly on a pin starts wire drawing
    // (only exact hits — proximity snap is reserved for DrawWire mode
    //  so that rubber-band box selection works on empty space)
    GraphicPin* clickedPin = pinAt(event->scenePos());

    if (m_mode == SceneMode::Select && clickedPin) {
        startWire(clickedPin);
        event->accept();
        return;
    }

    QGraphicsScene::mousePressEvent(event);
}

void SchematicScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    // PlaceComponent: move ghost to cursor
    if (m_mode == SceneMode::PlaceComponent && m_ghostComponent) {
        QPointF snapped = snapToGrid(event->scenePos());
        m_ghostComponent->setPos(snapped);
        if (!m_ghostComponent->isVisible())
            m_ghostComponent->setVisible(true);

        // Highlight wire under cursor for potential split
        GraphicWire* candidate = wireAt(snapped);
        if (candidate != m_splitTargetWire) {
            clearSplitHighlight();
            if (candidate) {
                candidate->setHighlighted(true);
                m_splitTargetWire = candidate;
            }
        }
        event->accept();
        return;
    }

    if (m_mode == SceneMode::DrawWire && m_tempWireLine && m_wireStartPin) {
        // Rubber-band starts from last pivot, or from start pin if no pivots
        QPointF anchor = m_wirePivots.isEmpty()
            ? m_wireStartPin->scenePos()
            : m_wirePivots.last();
        QPointF mousePos = event->scenePos();

        // Check for nearby connectable pin and highlight it
        updateWireHighlight(mousePos);

        // Snap rubber-band endpoint to highlighted pin, or to grid
        QPointF end;
        if (m_highlightedPin) {
            end = m_highlightedPin->scenePos();
            clearSplitHighlight(); // pin takes priority over wire
        } else if (event->modifiers() & Qt::ShiftModifier) {
            end = constrainTo45(anchor, mousePos);
        } else {
            end = snapToGrid(mousePos);
            // Check for nearby wire body for T-junction hint
            GraphicWire* candidate = wireAt(mousePos);
            if (candidate != m_splitTargetWire) {
                clearSplitHighlight();
                if (candidate) {
                    candidate->setHighlighted(true);
                    m_splitTargetWire = candidate;
                }
            }
        }
        m_tempWireLine->setLine(QLineF(anchor, end));
        event->accept();
        return;
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void SchematicScene::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        cancelOperation();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelectedItems();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_R) {
        double angle = (event->modifiers() & Qt::ShiftModifier) ? 45.0 : 90.0;
        // Rotate selected components
        bool hasComponents = false;
        for (auto* item : selectedItems()) {
            if (dynamic_cast<GraphicComponent*>(item)) { hasComponents = true; break; }
        }
        if (hasComponents)
            emit aboutToModifyCircuit();
        for (auto* item : selectedItems()) {
            if (auto* gc = dynamic_cast<GraphicComponent*>(item)) {
                gc->rotateBy(angle);
            }
        }
        event->accept();
        return;
    }

    QGraphicsScene::keyPressEvent(event);
}

// --- Wire drawing helpers ---

GraphicPin* SchematicScene::pinAt(const QPointF& scenePos) const
{
    QList<QGraphicsItem*> itemsAtPos = items(scenePos, Qt::IntersectsItemShape,
                                              Qt::DescendingOrder);
    for (auto* item : itemsAtPos) {
        if (auto* pin = dynamic_cast<GraphicPin*>(item)) {
            return pin;
        }
    }
    return nullptr;
}

GraphicPin* SchematicScene::nearestConnectablePin(const QPointF& scenePos) const
{
    GraphicPin* nearest = nullptr;
    qreal minDist = PIN_SNAP_RADIUS;

    for (auto& [id, gc] : m_graphicComponents) {
        for (auto* pin : gc->pins()) {
            // Skip the start pin and pins on the same component
            if (pin == m_wireStartPin) continue;
            if (m_wireStartPin && pin->parentComponent() == m_wireStartPin->parentComponent())
                continue;

            QPointF pinPos = pin->scenePos();
            qreal dx = pinPos.x() - scenePos.x();
            qreal dy = pinPos.y() - scenePos.y();
            qreal dist = std::sqrt(dx * dx + dy * dy);

            if (dist < minDist) {
                minDist = dist;
                nearest = pin;
            }
        }
    }
    return nearest;
}

void SchematicScene::updateWireHighlight(const QPointF& mousePos)
{
    GraphicPin* candidate = nearestConnectablePin(mousePos);

    if (candidate == m_highlightedPin) return; // no change

    // Unhighlight previous
    if (m_highlightedPin) {
        m_highlightedPin->setHighlighted(false);
        m_highlightedPin = nullptr;
    }

    // Highlight new
    if (candidate) {
        candidate->setHighlighted(true);
        m_highlightedPin = candidate;
    }
}

void SchematicScene::clearWireHighlight()
{
    if (m_highlightedPin) {
        m_highlightedPin->setHighlighted(false);
        m_highlightedPin = nullptr;
    }
}

void SchematicScene::startWire(GraphicPin* pin)
{
    m_wireStartPin = pin;
    m_mode = SceneMode::DrawWire;
    emit modeChanged(m_mode);

    // Create rubber-band line
    QPen rubberPen(QColor(0, 150, 0, 180), 2, Qt::DashLine);
    m_tempWireLine = addLine(QLineF(pin->scenePos(), pin->scenePos()), rubberPen);
    m_tempWireLine->setZValue(100);
}

void SchematicScene::finishWire(GraphicPin* endPin)
{
    if (!m_wireStartPin || !endPin || !m_circuit) return;

    // Get component IDs and pin indices
    int comp1 = m_wireStartPin->parentComponent()->coreComponent()->id();
    int pin1  = m_wireStartPin->pinIndex();
    int comp2 = endPin->parentComponent()->coreComponent()->id();
    int pin2  = endPin->pinIndex();

    // Check if this wire already exists
    for (auto& wire : m_circuit->wires()) {
        bool match = (wire->from().componentId == comp1 && wire->from().pinIndex == pin1 &&
                      wire->to().componentId == comp2 && wire->to().pinIndex == pin2) ||
                     (wire->from().componentId == comp2 && wire->from().pinIndex == pin2 &&
                      wire->to().componentId == comp1 && wire->to().pinIndex == pin1);
        if (match) {
            // Wire already exists, just cancel
            cancelWire();
            setMode(SceneMode::Select);
            return;
        }
    }

    emit aboutToModifyCircuit();

    // Create wire in core model
    int wireId = m_circuit->addWire(comp1, pin1, comp2, pin2);
    (void)wireId;

    // Find the wire we just created (it's the last one)
    Wire* coreWire = m_circuit->wires().back().get();

    // Create graphic wire
    auto* gWire = new GraphicWire(m_wireStartPin, endPin, coreWire);

    // Apply accumulated pivots as waypoints
    if (!m_wirePivots.isEmpty()) {
        gWire->setWaypoints(m_wirePivots);
        coreWire->setWaypoints(m_wirePivots);
    }

    addItem(gWire);
    m_graphicWires.push_back(gWire);

    // Register with pins
    m_wireStartPin->addWire(gWire);
    endPin->addWire(gWire);

    // Clean up
    cancelWire();
    setMode(SceneMode::Select);

    // Select the new wire so user can right-click to cycle routes
    clearSelection();
    gWire->setSelected(true);

    updateWireColors();
}

void SchematicScene::autoConnectPins(GraphicComponent* comp)
{
    if (!comp || !m_circuit) return;

    static constexpr qreal OVERLAP_THRESHOLD = 2.0;

    for (auto* myPin : comp->pins()) {
        if (myPin->isConnected()) continue; // already has a wire

        QPointF myPos = myPin->scenePos();

        for (auto& [otherId, otherGC] : m_graphicComponents) {
            if (otherGC == comp) continue;

            for (auto* otherPin : otherGC->pins()) {
                QPointF otherPos = otherPin->scenePos();
                double dx = myPos.x() - otherPos.x();
                double dy = myPos.y() - otherPos.y();
                if (std::abs(dx) > OVERLAP_THRESHOLD || std::abs(dy) > OVERLAP_THRESHOLD)
                    continue;

                // Pins overlap — check if wire already exists
                int c1 = comp->coreComponent()->id();
                int p1 = myPin->pinIndex();
                int c2 = otherGC->coreComponent()->id();
                int p2 = otherPin->pinIndex();

                bool exists = false;
                for (auto& wire : m_circuit->wires()) {
                    bool match = (wire->from().componentId == c1 && wire->from().pinIndex == p1 &&
                                  wire->to().componentId == c2 && wire->to().pinIndex == p2) ||
                                 (wire->from().componentId == c2 && wire->from().pinIndex == p2 &&
                                  wire->to().componentId == c1 && wire->to().pinIndex == p1);
                    if (match) { exists = true; break; }
                }
                if (exists) continue;

                // Create wire
                m_circuit->addWire(c1, p1, c2, p2);
                Wire* coreWire = m_circuit->wires().back().get();
                auto* gWire = new GraphicWire(myPin, otherPin, coreWire);
                addItem(gWire);
                m_graphicWires.push_back(gWire);
                myPin->addWire(gWire);
                otherPin->addWire(gWire);
            }
        }
    }

    updateWireColors();
}

void SchematicScene::cancelWire()
{
    clearWireHighlight();
    if (m_tempWireLine) {
        removeItem(m_tempWireLine);
        delete m_tempWireLine;
        m_tempWireLine = nullptr;
    }
    // Clean up committed pivot segment visuals
    for (auto* seg : m_tempWireSegments) {
        removeItem(seg);
        delete seg;
    }
    m_tempWireSegments.clear();
    m_wirePivots.clear();
    m_wireStartPin = nullptr;
}

// --- Selection ---

void SchematicScene::onSelectionChanged()
{
    auto items = selectedItems();
    qDebug() << "[Scene::selectionChanged] count=" << items.size();
    for (auto* item : items) {
        qDebug() << "  selected:" << item << "type=" << item->type();
    }
    if (items.size() == 1) {
        if (auto* gc = dynamic_cast<GraphicComponent*>(items.first())) {
            qDebug() << "  -> componentSelected";
            emit componentSelected(gc->coreComponent());
            return;
        }
    }
    qDebug() << "  -> selectionCleared";
    emit selectionCleared();
}

QPointF SchematicScene::snapToGrid(const QPointF& pos)
{
    return QPointF(
        std::round(pos.x() / GRID_SIZE) * GRID_SIZE,
        std::round(pos.y() / GRID_SIZE) * GRID_SIZE
    );
}

QPointF SchematicScene::constrainTo45(const QPointF& anchor, const QPointF& target)
{
    qreal dx = target.x() - anchor.x();
    qreal dy = target.y() - anchor.y();
    qreal adx = std::abs(dx);
    qreal ady = std::abs(dy);

    // tan(22.5°) ≈ 0.4142 — threshold between cardinal and diagonal
    if (ady < adx * 0.4142) {
        // Nearly horizontal — constrain to horizontal, snap X to grid
        qreal snappedX = std::round(target.x() / GRID_SIZE) * GRID_SIZE;
        return QPointF(snappedX, anchor.y());
    } else if (adx < ady * 0.4142) {
        // Nearly vertical — constrain to vertical, snap Y to grid
        qreal snappedY = std::round(target.y() / GRID_SIZE) * GRID_SIZE;
        return QPointF(anchor.x(), snappedY);
    } else {
        // Diagonal (45°) — equal |dx| and |dy|, snap to grid units
        qreal dist = std::max(adx, ady);
        qreal snappedDist = std::round(dist / GRID_SIZE) * GRID_SIZE;
        qreal sx = (dx >= 0) ? 1.0 : -1.0;
        qreal sy = (dy >= 0) ? 1.0 : -1.0;
        return QPointF(anchor.x() + sx * snappedDist, anchor.y() + sy * snappedDist);
    }
}

// --- Simulation results ---

void SchematicScene::setSimulationResult(const SimulationResult& result)
{
    clearSimulationResult();
    m_simResult = result;

    if (!result.success) return;

    // Add measurement labels on components and update lamp visuals
    for (auto& [compId, br] : result.branchResults) {
        auto* gc = graphicForComponent(compId);
        if (!gc) continue;

        // Update lamp glow based on current
        if (auto* gl = dynamic_cast<GLamp*>(gc)) {
            gl->setCurrent(br.current);
        }

        QString text;
        if (br.isMixed) {
            text = QString("DC: %1 / %2\nAC: %3 / %4")
                .arg(formatEngineering(br.dcVoltage, "V"))
                .arg(formatEngineering(br.dcCurrent, "A"))
                .arg(formatEngineering(br.acVoltage, "V"))
                .arg(formatEngineering(br.acCurrent, "A"));
        } else {
            text = QString("V=%1\nI=%2\nP=%3")
                .arg(formatEngineering(br.voltageDrop, "V"))
                .arg(formatEngineering(br.current, "A"))
                .arg(formatEngineering(br.power, "W"));
        }

        auto* label = new QGraphicsTextItem(text);
        label->setDefaultTextColor(QColor(0, 0, 180));
        QFont font;
        font.setPixelSize(9);
        label->setFont(font);
        label->setPos(gc->pos().x() + 35, gc->pos().y() - 20);
        label->setZValue(50);
        addItem(label);
        m_measurementLabels[compId] = label;
    }
}

void SchematicScene::clearSimulationResult()
{
    for (auto& [id, item] : m_measurementLabels) {
        removeItem(item);
        delete item;
    }
    m_measurementLabels.clear();
    m_simResult = SimulationResult();

    // Reset lamp visuals
    for (auto& [id, gc] : m_graphicComponents) {
        if (auto* gl = dynamic_cast<GLamp*>(gc)) {
            gl->clearCurrent();
        }
    }
}

void SchematicScene::updateMeasurementLabels(const TransientFrame& frame)
{
    for (auto& [compId, item] : m_measurementLabels) {
        auto* textItem = dynamic_cast<QGraphicsTextItem*>(item);
        if (!textItem) continue;

        double voltage = 0.0, current = 0.0;
        auto vIt = frame.componentVoltages.find(compId);
        if (vIt != frame.componentVoltages.end()) voltage = vIt->second;
        auto iIt = frame.branchCurrents.find(compId);
        if (iIt != frame.branchCurrents.end()) current = iIt->second;
        double power = voltage * current;

        QString text = QString("V=%1\nI=%2\nP=%3")
            .arg(formatEngineering(voltage, "V"))
            .arg(formatEngineering(current, "A"))
            .arg(formatEngineering(power, "W"));
        textItem->setPlainText(text);

        // Update lamp glow
        auto gcIt = m_graphicComponents.find(compId);
        if (gcIt != m_graphicComponents.end()) {
            if (auto* gl = dynamic_cast<GLamp*>(gcIt->second))
                gl->setCurrent(current);
        }
    }
}

void SchematicScene::restoreMeasurementLabels()
{
    for (auto& [compId, item] : m_measurementLabels) {
        auto* textItem = dynamic_cast<QGraphicsTextItem*>(item);
        if (!textItem) continue;

        auto brIt = m_simResult.branchResults.find(compId);
        if (brIt == m_simResult.branchResults.end()) continue;
        const auto& br = brIt->second;

        QString text;
        if (br.isMixed) {
            text = QString("DC: %1 / %2\nAC: %3 / %4")
                .arg(formatEngineering(br.dcVoltage, "V"))
                .arg(formatEngineering(br.dcCurrent, "A"))
                .arg(formatEngineering(br.acVoltage, "V"))
                .arg(formatEngineering(br.acCurrent, "A"));
        } else {
            text = QString("V=%1\nI=%2\nP=%3")
                .arg(formatEngineering(br.voltageDrop, "V"))
                .arg(formatEngineering(br.current, "A"))
                .arg(formatEngineering(br.power, "W"));
        }
        textItem->setPlainText(text);

        // Restore lamp glow
        auto gcIt = m_graphicComponents.find(compId);
        if (gcIt != m_graphicComponents.end()) {
            if (auto* gl = dynamic_cast<GLamp*>(gcIt->second))
                gl->setCurrent(br.current);
        }
    }
}

void SchematicScene::highlightProbePin(int componentId, int pinIndex, const QColor& color)
{
    auto it = m_graphicComponents.find(componentId);
    if (it == m_graphicComponents.end()) return;

    GraphicPin* gp = it->second->graphicPin(pinIndex);
    if (!gp) return;

    gp->setProbeHighlight(color);
    m_highlightedProbePins.push_back(gp);
}

void SchematicScene::clearProbeHighlights()
{
    for (GraphicPin* gp : m_highlightedProbePins) {
        gp->clearProbeHighlight();
    }
    m_highlightedProbePins.clear();
}

void SchematicScene::rebuildFromCircuit()
{
    // Clear existing scene items
    clearSimulationResult();
    cancelWire();
    m_graphicComponents.clear();
    m_graphicWires.clear();
    clear(); // QGraphicsScene::clear() removes all items

    if (!m_circuit) return;

    // Recreate graphic components
    for (auto& [id, comp] : m_circuit->components()) {
        GraphicComponent* graphic = nullptr;

        switch (comp->type()) {
            case ComponentType::Resistor:
                graphic = new GResistor(static_cast<Resistor*>(comp.get()));
                break;
            case ComponentType::Capacitor:
                graphic = new GCapacitor(static_cast<Capacitor*>(comp.get()));
                break;
            case ComponentType::DCSource:
                graphic = new GDCSource(static_cast<DCSource*>(comp.get()));
                break;
            case ComponentType::ACSource:
                graphic = new GACSource(static_cast<ACSource*>(comp.get()));
                break;
            case ComponentType::Lamp:
                graphic = new GLamp(static_cast<Lamp*>(comp.get()));
                break;
            case ComponentType::Junction:
                graphic = new GJunction(static_cast<Junction*>(comp.get()));
                break;
            case ComponentType::Switch2Way:
                graphic = new GSwitch(static_cast<Switch*>(comp.get()));
                break;
            case ComponentType::Switch3Way:
                graphic = new GSwitch3Way(static_cast<Switch3Way*>(comp.get()));
                break;
            case ComponentType::Switch4Way:
                graphic = new GSwitch4Way(static_cast<Switch4Way*>(comp.get()));
                break;
            case ComponentType::Inductor:
                graphic = new GInductor(static_cast<Inductor*>(comp.get()));
                break;
            case ComponentType::Diode:
                graphic = new GDiode(static_cast<Diode*>(comp.get()));
                break;
            case ComponentType::ZenerDiode:
                graphic = new GZenerDiode(static_cast<ZenerDiode*>(comp.get()));
                break;
            case ComponentType::Ground:
                graphic = new GGround(static_cast<Ground*>(comp.get()));
                break;
            case ComponentType::DCCurrentSource:
                graphic = new GDCCurrentSource(static_cast<DCCurrentSource*>(comp.get()));
                break;
            case ComponentType::PulseSource:
                graphic = new GPulseSource(static_cast<PulseSource*>(comp.get()));
                break;
        }

        if (!graphic) continue;

        graphic->setPos(comp->position());
        graphic->setRotation(comp->rotation());
        addItem(graphic);
        m_graphicComponents[id] = graphic;

        connect(graphic, &GraphicComponent::componentDoubleClicked,
                this, &SchematicScene::componentDoubleClicked);
        connect(graphic, &GraphicComponent::componentMoved,
                this, [this]() { emit simulationRecalcNeeded(); }, Qt::QueuedConnection);
        connect(graphic, &GraphicComponent::dragStarted,
                this, &SchematicScene::aboutToModifyCircuit);
        connect(graphic, &GraphicComponent::aboutToRotate,
                this, &SchematicScene::aboutToModifyCircuit);
        connect(graphic, &GraphicComponent::dropCompleted,
                this, &SchematicScene::autoConnectPins, Qt::QueuedConnection);
    }

    // Recreate graphic wires
    for (auto& wire : m_circuit->wires()) {
        int comp1Id = wire->from().componentId;
        int pin1Idx = wire->from().pinIndex;
        int comp2Id = wire->to().componentId;
        int pin2Idx = wire->to().pinIndex;

        auto* gc1 = graphicForComponent(comp1Id);
        auto* gc2 = graphicForComponent(comp2Id);
        if (!gc1 || !gc2) continue;

        auto* gPin1 = gc1->graphicPin(pin1Idx);
        auto* gPin2 = gc2->graphicPin(pin2Idx);
        if (!gPin1 || !gPin2) continue;

        auto* gWire = new GraphicWire(gPin1, gPin2, wire.get());
        // Restore saved waypoints if present
        if (!wire->waypoints().isEmpty()) {
            gWire->setWaypoints(wire->waypoints());
        }
        addItem(gWire);
        m_graphicWires.push_back(gWire);

        gPin1->addWire(gWire);
        gPin2->addWire(gWire);
    }

    // Reconnect selection changed signal (clear() disconnects it)
    connect(this, &QGraphicsScene::selectionChanged,
            this, &SchematicScene::onSelectionChanged);

    updateWireColors();
}

// --- Click-to-place ---

void SchematicScene::enterPlaceMode(ComponentType type)
{
    // Toggle off if clicking the same component type again
    if (m_mode == SceneMode::PlaceComponent && m_placeType == type) {
        setMode(SceneMode::Select);
        return;
    }
    setMode(SceneMode::PlaceComponent);
    m_placeType = type;
    m_ghostRotation = 0.0;
    createGhostComponent();

    // Ensure the view has keyboard focus so ESC works immediately
    if (!views().isEmpty())
        views().first()->setFocus();
}

void SchematicScene::createGhostComponent()
{
    destroyGhostComponent();

    // Create a temporary core component (not added to circuit)
    switch (m_placeType) {
        case ComponentType::Resistor:
            m_ghostCoreComponent = std::make_unique<Resistor>();
            m_ghostComponent = new GResistor(static_cast<Resistor*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::Capacitor:
            m_ghostCoreComponent = std::make_unique<Capacitor>();
            m_ghostComponent = new GCapacitor(static_cast<Capacitor*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::DCSource:
            m_ghostCoreComponent = std::make_unique<DCSource>();
            m_ghostComponent = new GDCSource(static_cast<DCSource*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::ACSource:
            m_ghostCoreComponent = std::make_unique<ACSource>();
            m_ghostComponent = new GACSource(static_cast<ACSource*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::Lamp:
            m_ghostCoreComponent = std::make_unique<Lamp>();
            m_ghostComponent = new GLamp(static_cast<Lamp*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::Junction:
            return; // junctions are not placed manually
        case ComponentType::Switch2Way:
            m_ghostCoreComponent = std::make_unique<Switch>();
            m_ghostComponent = new GSwitch(static_cast<Switch*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::Switch3Way:
            m_ghostCoreComponent = std::make_unique<Switch3Way>();
            m_ghostComponent = new GSwitch3Way(static_cast<Switch3Way*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::Switch4Way:
            m_ghostCoreComponent = std::make_unique<Switch4Way>();
            m_ghostComponent = new GSwitch4Way(static_cast<Switch4Way*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::Inductor:
            m_ghostCoreComponent = std::make_unique<Inductor>();
            m_ghostComponent = new GInductor(static_cast<Inductor*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::Diode:
            m_ghostCoreComponent = std::make_unique<Diode>();
            m_ghostComponent = new GDiode(static_cast<Diode*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::ZenerDiode:
            m_ghostCoreComponent = std::make_unique<ZenerDiode>();
            m_ghostComponent = new GZenerDiode(static_cast<ZenerDiode*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::Ground:
            m_ghostCoreComponent = std::make_unique<Ground>();
            m_ghostComponent = new GGround(static_cast<Ground*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::DCCurrentSource:
            m_ghostCoreComponent = std::make_unique<DCCurrentSource>();
            m_ghostComponent = new GDCCurrentSource(static_cast<DCCurrentSource*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::PulseSource:
            m_ghostCoreComponent = std::make_unique<PulseSource>();
            m_ghostComponent = new GPulseSource(static_cast<PulseSource*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::NMosfet:
            m_ghostCoreComponent = std::make_unique<NMosfet>();
            m_ghostComponent = new GNMosfet(static_cast<NMosfet*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::PMosfet:
            m_ghostCoreComponent = std::make_unique<PMosfet>();
            m_ghostComponent = new GPMosfet(static_cast<PMosfet*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::NOTGate:
            m_ghostCoreComponent = std::make_unique<NOTGate>();
            m_ghostComponent = new GNOTGate(static_cast<NOTGate*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::ANDGate:
            m_ghostCoreComponent = std::make_unique<ANDGate>();
            m_ghostComponent = new GANDGate(static_cast<ANDGate*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::ORGate:
            m_ghostCoreComponent = std::make_unique<ORGate>();
            m_ghostComponent = new GORGate(static_cast<ORGate*>(m_ghostCoreComponent.get()));
            break;
        case ComponentType::XORGate:
            m_ghostCoreComponent = std::make_unique<XORGate>();
            m_ghostComponent = new GXORGate(static_cast<XORGate*>(m_ghostCoreComponent.get()));
            break;
    }

    if (!m_ghostComponent) return;

    m_ghostComponent->setGhostMode(true);
    m_ghostComponent->setOpacity(0.6);
    m_ghostComponent->setVisible(false);  // hidden until mouse moves over the scene
    m_ghostComponent->setFlag(QGraphicsItem::ItemIsMovable, false);
    m_ghostComponent->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_ghostComponent->setFlag(QGraphicsItem::ItemSendsGeometryChanges, false);
    m_ghostComponent->setAcceptHoverEvents(false);
    m_ghostComponent->setRotation(m_ghostRotation);
    m_ghostComponent->setZValue(100);
    addItem(m_ghostComponent);
}

void SchematicScene::destroyGhostComponent()
{
    if (m_ghostComponent) {
        removeItem(m_ghostComponent);
        delete m_ghostComponent;
        m_ghostComponent = nullptr;
    }
    m_ghostCoreComponent.reset();
}

void SchematicScene::cancelPlacement()
{
    destroyGhostComponent();
    clearSplitHighlight();
    m_ghostRotation = 0.0;
}

// --- Wire proximity detection ---

qreal SchematicScene::distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
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

GraphicWire* SchematicScene::wireAt(const QPointF& pos, qreal threshold) const
{
    GraphicWire* nearest = nullptr;
    qreal minDist = threshold;

    for (auto* gw : m_graphicWires) {
        QPointF start = gw->startPin()->scenePos();
        QPointF end = gw->endPin()->scenePos();
        QPointF mid(end.x(), start.y()); // manhattan midpoint

        // Distance to horizontal segment (start -> mid)
        qreal d1 = distanceToSegment(pos, start, mid);
        // Distance to vertical segment (mid -> end)
        qreal d2 = distanceToSegment(pos, mid, end);
        qreal d = std::min(d1, d2);

        if (d < minDist) {
            minDist = d;
            nearest = gw;
        }
    }
    return nearest;
}

void SchematicScene::clearSplitHighlight()
{
    if (m_splitTargetWire) {
        m_splitTargetWire->setHighlighted(false);
        m_splitTargetWire = nullptr;
    }
}

// --- Wire splitting ---

void SchematicScene::insertComponentOntoWire(GraphicComponent* comp, GraphicWire* wire)
{
    if (!comp || !wire || !m_circuit) return;
    if (comp->pins().size() < 2) return;

    // Save wire's endpoints
    GraphicPin* wireStartPin = wire->startPin();
    GraphicPin* wireEndPin = wire->endPin();

    int oldComp1 = wireStartPin->parentComponent()->coreComponent()->id();
    int oldPin1  = wireStartPin->pinIndex();
    int oldComp2 = wireEndPin->parentComponent()->coreComponent()->id();
    int oldPin2  = wireEndPin->pinIndex();

    int newCompId = comp->coreComponent()->id();

    // 1. Remove the old wire
    wireStartPin->removeWire(wire);
    wireEndPin->removeWire(wire);
    m_circuit->removeWire(wire->coreWire()->id());
    m_graphicWires.erase(
        std::remove(m_graphicWires.begin(), m_graphicWires.end(), wire),
        m_graphicWires.end());
    removeItem(wire);
    delete wire;

    // 2. Create wire: old start pin -> new component pin 0
    {
        m_circuit->addWire(oldComp1, oldPin1, newCompId, 0);
        Wire* cw = m_circuit->wires().back().get();
        GraphicPin* newPin0 = comp->graphicPin(0);
        auto* gw = new GraphicWire(wireStartPin, newPin0, cw);
        addItem(gw);
        m_graphicWires.push_back(gw);
        wireStartPin->addWire(gw);
        newPin0->addWire(gw);
    }

    // 3. Create wire: new component pin 1 -> old end pin
    {
        m_circuit->addWire(newCompId, 1, oldComp2, oldPin2);
        Wire* cw = m_circuit->wires().back().get();
        GraphicPin* newPin1 = comp->graphicPin(1);
        auto* gw = new GraphicWire(newPin1, wireEndPin, cw);
        addItem(gw);
        m_graphicWires.push_back(gw);
        newPin1->addWire(gw);
        wireEndPin->addWire(gw);
    }

    updateWireColors();
}

// --- T-junction ---

void SchematicScene::createTJunction(GraphicPin* fromPin, const QPointF& junctionPos,
                                      GraphicWire* targetWire)
{
    if (!fromPin || !targetWire || !m_circuit) return;

    QPointF snappedPos = snapToGrid(junctionPos);

    // Save target wire endpoints before deletion
    GraphicPin* wireStartPin = targetWire->startPin();
    GraphicPin* wireEndPin = targetWire->endPin();
    int oldComp1 = wireStartPin->parentComponent()->coreComponent()->id();
    int oldPin1  = wireStartPin->pinIndex();
    int oldComp2 = wireEndPin->parentComponent()->coreComponent()->id();
    int oldPin2  = wireEndPin->pinIndex();

    // 1. Create junction component
    auto* junctionComp = createComponent(ComponentType::Junction, snappedPos);
    if (!junctionComp) return;
    int junctionId = junctionComp->coreComponent()->id();

    // 2. Remove old wire
    wireStartPin->removeWire(targetWire);
    wireEndPin->removeWire(targetWire);
    m_circuit->removeWire(targetWire->coreWire()->id());
    m_graphicWires.erase(
        std::remove(m_graphicWires.begin(), m_graphicWires.end(), targetWire),
        m_graphicWires.end());
    removeItem(targetWire);
    delete targetWire;

    // 3. Create wire: old start -> junction pin 0
    {
        m_circuit->addWire(oldComp1, oldPin1, junctionId, 0);
        Wire* cw = m_circuit->wires().back().get();
        auto* gw = new GraphicWire(wireStartPin, junctionComp->graphicPin(0), cw);
        addItem(gw);
        m_graphicWires.push_back(gw);
        wireStartPin->addWire(gw);
        junctionComp->graphicPin(0)->addWire(gw);
    }

    // 4. Create wire: junction pin 1 -> old end
    {
        m_circuit->addWire(junctionId, 1, oldComp2, oldPin2);
        Wire* cw = m_circuit->wires().back().get();
        auto* gw = new GraphicWire(junctionComp->graphicPin(1), wireEndPin, cw);
        addItem(gw);
        m_graphicWires.push_back(gw);
        junctionComp->graphicPin(1)->addWire(gw);
        wireEndPin->addWire(gw);
    }

    // 5. Create wire: fromPin -> junction pin 2
    {
        int comp1 = fromPin->parentComponent()->coreComponent()->id();
        int pin1  = fromPin->pinIndex();
        m_circuit->addWire(comp1, pin1, junctionId, 2);
        Wire* cw = m_circuit->wires().back().get();
        auto* gw = new GraphicWire(fromPin, junctionComp->graphicPin(2), cw);
        addItem(gw);
        m_graphicWires.push_back(gw);
        fromPin->addWire(gw);
        junctionComp->graphicPin(2)->addWire(gw);
    }

    updateWireColors();
}

// --- Wire connectivity coloring ---

void SchematicScene::updateWireColors()
{
    if (!m_circuit || m_graphicWires.empty()) return;

    // Build adjacency: component pin -> list of connected component pins via wires
    // Key: (componentId, pinIndex), Value: set of (componentId, pinIndex) neighbors
    using PinKey = std::pair<int, int>;
    std::map<PinKey, std::vector<PinKey>> adj;

    for (auto* gw : m_graphicWires) {
        Wire* w = gw->coreWire();
        PinKey a = {w->from().componentId, w->from().pinIndex};
        PinKey b = {w->to().componentId, w->to().pinIndex};
        adj[a].push_back(b);
        adj[b].push_back(a);
    }

    // Connect pins of the same component based on internal connectivity
    // Switches only connect pins that are electrically linked by their state
    auto connectPins = [&](int id, int p1, int p2) {
        PinKey a = {id, p1};
        PinKey b = {id, p2};
        adj[a].push_back(b);
        adj[b].push_back(a);
    };

    for (auto& [id, comp] : m_circuit->components()) {
        switch (comp->type()) {
            case ComponentType::Switch2Way: {
                auto* sw = static_cast<Switch*>(comp.get());
                if (sw->isClosed()) connectPins(id, 0, 1);
                break;
            }
            case ComponentType::Switch3Way: {
                auto* sw = static_cast<Switch3Way*>(comp.get());
                // Common pin0 connects to active position pin
                connectPins(id, 0, sw->activePosition() == 0 ? 1 : 2);
                break;
            }
            case ComponentType::Switch4Way: {
                auto* sw = static_cast<Switch4Way*>(comp.get());
                if (sw->isCrossed()) {
                    connectPins(id, 0, 3);
                    connectPins(id, 1, 2);
                } else {
                    connectPins(id, 0, 2);
                    connectPins(id, 1, 3);
                }
                break;
            }
            default: {
                // All other components: connect all pins
                int numPins = comp->pinCount();
                for (int i = 0; i < numPins; ++i)
                    for (int j = i + 1; j < numPins; ++j)
                        connectPins(id, i, j);
                break;
            }
        }
    }

    // Find all pins connected to a power source (DCSource or ACSource)
    std::set<PinKey> powered;
    std::vector<PinKey> queue;

    for (auto& [id, comp] : m_circuit->components()) {
        if (comp->isSource()) {
            for (int i = 0; i < comp->pinCount(); ++i) {
                PinKey pk = {id, i};
                if (powered.insert(pk).second) {
                    queue.push_back(pk);
                }
            }
        }
    }

    // BFS from all power source pins
    size_t head = 0;
    while (head < queue.size()) {
        PinKey cur = queue[head++];
        auto it = adj.find(cur);
        if (it == adj.end()) continue;
        for (const auto& neighbor : it->second) {
            if (powered.insert(neighbor).second) {
                queue.push_back(neighbor);
            }
        }
    }

    // Color each wire based on whether both endpoints are in the powered set
    for (auto* gw : m_graphicWires) {
        Wire* w = gw->coreWire();
        PinKey a = {w->from().componentId, w->from().pinIndex};
        PinKey b = {w->to().componentId, w->to().pinIndex};
        bool isPowered = powered.count(a) && powered.count(b);
        gw->setPowered(isPowered);
    }
}
