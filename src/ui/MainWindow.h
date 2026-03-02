#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <memory>
#include "../simulation/SimulationConfig.h"

class QAction;
class QMenu;
class QToolBar;
class Circuit;
class SchematicScene;
class SchematicView;
class ComponentToolbar;
class PropertyPanel;
class UndoManager;
class OscilloscopeWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void markDirty();
    void markClean();
    void updateWindowTitle();
    bool maybeSave();
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void onSimulate();
    void onAnalyzeDC();
    void onAnalyzeAC();
    void onAnalyzeMixed();
    void startSimulation(const SimulationConfig& cfg);
    void onStopSimulation();
    void rerunSimulation();
    double detectACFrequency() const;

    // File operations
    void onNewCircuit();
    void onOpenCircuit();
    void openFile(const QString& filePath);
    void onSaveCircuit();
    void onSaveCircuitAs();
    void rebuildSceneFromCircuit();

    // Undo/Redo
    void saveUndoState();
    void restoreCircuit(const QJsonObject& state);
    void onUndo();
    void onRedo();

    // Recent files
    void addRecentFile(const QString& filePath);
    void updateRecentFilesMenu();
    QStringList recentFiles() const;
    void setRecentFiles(const QStringList& files);
    static constexpr int MAX_RECENT_FILES = 10;

    QString m_currentFilePath;
    bool m_isDirty = false;

    // Data model
    std::unique_ptr<Circuit> m_circuit;

    // Central widget
    SchematicScene* m_scene = nullptr;
    SchematicView*  m_view  = nullptr;

    // Component toolbar
    ComponentToolbar* m_componentToolBar = nullptr;

    // Property panel
    PropertyPanel* m_propertyPanel = nullptr;

    // Oscilloscope
    OscilloscopeWidget* m_oscilloscope = nullptr;

    // Undo manager
    UndoManager* m_undoManager = nullptr;

    // --- Menus ---
    QMenu* m_fileMenu       = nullptr;
    QMenu* m_editMenu       = nullptr;
    QMenu* m_simulationMenu = nullptr;
    QMenu* m_viewMenu       = nullptr;
    QMenu* m_helpMenu       = nullptr;
    QMenu* m_recentFilesMenu = nullptr;

    // --- File actions ---
    QAction* m_newAction    = nullptr;
    QAction* m_openAction   = nullptr;
    QAction* m_saveAction   = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_exitAction   = nullptr;

    // --- Edit actions ---
    QAction* m_undoAction   = nullptr;
    QAction* m_redoAction   = nullptr;
    QAction* m_deleteAction = nullptr;

    // --- Simulation actions ---
    QAction* m_simulateAction        = nullptr;
    QAction* m_stopSimAction         = nullptr;
    QMenu*   m_analysisMenu          = nullptr;
    QAction* m_analysisDCAction      = nullptr;
    QAction* m_analysisACAction      = nullptr;
    QAction* m_analysisMixedAction   = nullptr;

    // --- Help actions ---
    QAction* m_aboutAction = nullptr;

    // --- Toolbars ---
    QToolBar* m_mainToolBar = nullptr;

    // --- Simulation state ---
    bool m_simActive = false;
    SimulationConfig m_lastSimConfig;
};
