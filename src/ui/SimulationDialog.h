#pragma once

#include <QDialog>
#include "simulation/SimulationConfig.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QDialogButtonBox;

class SimulationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SimulationDialog(QWidget* parent = nullptr);

    SimulationConfig config() const;

private slots:
    void onTypeChanged(int index);

private:
    QComboBox* m_typeCombo = nullptr;

    // AC fields
    QLabel* m_freqLabel = nullptr;
    QDoubleSpinBox* m_freqSpin = nullptr;

    // Transient fields
    QLabel* m_timeStepLabel = nullptr;
    QDoubleSpinBox* m_timeStepSpin = nullptr;
    QLabel* m_totalTimeLabel = nullptr;
    QDoubleSpinBox* m_totalTimeSpin = nullptr;

    QDialogButtonBox* m_buttons = nullptr;
};
