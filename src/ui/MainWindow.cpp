#include "MainWindow.h"
#include "SchematicScene.h"
#include "SchematicView.h"
#include "ComponentToolbar.h"
#include "PropertyPanel.h"
#include "graphics/GraphicComponent.h"
#include "../core/Circuit.h"
#include "../core/UndoManager.h"
#include "../core/components/ACSource.h"
#include "../core/components/Switch.h"
#include "../core/components/Switch3Way.h"
#include "../core/components/Switch4Way.h"
#include "graphics/items/GSwitch.h"
#include "graphics/items/GSwitch3Way.h"
#include "graphics/items/GSwitch4Way.h"
#include "../simulation/MNASolver.h"
#include "../simulation/SimulationResult.h"
#include "../simulation/SimulationConfig.h"
#include "OscilloscopeWidget.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QKeySequence>
#include <QInputDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QFile>
#include <QSettings>
#include <QStyle>

MainWindow::~MainWindow() = default;

void MainWindow::markDirty()
{
    if (!m_isDirty) {
        m_isDirty = true;
        updateWindowTitle();
    }
}

void MainWindow::markClean()
{
    m_isDirty = false;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString title;
    if (m_currentFilePath.isEmpty())
        title = "EleSim - Electrical Circuit Simulator";
    else
        title = tr("EleSim - %1").arg(QFileInfo(m_currentFilePath).fileName());

    if (m_isDirty)
        title.prepend("* ");

    setWindowTitle(title);
}

bool MainWindow::maybeSave()
{
    if (!m_isDirty)
        return true;

    auto btn = QMessageBox::warning(this, tr("Unsaved Changes"),
        tr("The circuit has been modified.\nDo you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (btn == QMessageBox::Save) {
        onSaveCircuit();
        return !m_isDirty; // false if user cancelled the save dialog
    }
    return btn == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_circuit(std::make_unique<Circuit>())
{
    setWindowTitle("EleSim - Electrical Circuit Simulator");
    resize(1200, 800);

    // Create undo manager
    m_undoManager = new UndoManager(this);

    // Create the schematic canvas
    m_scene = new SchematicScene(this);
    m_scene->setCircuit(m_circuit.get());
    m_view  = new SchematicView(m_scene, this);
    setCentralWidget(m_view);

    // Property panel (right dock)
    m_propertyPanel = new PropertyPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyPanel);

    // Oscilloscope (bottom dock, hidden by default)
    m_oscilloscope = new OscilloscopeWidget(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_oscilloscope);
    m_oscilloscope->hide();

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    // Connect selection signals to property panel and status bar
    connect(m_scene, &SchematicScene::componentSelected, this, [this](Component* comp) {
        m_propertyPanel->showProperties(comp);
        statusBar()->showMessage(
            tr("Selected: %1 (%2)").arg(comp->name(), comp->valueString()));
    });
    connect(m_scene, &SchematicScene::selectionCleared, this, [this]() {
        m_propertyPanel->clearProperties();
        statusBar()->showMessage(tr("Ready"));
    });

    // Double-click: toggle switches, or quick edit dialog for other components
    connect(m_scene, &SchematicScene::componentDoubleClicked, this, [this](Component* comp) {
        // Switch toggle on double-click
        if (comp->type() == ComponentType::Switch2Way) {
            saveUndoState();
            auto* sw = static_cast<Switch*>(comp);
            sw->toggle();
            auto* gc = m_scene->graphicForComponent(comp->id());
            if (auto* gs = dynamic_cast<GSwitch*>(gc)) gs->updateSwitchVisual();
            m_propertyPanel->showProperties(comp);
            m_scene->update();
            rerunSimulation();
            return;
        }
        if (comp->type() == ComponentType::Switch3Way) {
            saveUndoState();
            auto* sw = static_cast<Switch3Way*>(comp);
            sw->toggle();
            auto* gc = m_scene->graphicForComponent(comp->id());
            if (auto* gs = dynamic_cast<GSwitch3Way*>(gc)) gs->updateSwitchVisual();
            m_propertyPanel->showProperties(comp);
            m_scene->update();
            rerunSimulation();
            return;
        }
        if (comp->type() == ComponentType::Switch4Way) {
            saveUndoState();
            auto* sw = static_cast<Switch4Way*>(comp);
            sw->toggle();
            auto* gc = m_scene->graphicForComponent(comp->id());
            if (auto* gs = dynamic_cast<GSwitch4Way*>(gc)) gs->updateSwitchVisual();
            m_propertyPanel->showProperties(comp);
            m_scene->update();
            rerunSimulation();
            return;
        }

        // Non-switch: open value edit dialog
        QString current = QString::number(comp->value());
        QString label = tr("Enter value for %1 (%2):").arg(comp->name(), comp->valueUnit());

        bool ok = false;
        QString text = QInputDialog::getText(this, tr("Edit Value"), label,
                                              QLineEdit::Normal, current, &ok);
        if (ok && !text.isEmpty()) {
            double val = 0.0;
            if (parseEngineeringValue(text, val) && val > 0.0) {
                saveUndoState();
                comp->setValue(val);
                m_propertyPanel->showProperties(comp);
                m_scene->update();
                rerunSimulation();
            }
        }
    });

    // Auto-recalculate simulation when properties change
    connect(m_propertyPanel, &PropertyPanel::propertyChanged,
            this, &MainWindow::rerunSimulation);

    // Save undo state before property changes
    connect(m_propertyPanel, &PropertyPanel::aboutToApplyChanges,
            this, &MainWindow::saveUndoState);

    // Auto-recalculate simulation when components move
    connect(m_scene, &SchematicScene::simulationRecalcNeeded,
            this, &MainWindow::rerunSimulation);

    // Save undo state before circuit modifications from scene
    connect(m_scene, &SchematicScene::aboutToModifyCircuit,
            this, &MainWindow::saveUndoState);

    // Update undo/redo action states
    connect(m_undoManager, &UndoManager::stateChanged,
            this, [this](bool canUndo, bool canRedo) {
        m_undoAction->setEnabled(canUndo);
        m_redoAction->setEnabled(canRedo);
    });

    // Connect toolbar click to placement mode
    connect(m_componentToolBar, &ComponentToolbar::componentSelected,
            m_scene, &SchematicScene::enterPlaceMode);

    // Connect mode changes to status bar
    connect(m_scene, &SchematicScene::modeChanged, this, [this](SceneMode mode) {
        switch (mode) {
            case SceneMode::Select:
                statusBar()->showMessage(tr("Ready"));
                break;
            case SceneMode::DrawWire:
                statusBar()->showMessage(tr("Drawing wire - Click a pin to connect, ESC to cancel"));
                break;
            case SceneMode::PlaceComponent:
                statusBar()->showMessage(tr("Click to place component - Right-click to rotate, ESC to cancel"));
                break;
        }
    });

    // Connect oscilloscope probe changes to schematic highlights
    connect(m_oscilloscope, &OscilloscopeWidget::probeSelectionChanged, this, [this]() {
        m_scene->clearProbeHighlights();

        // CH1 probes — yellow
        ProbePin ch1Pos = m_oscilloscope->probeSelection(1, true);
        ProbePin ch1Neg = m_oscilloscope->probeSelection(1, false);
        if (ch1Pos.componentId >= 0)
            m_scene->highlightProbePin(ch1Pos.componentId, ch1Pos.pinIndex, QColor(255, 255, 50));
        if (ch1Neg.componentId >= 0)
            m_scene->highlightProbePin(ch1Neg.componentId, ch1Neg.pinIndex, QColor(255, 255, 50));

        // CH2 probes — cyan
        ProbePin ch2Pos = m_oscilloscope->probeSelection(2, true);
        ProbePin ch2Neg = m_oscilloscope->probeSelection(2, false);
        if (ch2Pos.componentId >= 0)
            m_scene->highlightProbePin(ch2Pos.componentId, ch2Pos.pinIndex, QColor(50, 255, 255));
        if (ch2Neg.componentId >= 0)
            m_scene->highlightProbePin(ch2Neg.componentId, ch2Neg.pinIndex, QColor(50, 255, 255));
    });

    // Initialize recent files menu
    updateRecentFilesMenu();
}

void MainWindow::createActions()
{
    auto icon = [this](QStyle::StandardPixmap sp) { return style()->standardIcon(sp); };

    // --- File actions ---
    m_newAction = new QAction(icon(QStyle::SP_FileIcon), tr("&New"), this);
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setStatusTip(tr("Create a new circuit"));
    connect(m_newAction, &QAction::triggered, this, &MainWindow::onNewCircuit);

    m_openAction = new QAction(icon(QStyle::SP_DialogOpenButton), tr("&Open..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip(tr("Open an existing circuit"));
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenCircuit);

    m_saveAction = new QAction(icon(QStyle::SP_DialogSaveButton), tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setStatusTip(tr("Save the current circuit"));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveCircuit);

    m_saveAsAction = new QAction(icon(QStyle::SP_DialogSaveButton), tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip(tr("Save the circuit to a new file"));
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveCircuitAs);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip(tr("Exit the application"));
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    // --- Edit actions ---
    m_undoAction = new QAction(icon(QStyle::SP_ArrowBack), tr("&Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    m_redoAction = new QAction(icon(QStyle::SP_ArrowForward), tr("&Redo"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    m_deleteAction = new QAction(icon(QStyle::SP_TrashIcon), tr("&Delete"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_deleteAction->setStatusTip(tr("Delete selected components"));
    connect(m_deleteAction, &QAction::triggered, this, [this]() {
        m_scene->deleteSelectedItems();
    });

    // --- Simulation actions ---
    m_simulateAction = new QAction(icon(QStyle::SP_MediaPlay), tr("&Simulate"), this);
    m_simulateAction->setShortcut(Qt::Key_F5);
    m_simulateAction->setStatusTip(tr("Run time-domain simulation"));
    connect(m_simulateAction, &QAction::triggered, this, &MainWindow::onSimulate);

    m_stopSimAction = new QAction(icon(QStyle::SP_MediaStop), tr("S&top"), this);
    m_stopSimAction->setShortcut(Qt::SHIFT | Qt::Key_F5);
    m_stopSimAction->setStatusTip(tr("Stop the running simulation"));
    m_stopSimAction->setEnabled(false);
    connect(m_stopSimAction, &QAction::triggered, this, &MainWindow::onStopSimulation);

    // --- Analysis actions ---
    m_analysisDCAction = new QAction(tr("&DC"), this);
    m_analysisDCAction->setStatusTip(tr("DC steady-state analysis"));
    connect(m_analysisDCAction, &QAction::triggered, this, &MainWindow::onAnalyzeDC);

    m_analysisACAction = new QAction(tr("&AC"), this);
    m_analysisACAction->setStatusTip(tr("AC frequency-domain analysis"));
    connect(m_analysisACAction, &QAction::triggered, this, &MainWindow::onAnalyzeAC);

    m_analysisMixedAction = new QAction(tr("DC+AC (&Superposition)"), this);
    m_analysisMixedAction->setStatusTip(tr("Combined DC and AC analysis using superposition"));
    connect(m_analysisMixedAction, &QAction::triggered, this, &MainWindow::onAnalyzeMixed);

    // --- Help actions ---
    m_aboutAction = new QAction(tr("&About EleSim"), this);
    m_aboutAction->setStatusTip(tr("About this application"));
    connect(m_aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About EleSim"),
            tr("<h2>EleSim v0.1.0</h2>"
               "<p>Electrical Circuit Simulator</p>"
               "<p>Design circuits and simulate DC/AC analysis "
               "with real-time measurements.</p>"));
    });
}

void MainWindow::createMenus()
{
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_openAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_saveAction);
    m_fileMenu->addAction(m_saveAsAction);
    m_fileMenu->addSeparator();
    m_recentFilesMenu = m_fileMenu->addMenu(tr("Recent Files"));
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_deleteAction);

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->addAction(m_oscilloscope->toggleViewAction());

    m_simulationMenu = menuBar()->addMenu(tr("&Simulation"));
    m_simulationMenu->addAction(m_simulateAction);
    m_simulationMenu->addAction(m_stopSimAction);
    m_simulationMenu->addSeparator();
    m_analysisMenu = m_simulationMenu->addMenu(tr("&Analysis"));
    m_analysisMenu->addAction(m_analysisDCAction);
    m_analysisMenu->addAction(m_analysisACAction);
    m_analysisMenu->addAction(m_analysisMixedAction);

    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_helpMenu->addAction(m_aboutAction);
}

void MainWindow::createToolBars()
{
    // Main toolbar
    m_mainToolBar = addToolBar(tr("Main"));
    m_mainToolBar->setMovable(false);
    m_mainToolBar->addAction(m_newAction);
    m_mainToolBar->addAction(m_openAction);
    m_mainToolBar->addAction(m_saveAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_undoAction);
    m_mainToolBar->addAction(m_redoAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_simulateAction);
    m_mainToolBar->addAction(m_stopSimAction);

    // Component toolbar (left side - drag to place)
    m_componentToolBar = new ComponentToolbar(this);
    addToolBar(Qt::LeftToolBarArea, m_componentToolBar);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready - Drag components from the left toolbar onto the canvas"));
}

// --- Undo/Redo ---

void MainWindow::saveUndoState()
{
    m_undoManager->pushState(m_circuit->toJson());
    markDirty();
}

void MainWindow::restoreCircuit(const QJsonObject& state)
{
    m_circuit = Circuit::fromJson(state);
    m_scene->setCircuit(m_circuit.get());
    m_scene->rebuildFromCircuit();
    m_propertyPanel->clearProperties();
    rerunSimulation();
}

void MainWindow::onUndo()
{
    QJsonObject currentState = m_circuit->toJson();
    QJsonObject previousState = m_undoManager->undo(currentState);
    if (previousState.isEmpty()) return;
    restoreCircuit(previousState);
    markDirty();
    statusBar()->showMessage(tr("Undo"));
}

void MainWindow::onRedo()
{
    QJsonObject currentState = m_circuit->toJson();
    QJsonObject nextState = m_undoManager->redo(currentState);
    if (nextState.isEmpty()) return;
    restoreCircuit(nextState);
    markDirty();
    statusBar()->showMessage(tr("Redo"));
}

// --- Simulation ---

double MainWindow::detectACFrequency() const
{
    double freq = 0.0;
    for (auto& [id, comp] : m_circuit->components()) {
        if (comp->type() == ComponentType::ACSource) {
            auto* ac = static_cast<ACSource*>(comp.get());
            if (ac->frequency() > freq)
                freq = ac->frequency();
        }
    }
    return freq;
}

void MainWindow::onSimulate()
{
    SimulationConfig cfg;
    cfg.type = AnalysisType::Transient;

    double freq = detectACFrequency();
    if (freq > 0.0) {
        cfg.timeStep = 1.0 / (freq * 100.0);
        cfg.totalTime = 5.0 / freq;
        cfg.acFrequency = freq;
    } else {
        cfg.timeStep = 0.001;
        cfg.totalTime = 0.1;
    }
    startSimulation(cfg);
}

void MainWindow::onAnalyzeDC()
{
    SimulationConfig cfg;
    cfg.type = AnalysisType::DC;
    startSimulation(cfg);
}

void MainWindow::onAnalyzeAC()
{
    SimulationConfig cfg;
    cfg.type = AnalysisType::AC;
    double freq = detectACFrequency();
    cfg.acFrequency = (freq > 0.0) ? freq : 50.0;
    startSimulation(cfg);
}

void MainWindow::onAnalyzeMixed()
{
    SimulationConfig cfg;
    cfg.type = AnalysisType::Mixed;
    double freq = detectACFrequency();
    cfg.acFrequency = (freq > 0.0) ? freq : 50.0;
    startSimulation(cfg);
}

void MainWindow::startSimulation(const SimulationConfig& cfg)
{
    SimulationResult result;
    QString modeName;

    switch (cfg.type) {
        case AnalysisType::DC:
            modeName = "DC Analysis";
            result = MNASolver::solveDC(*m_circuit);
            break;
        case AnalysisType::AC:
            modeName = "AC Analysis";
            result = MNASolver::solveAC(*m_circuit, cfg.acFrequency);
            break;
        case AnalysisType::Mixed:
            modeName = "DC+AC Analysis";
            result = MNASolver::solveMixed(*m_circuit, cfg.acFrequency);
            break;
        case AnalysisType::Transient: {
            modeName = "Simulation";
            result = MNASolver::solveTransient(*m_circuit, cfg.timeStep, cfg.totalTime);

            // Also run full transient for oscilloscope
            auto tResult = MNASolver::solveTransientFull(*m_circuit, cfg.timeStep, cfg.totalTime);
            if (tResult.success) {
                m_oscilloscope->updateComponentList(m_circuit.get());
                m_oscilloscope->setTransientResult(tResult);
                m_oscilloscope->show();
            }
            break;
        }
    }

    if (!result.success) {
        QMessageBox::warning(this, tr("Simulation Error"), result.errorMessage);
        statusBar()->showMessage(tr("Simulation failed"));
        return;
    }

    m_scene->setSimulationResult(result);
    m_propertyPanel->setCircuit(m_circuit.get());
    m_propertyPanel->setSimulationConfig(cfg);
    m_propertyPanel->setSimulationResult(result);

    m_lastSimConfig = cfg;
    m_simActive = true;
    m_stopSimAction->setEnabled(true);

    statusBar()->showMessage(tr("%1 active").arg(modeName));
}

void MainWindow::onStopSimulation()
{
    m_simActive = false;
    m_stopSimAction->setEnabled(false);
    m_scene->clearSimulationResult();
    m_propertyPanel->clearSimulationResult();
    m_oscilloscope->clearData();
    statusBar()->showMessage(tr("Simulation stopped"));
}

void MainWindow::rerunSimulation()
{
    if (!m_simActive) return;

    SimulationResult result;
    switch (m_lastSimConfig.type) {
        case AnalysisType::DC:
            result = MNASolver::solveDC(*m_circuit);
            break;
        case AnalysisType::AC:
            result = MNASolver::solveAC(*m_circuit, m_lastSimConfig.acFrequency);
            break;
        case AnalysisType::Mixed:
            result = MNASolver::solveMixed(*m_circuit, m_lastSimConfig.acFrequency);
            break;
        case AnalysisType::Transient:
            result = MNASolver::solveTransient(*m_circuit, m_lastSimConfig.timeStep, m_lastSimConfig.totalTime);
            {
                auto tResult = MNASolver::solveTransientFull(*m_circuit, m_lastSimConfig.timeStep, m_lastSimConfig.totalTime);
                if (tResult.success) {
                    m_oscilloscope->updateComponentList(m_circuit.get());
                    m_oscilloscope->setTransientResult(tResult);
                }
            }
            break;
    }

    if (result.success) {
        m_scene->setSimulationResult(result);
        m_propertyPanel->setCircuit(m_circuit.get());
        m_propertyPanel->setSimulationConfig(m_lastSimConfig);
        m_propertyPanel->setSimulationResult(result);
    } else {
        m_scene->clearSimulationResult();
        m_propertyPanel->clearSimulationResult();
        statusBar()->showMessage(tr("Simulation: %1").arg(result.errorMessage));
    }
}

// --- File operations ---

void MainWindow::onNewCircuit()
{
    if (!maybeSave()) return;
    if (m_simActive) onStopSimulation();

    m_circuit = std::make_unique<Circuit>();
    m_scene->setCircuit(m_circuit.get());
    m_scene->rebuildFromCircuit();
    m_currentFilePath.clear();
    m_undoManager->clear();
    markClean();
    statusBar()->showMessage(tr("New circuit created"));
}

void MainWindow::onOpenCircuit()
{
    if (!maybeSave()) return;

    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open Circuit"), QString(),
        tr("EleSim Files (*.esim);;JSON Files (*.json);;All Files (*)"));

    if (filePath.isEmpty()) return;
    openFile(filePath);
}

void MainWindow::openFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open Error"),
                             tr("Could not open file: %1").arg(file.errorString()));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, tr("Parse Error"),
                             tr("Invalid file format: %1").arg(parseError.errorString()));
        return;
    }

    auto loaded = Circuit::fromJson(doc.object());
    if (!loaded) {
        QMessageBox::warning(this, tr("Load Error"), tr("Failed to load circuit data"));
        return;
    }

    if (m_simActive) onStopSimulation();

    m_circuit = std::move(loaded);
    m_scene->setCircuit(m_circuit.get());
    m_scene->rebuildFromCircuit();
    m_currentFilePath = filePath;
    m_undoManager->clear();
    addRecentFile(filePath);

    // Restore view state
    QJsonObject root = doc.object();
    if (root.contains("view")) {
        QJsonObject viewObj = root["view"].toObject();
        double zoom = viewObj["zoom"].toDouble(1.0);
        QPointF center(viewObj["centerX"].toDouble(0.0),
                       viewObj["centerY"].toDouble(0.0));
        m_view->setViewState(zoom, center);
    } else {
        m_view->setViewState(1.0, QPointF(0, 0));
    }

    markClean();
    statusBar()->showMessage(tr("Opened: %1").arg(filePath));
}

void MainWindow::onSaveCircuit()
{
    if (m_currentFilePath.isEmpty()) {
        onSaveCircuitAs();
        return;
    }

    QJsonObject root = m_circuit->toJson();

    // Save view state
    QJsonObject viewObj;
    viewObj["zoom"] = m_view->zoom();
    QPointF center = m_view->viewCenter();
    viewObj["centerX"] = center.x();
    viewObj["centerY"] = center.y();
    root["view"] = viewObj;

    QJsonDocument doc(root);

    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Save Error"),
                             tr("Could not save file: %1").arg(file.errorString()));
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    addRecentFile(m_currentFilePath);
    markClean();
    statusBar()->showMessage(tr("Saved: %1").arg(m_currentFilePath));
}

void MainWindow::onSaveCircuitAs()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Circuit"), QString(),
        tr("EleSim Files (*.esim);;JSON Files (*.json);;All Files (*)"));

    if (filePath.isEmpty()) return;

    // Ensure .esim extension
    if (!filePath.endsWith(".esim") && !filePath.endsWith(".json")) {
        filePath += ".esim";
    }

    m_currentFilePath = filePath;
    onSaveCircuit();
}

void MainWindow::rebuildSceneFromCircuit()
{
    m_scene->rebuildFromCircuit();
}

// --- Recent Files ---

QStringList MainWindow::recentFiles() const
{
    QSettings settings;
    return settings.value("recentFiles").toStringList();
}

void MainWindow::setRecentFiles(const QStringList& files)
{
    QSettings settings;
    settings.setValue("recentFiles", files);
}

void MainWindow::addRecentFile(const QString& filePath)
{
    QStringList files = recentFiles();
    files.removeAll(filePath);
    files.prepend(filePath);
    while (files.size() > MAX_RECENT_FILES)
        files.removeLast();
    setRecentFiles(files);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();

    QStringList files = recentFiles();
    QStringList validFiles;
    for (const QString& path : files) {
        if (QFileInfo::exists(path))
            validFiles.append(path);
    }
    if (validFiles.size() != files.size())
        setRecentFiles(validFiles);

    if (validFiles.isEmpty()) {
        m_recentFilesMenu->setEnabled(false);
        return;
    }
    m_recentFilesMenu->setEnabled(true);

    for (int i = 0; i < validFiles.size(); ++i) {
        const QString& filePath = validFiles[i];
        QFileInfo fi(filePath);
        QString text = tr("&%1 %2").arg(i + 1).arg(fi.fileName());
        QAction* action = m_recentFilesMenu->addAction(text);
        connect(action, &QAction::triggered, this, [this, filePath]() {
            openFile(filePath);
        });
    }

    m_recentFilesMenu->addSeparator();
    QAction* clearAction = m_recentFilesMenu->addAction(tr("Clear Recent Files"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        setRecentFiles({});
        updateRecentFilesMenu();
    });
}
