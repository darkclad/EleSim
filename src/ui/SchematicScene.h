#pragma once

#include <QGraphicsScene>
#include <map>
#include <vector>
#include <memory>
#include "core/Component.h"
#include "simulation/SimulationResult.h"

class Circuit;
class GraphicComponent;
class GraphicWire;
class GraphicPin;
class QGraphicsLineItem;

enum class SceneMode {
    Select,
    DrawWire,
    PlaceComponent
};

class SchematicScene : public QGraphicsScene
{
    Q_OBJECT

public:
    static constexpr qreal GRID_SIZE = 20.0;

    explicit SchematicScene(QObject* parent = nullptr);
    ~SchematicScene() override;

    void setCircuit(Circuit* circuit) { m_circuit = circuit; }
    Circuit* circuit() const { return m_circuit; }

    // Component creation
    GraphicComponent* createComponent(ComponentType type, QPointF pos);

    // Lookup
    GraphicComponent* graphicForComponent(int componentId) const;

    // Mode
    void setMode(SceneMode mode);
    SceneMode mode() const { return m_mode; }

    // Click-to-place mode
    void enterPlaceMode(ComponentType type);

    // Delete selected items
    void deleteSelectedItems();

    // Called by GraphicWire to trigger undo snapshot before wire drag
    void notifyAboutToModify() { emit aboutToModifyCircuit(); }

    // Cancel current operation
    void cancelOperation();

    // Simulation results (for measurement overlay)
    void setSimulationResult(const SimulationResult& result);
    void clearSimulationResult();
    bool hasSimulationResult() const { return m_simResult.success; }

    // Rebuild scene from circuit data (after load)
    void rebuildFromCircuit();

    // Oscilloscope probe highlighting
    void highlightProbePin(int componentId, int pinIndex, const QColor& color);
    void clearProbeHighlights();

signals:
    void componentSelected(Component* comp);
    void selectionCleared();
    void modeChanged(SceneMode mode);
    void componentDoubleClicked(Component* comp);
    void simulationRecalcNeeded();
    void aboutToModifyCircuit();

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;

    // Drag-and-drop from toolbar
    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;

    // Mouse events for wire drawing
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSelectionChanged();

private:
    static QPointF snapToGrid(const QPointF& pos);
    static QPointF constrainTo45(const QPointF& anchor, const QPointF& target);

    // Find a GraphicPin at the given scene position (exact hit)
    GraphicPin* pinAt(const QPointF& scenePos) const;

    // Find nearest connectable pin within snap radius
    GraphicPin* nearestConnectablePin(const QPointF& scenePos) const;

    // Update highlight state during wire drawing
    void updateWireHighlight(const QPointF& mousePos);
    void clearWireHighlight();

    // Wire drawing
    void startWire(GraphicPin* pin);
    void finishWire(GraphicPin* endPin);
    void cancelWire();

    // Click-to-place helpers
    void createGhostComponent();
    void destroyGhostComponent();
    void cancelPlacement();

    // Wire proximity detection
    static qreal distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b);
    GraphicWire* wireAt(const QPointF& pos, qreal threshold = 15.0) const;
    void clearSplitHighlight();

    // Wire splitting
    void insertComponentOntoWire(GraphicComponent* comp, GraphicWire* wire);

    // T-junction
    void createTJunction(GraphicPin* fromPin, const QPointF& junctionPos, GraphicWire* targetWire);

    // Auto-connect overlapping pins
    void autoConnectPins(GraphicComponent* comp);

    static constexpr qreal PIN_SNAP_RADIUS = 20.0;

    Circuit* m_circuit = nullptr;
    SceneMode m_mode = SceneMode::Select;

    std::map<int, GraphicComponent*> m_graphicComponents; // componentId -> graphic
    std::vector<GraphicWire*> m_graphicWires;

    // Wire drawing state
    GraphicPin* m_wireStartPin = nullptr;
    GraphicPin* m_highlightedPin = nullptr; // currently highlighted target pin
    QGraphicsLineItem* m_tempWireLine = nullptr;
    QVector<QPointF> m_wirePivots; // accumulated pivot points during wire drawing
    std::vector<QGraphicsLineItem*> m_tempWireSegments; // committed segment visuals

    // Click-to-place state
    ComponentType m_placeType = ComponentType::Resistor;
    GraphicComponent* m_ghostComponent = nullptr;
    std::unique_ptr<Component> m_ghostCoreComponent;
    qreal m_ghostRotation = 0.0;

    // Wire split/junction highlight
    GraphicWire* m_splitTargetWire = nullptr;

    // Wire connectivity coloring
    void updateWireColors();

    // Simulation results
    SimulationResult m_simResult;
    std::map<int, QGraphicsItem*> m_measurementLabels; // componentId -> label

    // Probe-highlighted pins (for cleanup)
    std::vector<GraphicPin*> m_highlightedProbePins;
};
