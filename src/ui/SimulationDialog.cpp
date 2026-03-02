#include "SimulationDialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

SimulationDialog::SimulationDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Run Simulation"));
    setMinimumWidth(300);

    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout;

    m_typeCombo = new QComboBox;
    m_typeCombo->addItem(tr("DC Analysis"));
    m_typeCombo->addItem(tr("AC Analysis"));
    m_typeCombo->addItem(tr("Transient Analysis"));
    form->addRow(tr("Analysis Type:"), m_typeCombo);

    // AC fields
    m_freqLabel = new QLabel(tr("Frequency:"));
    m_freqSpin = new QDoubleSpinBox;
    m_freqSpin->setRange(0.01, 1e9);
    m_freqSpin->setValue(50.0);
    m_freqSpin->setSuffix(" Hz");
    m_freqSpin->setDecimals(2);
    form->addRow(m_freqLabel, m_freqSpin);

    // Transient fields
    m_timeStepLabel = new QLabel(tr("Time Step:"));
    m_timeStepSpin = new QDoubleSpinBox;
    m_timeStepSpin->setRange(1e-6, 1.0);
    m_timeStepSpin->setValue(0.001);
    m_timeStepSpin->setSuffix(" s");
    m_timeStepSpin->setDecimals(6);
    form->addRow(m_timeStepLabel, m_timeStepSpin);

    m_totalTimeLabel = new QLabel(tr("Total Time:"));
    m_totalTimeSpin = new QDoubleSpinBox;
    m_totalTimeSpin->setRange(0.001, 10.0);
    m_totalTimeSpin->setValue(0.1);
    m_totalTimeSpin->setSuffix(" s");
    m_totalTimeSpin->setDecimals(4);
    form->addRow(m_totalTimeLabel, m_totalTimeSpin);

    layout->addLayout(form);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimulationDialog::onTypeChanged);

    onTypeChanged(0); // Initialize visibility
}

void SimulationDialog::onTypeChanged(int index)
{
    bool isAC = (index == 1);
    bool isTransient = (index == 2);

    m_freqLabel->setVisible(isAC);
    m_freqSpin->setVisible(isAC);
    m_timeStepLabel->setVisible(isTransient);
    m_timeStepSpin->setVisible(isTransient);
    m_totalTimeLabel->setVisible(isTransient);
    m_totalTimeSpin->setVisible(isTransient);
}

SimulationConfig SimulationDialog::config() const
{
    SimulationConfig cfg;
    switch (m_typeCombo->currentIndex()) {
        case 0: cfg.type = AnalysisType::DC; break;
        case 1: cfg.type = AnalysisType::AC; break;
        case 2: cfg.type = AnalysisType::Transient; break;
    }
    cfg.acFrequency = m_freqSpin->value();
    cfg.timeStep = m_timeStepSpin->value();
    cfg.totalTime = m_totalTimeSpin->value();
    return cfg;
}
