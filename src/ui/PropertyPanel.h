#pragma once

#include <QDockWidget>
#include "core/Component.h"
#include "simulation/SimulationResult.h"
#include "simulation/SimulationConfig.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QFormLayout;
class QFrame;
class Circuit;

class PropertyPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget* parent = nullptr);

public slots:
    void showProperties(Component* comp);
    void clearProperties();
    void setSimulationResult(const SimulationResult& result);
    void setSimulationConfig(const SimulationConfig& config);
    void setCircuit(Circuit* circuit);
    void clearSimulationResult();

signals:
    void propertyChanged();
    void aboutToApplyChanges();

private slots:
    void applyChanges();

private:
    void setupUi();
    void updateSimulationStats();

    Component* m_currentComponent = nullptr;
    SimulationResult m_simResult;
    SimulationConfig m_simConfig;
    Circuit* m_circuit = nullptr;
    bool m_hasSimResult = false;

    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_typeLabel = nullptr;
    QLineEdit* m_valueEdit = nullptr;
    QLabel* m_unitLabel = nullptr;

    // AC/Pulse source extra fields
    QLabel* m_freqLabel = nullptr;
    QLineEdit* m_freqEdit = nullptr;
    QLabel* m_phaseLabel = nullptr;
    QLineEdit* m_phaseEdit = nullptr;
    QLabel* m_dutyLabel = nullptr;
    QLineEdit* m_dutyEdit = nullptr;

    // Zener diode extra field
    QLabel* m_zenerVoltageLabel = nullptr;
    QLineEdit* m_zenerVoltageEdit = nullptr;

    // Switch state
    QLabel* m_switchStateLabel = nullptr;
    QLabel* m_switchStateValue = nullptr;

    QPushButton* m_applyButton = nullptr;
    QFormLayout* m_formLayout = nullptr;

    // Simulation stats section
    QFrame* m_statsFrame = nullptr;
    QLabel* m_statsTitle = nullptr;
    QLabel* m_statsVoltage = nullptr;
    QLabel* m_statsCurrent = nullptr;
    QLabel* m_statsPower = nullptr;
    // Mixed analysis breakdown
    QLabel* m_statsDCLabel = nullptr;
    QLabel* m_statsACLabel = nullptr;

    // LC circuit analysis stats
    QLabel* m_statsReactance = nullptr;
    QLabel* m_statsNetImpedance = nullptr;
    QLabel* m_statsResonantFreq = nullptr;
    QLabel* m_statsPeakCurrent = nullptr;
};

// Parse engineering notation: "4.7k" -> 4700.0, "10M" -> 10e6
// Returns true on success
bool parseEngineeringValue(const QString& text, double& result);
