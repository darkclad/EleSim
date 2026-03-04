#include "PropertyPanel.h"
#include "../core/Circuit.h"
#include "../core/components/ACSource.h"
#include "../core/components/PulseSource.h"
#include "../core/components/Switch.h"
#include "../core/components/Switch3Way.h"
#include "../core/components/Switch4Way.h"
#include "../core/components/ZenerDiode.h"
#include "../core/components/NMosfet.h"
#include "../core/components/PMosfet.h"
#include "../core/components/NOTGate.h"
#include "../core/components/ANDGate.h"
#include "../core/components/ORGate.h"
#include "../core/components/XORGate.h"

#include <cmath>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QFrame>
#include <QRegularExpression>

// --- Engineering value parser ---

bool parseEngineeringValue(const QString& text, double& result)
{
    static const QRegularExpression re(
        R"(^\s*([+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)\s*([TGMkmunp\xC2\xB5]?)\s*\S*\s*$)");

    auto match = re.match(text.trimmed());
    if (!match.hasMatch())
        return false;

    bool ok = false;
    double num = match.captured(1).toDouble(&ok);
    if (!ok) return false;

    QString prefix = match.captured(2);
    double scale = 1.0;

    if (prefix == "T") scale = 1e12;
    else if (prefix == "G") scale = 1e9;
    else if (prefix == "M") scale = 1e6;
    else if (prefix == "k") scale = 1e3;
    else if (prefix == "m") scale = 1e-3;
    else if (prefix == "u" || prefix == "\xC2\xB5") scale = 1e-6;
    else if (prefix == "n") scale = 1e-9;
    else if (prefix == "p") scale = 1e-12;

    result = num * scale;
    return true;
}

// --- PropertyPanel ---

PropertyPanel::PropertyPanel(QWidget* parent)
    : QDockWidget(tr("Properties"), parent)
{
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    setMinimumWidth(200);
    setupUi();
    clearProperties();
}

void PropertyPanel::setupUi()
{
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setStyleSheet("font-weight: bold; font-size: 14px; border: 1px solid #ccc; padding: 2px;");
    m_nameEdit->setPlaceholderText(tr("Component name"));
    layout->addWidget(m_nameEdit);

    m_typeLabel = new QLabel;
    m_typeLabel->setStyleSheet("color: #666;");
    layout->addWidget(m_typeLabel);

    layout->addSpacing(8);

    m_formLayout = new QFormLayout;
    m_formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Value row
    auto* valueRow = new QHBoxLayout;
    m_valueEdit = new QLineEdit;
    m_unitLabel = new QLabel;
    valueRow->addWidget(m_valueEdit);
    valueRow->addWidget(m_unitLabel);
    m_formLayout->addRow(tr("Value:"), valueRow);

    // AC source frequency
    m_freqLabel = new QLabel(tr("Frequency:"));
    m_freqEdit = new QLineEdit;
    auto* freqRow = new QHBoxLayout;
    freqRow->addWidget(m_freqEdit);
    freqRow->addWidget(new QLabel("Hz"));
    m_formLayout->addRow(m_freqLabel, freqRow);

    // AC source phase
    m_phaseLabel = new QLabel(tr("Phase:"));
    m_phaseEdit = new QLineEdit;
    auto* phaseRow = new QHBoxLayout;
    phaseRow->addWidget(m_phaseEdit);
    phaseRow->addWidget(new QLabel("\xC2\xB0")); // degree sign
    m_formLayout->addRow(m_phaseLabel, phaseRow);

    // Pulse source duty cycle
    m_dutyLabel = new QLabel(tr("Duty Cycle:"));
    m_dutyEdit = new QLineEdit;
    auto* dutyRow = new QHBoxLayout;
    dutyRow->addWidget(m_dutyEdit);
    dutyRow->addWidget(new QLabel("%"));
    m_formLayout->addRow(m_dutyLabel, dutyRow);

    // Zener diode voltage
    m_zenerVoltageLabel = new QLabel(tr("Zener Voltage:"));
    m_zenerVoltageEdit = new QLineEdit;
    auto* zenerRow = new QHBoxLayout;
    zenerRow->addWidget(m_zenerVoltageEdit);
    zenerRow->addWidget(new QLabel("V"));
    m_formLayout->addRow(m_zenerVoltageLabel, zenerRow);

    // Switch state
    m_switchStateLabel = new QLabel(tr("State:"));
    m_switchStateValue = new QLabel;
    m_switchStateValue->setStyleSheet("font-weight: bold;");
    m_formLayout->addRow(m_switchStateLabel, m_switchStateValue);

    layout->addLayout(m_formLayout);

    layout->addSpacing(8);

    m_applyButton = new QPushButton(tr("Apply"));
    layout->addWidget(m_applyButton);

    // --- Simulation stats section ---
    m_statsFrame = new QFrame;
    m_statsFrame->setFrameShape(QFrame::StyledPanel);
    m_statsFrame->setStyleSheet(
        "QFrame { background: #f0f4ff; border: 1px solid #c0c8e0; border-radius: 4px; padding: 6px; }");
    auto* statsLayout = new QVBoxLayout(m_statsFrame);
    statsLayout->setContentsMargins(6, 4, 6, 4);
    statsLayout->setSpacing(2);

    m_statsTitle = new QLabel(tr("Simulation Results"));
    m_statsTitle->setStyleSheet("font-weight: bold; color: #0048aa; border: none; background: transparent;");
    statsLayout->addWidget(m_statsTitle);

    m_statsVoltage = new QLabel;
    m_statsVoltage->setStyleSheet("color: #333; border: none; background: transparent;");
    statsLayout->addWidget(m_statsVoltage);

    m_statsCurrent = new QLabel;
    m_statsCurrent->setStyleSheet("color: #333; border: none; background: transparent;");
    statsLayout->addWidget(m_statsCurrent);

    m_statsPower = new QLabel;
    m_statsPower->setStyleSheet("color: #333; border: none; background: transparent;");
    statsLayout->addWidget(m_statsPower);

    m_statsDCLabel = new QLabel;
    m_statsDCLabel->setStyleSheet("color: #555; border: none; background: transparent;");
    statsLayout->addWidget(m_statsDCLabel);

    m_statsACLabel = new QLabel;
    m_statsACLabel->setStyleSheet("color: #555; border: none; background: transparent;");
    statsLayout->addWidget(m_statsACLabel);

    // LC analysis stats
    QString lcStyle = "color: #006868; border: none; background: transparent;";
    m_statsReactance = new QLabel;
    m_statsReactance->setStyleSheet(lcStyle);
    statsLayout->addWidget(m_statsReactance);

    m_statsNetImpedance = new QLabel;
    m_statsNetImpedance->setStyleSheet(lcStyle);
    statsLayout->addWidget(m_statsNetImpedance);

    m_statsResonantFreq = new QLabel;
    m_statsResonantFreq->setStyleSheet(lcStyle);
    statsLayout->addWidget(m_statsResonantFreq);

    m_statsPeakCurrent = new QLabel;
    m_statsPeakCurrent->setStyleSheet(lcStyle);
    statsLayout->addWidget(m_statsPeakCurrent);

    m_statsFrame->setVisible(false);
    layout->addWidget(m_statsFrame);

    layout->addStretch();

    setWidget(container);

    connect(m_applyButton, &QPushButton::clicked, this, &PropertyPanel::applyChanges);
    // Also apply on Enter in any edit field
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &PropertyPanel::applyChanges);
    connect(m_valueEdit, &QLineEdit::returnPressed, this, &PropertyPanel::applyChanges);
    connect(m_freqEdit, &QLineEdit::returnPressed, this, &PropertyPanel::applyChanges);
    connect(m_phaseEdit, &QLineEdit::returnPressed, this, &PropertyPanel::applyChanges);
}

void PropertyPanel::showProperties(Component* comp)
{
    m_currentComponent = comp;

    if (!comp) {
        clearProperties();
        return;
    }

    m_nameEdit->setText(comp->name());

    // Type label
    switch (comp->type()) {
        case ComponentType::Resistor:  m_typeLabel->setText(tr("Resistor")); break;
        case ComponentType::Capacitor: m_typeLabel->setText(tr("Capacitor")); break;
        case ComponentType::DCSource:  m_typeLabel->setText(tr("DC Voltage Source")); break;
        case ComponentType::ACSource:  m_typeLabel->setText(tr("AC Voltage Source")); break;
        case ComponentType::Lamp:      m_typeLabel->setText(tr("Lamp")); break;
        case ComponentType::Switch2Way: m_typeLabel->setText(tr("Two-way Switch")); break;
        case ComponentType::Switch3Way: m_typeLabel->setText(tr("Three-way Switch")); break;
        case ComponentType::Switch4Way: m_typeLabel->setText(tr("Four-way Switch")); break;
        case ComponentType::Inductor:  m_typeLabel->setText(tr("Inductor")); break;
        case ComponentType::Diode:     m_typeLabel->setText(tr("Diode")); break;
        case ComponentType::ZenerDiode: m_typeLabel->setText(tr("Zener Diode")); break;
        case ComponentType::PulseSource: m_typeLabel->setText(tr("Pulse Source")); break;
        case ComponentType::DCCurrentSource: m_typeLabel->setText(tr("DC Current Source")); break;
        case ComponentType::NMosfet: m_typeLabel->setText(tr("N-Channel MOSFET")); break;
        case ComponentType::PMosfet: m_typeLabel->setText(tr("P-Channel MOSFET")); break;
        case ComponentType::NOTGate: m_typeLabel->setText(tr("NOT Gate")); break;
        case ComponentType::ANDGate: m_typeLabel->setText(tr("AND Gate")); break;
        case ComponentType::ORGate: m_typeLabel->setText(tr("OR Gate")); break;
        case ComponentType::XORGate: m_typeLabel->setText(tr("XOR Gate")); break;
        default: break;
    }

    // Value
    m_valueEdit->setText(QString::number(comp->value()));
    m_unitLabel->setText(comp->valueUnit());

    // Show/hide frequency/phase/duty fields
    bool isAC = (comp->type() == ComponentType::ACSource);
    bool isPulse = (comp->type() == ComponentType::PulseSource);
    bool hasFreqPhase = isAC || isPulse;
    m_freqLabel->setVisible(hasFreqPhase);
    m_freqEdit->setVisible(hasFreqPhase);
    m_phaseLabel->setVisible(hasFreqPhase);
    m_phaseEdit->setVisible(hasFreqPhase);
    m_dutyLabel->setVisible(isPulse);
    m_dutyEdit->setVisible(isPulse);

    // Show/hide zener voltage field
    bool isZener = (comp->type() == ComponentType::ZenerDiode);
    m_zenerVoltageLabel->setVisible(isZener);
    m_zenerVoltageEdit->setVisible(isZener);

    if (isAC) {
        auto* ac = static_cast<ACSource*>(comp);
        m_freqEdit->setText(QString::number(ac->frequency()));
        m_phaseEdit->setText(QString::number(ac->phase()));
    } else if (isPulse) {
        auto* ps = static_cast<PulseSource*>(comp);
        m_freqEdit->setText(QString::number(ps->frequency()));
        m_phaseEdit->setText(QString::number(ps->phase()));
        m_dutyEdit->setText(QString::number(ps->dutyCycle() * 100.0));
    }

    if (isZener) {
        auto* zd = static_cast<ZenerDiode*>(comp);
        m_zenerVoltageEdit->setText(QString::number(zd->zenerVoltage()));
    }

    // Show/hide switch state
    bool isSwitch = (comp->type() == ComponentType::Switch2Way ||
                     comp->type() == ComponentType::Switch3Way ||
                     comp->type() == ComponentType::Switch4Way);
    m_switchStateLabel->setVisible(isSwitch);
    m_switchStateValue->setVisible(isSwitch);

    if (comp->type() == ComponentType::Switch2Way) {
        auto* sw = static_cast<Switch*>(comp);
        m_switchStateValue->setText(sw->isClosed() ? tr("Closed") : tr("Open"));
    } else if (comp->type() == ComponentType::Switch3Way) {
        auto* sw = static_cast<Switch3Way*>(comp);
        m_switchStateValue->setText(sw->activePosition() == 0 ? tr("Position A") : tr("Position B"));
    } else if (comp->type() == ComponentType::Switch4Way) {
        auto* sw = static_cast<Switch4Way*>(comp);
        m_switchStateValue->setText(sw->isCrossed() ? tr("Crossed") : tr("Straight"));
    }

    // Show everything
    m_nameEdit->setVisible(true);
    m_typeLabel->setVisible(true);
    m_valueEdit->setVisible(true);
    m_unitLabel->setVisible(true);
    m_applyButton->setVisible(true);

    // Update simulation stats for the selected component
    updateSimulationStats();

    // Also show/hide the frequency/phase/duty row labels in the form layout
    for (int i = 0; i < m_formLayout->rowCount(); ++i) {
        auto labelItem = m_formLayout->itemAt(i, QFormLayout::LabelRole);
        auto fieldItem = m_formLayout->itemAt(i, QFormLayout::FieldRole);
        if (labelItem && labelItem->widget() == m_freqLabel) {
            labelItem->widget()->setVisible(hasFreqPhase);
            if (fieldItem && fieldItem->layout()) {
                for (int j = 0; j < fieldItem->layout()->count(); ++j)
                    if (auto* w = fieldItem->layout()->itemAt(j)->widget())
                        w->setVisible(hasFreqPhase);
            }
        }
        if (labelItem && labelItem->widget() == m_phaseLabel) {
            labelItem->widget()->setVisible(hasFreqPhase);
            if (fieldItem && fieldItem->layout()) {
                for (int j = 0; j < fieldItem->layout()->count(); ++j)
                    if (auto* w = fieldItem->layout()->itemAt(j)->widget())
                        w->setVisible(hasFreqPhase);
            }
        }
        if (labelItem && labelItem->widget() == m_dutyLabel) {
            labelItem->widget()->setVisible(isPulse);
            if (fieldItem && fieldItem->layout()) {
                for (int j = 0; j < fieldItem->layout()->count(); ++j)
                    if (auto* w = fieldItem->layout()->itemAt(j)->widget())
                        w->setVisible(isPulse);
            }
        }
        if (labelItem && labelItem->widget() == m_zenerVoltageLabel) {
            labelItem->widget()->setVisible(isZener);
            if (fieldItem && fieldItem->layout()) {
                for (int j = 0; j < fieldItem->layout()->count(); ++j)
                    if (auto* w = fieldItem->layout()->itemAt(j)->widget())
                        w->setVisible(isZener);
            }
        }
    }
}

void PropertyPanel::clearProperties()
{
    m_currentComponent = nullptr;
    m_nameEdit->clear();
    m_nameEdit->setVisible(false);
    m_typeLabel->setText("");
    m_valueEdit->clear();
    m_unitLabel->setText("");
    m_freqEdit->clear();
    m_phaseEdit->clear();
    m_dutyEdit->clear();
    m_zenerVoltageEdit->clear();

    m_valueEdit->setVisible(false);
    m_unitLabel->setVisible(false);
    m_freqLabel->setVisible(false);
    m_freqEdit->setVisible(false);
    m_phaseLabel->setVisible(false);
    m_phaseEdit->setVisible(false);
    m_dutyLabel->setVisible(false);
    m_dutyEdit->setVisible(false);
    m_zenerVoltageLabel->setVisible(false);
    m_zenerVoltageEdit->setVisible(false);
    m_switchStateLabel->setVisible(false);
    m_switchStateValue->setVisible(false);
    m_applyButton->setVisible(false);
    m_statsFrame->setVisible(false);

    // Hide freq/phase row extra widgets
    for (int i = 0; i < m_formLayout->rowCount(); ++i) {
        auto labelItem = m_formLayout->itemAt(i, QFormLayout::LabelRole);
        auto fieldItem = m_formLayout->itemAt(i, QFormLayout::FieldRole);
        if (labelItem && (labelItem->widget() == m_freqLabel || labelItem->widget() == m_phaseLabel || labelItem->widget() == m_dutyLabel)) {
            if (fieldItem && fieldItem->layout()) {
                for (int j = 0; j < fieldItem->layout()->count(); ++j)
                    if (auto* w = fieldItem->layout()->itemAt(j)->widget())
                        w->setVisible(false);
            }
        }
    }
}

void PropertyPanel::applyChanges()
{
    if (!m_currentComponent) return;

    emit aboutToApplyChanges();

    // Update component name
    QString newName = m_nameEdit->text().trimmed();
    if (!newName.isEmpty() && newName != m_currentComponent->name()) {
        m_currentComponent->setName(newName);
    }

    // Parse main value
    double val = 0.0;
    if (parseEngineeringValue(m_valueEdit->text(), val)) {
        if (val > 0.0) {
            m_currentComponent->setValue(val);
        }
    }

    // AC source extra fields
    if (m_currentComponent->type() == ComponentType::ACSource) {
        auto* ac = static_cast<ACSource*>(m_currentComponent);

        bool ok = false;
        double freq = m_freqEdit->text().toDouble(&ok);
        if (ok && freq > 0.0)
            ac->setFrequency(freq);

        double phase = m_phaseEdit->text().toDouble(&ok);
        if (ok)
            ac->setPhase(phase);
    }

    // Pulse source extra fields
    if (m_currentComponent->type() == ComponentType::PulseSource) {
        auto* ps = static_cast<PulseSource*>(m_currentComponent);

        bool ok = false;
        double freq = m_freqEdit->text().toDouble(&ok);
        if (ok && freq > 0.0)
            ps->setFrequency(freq);

        double phase = m_phaseEdit->text().toDouble(&ok);
        if (ok)
            ps->setPhase(phase);

        double duty = m_dutyEdit->text().toDouble(&ok);
        if (ok && duty > 0.0 && duty < 100.0)
            ps->setDutyCycle(duty / 100.0);
    }

    // Zener diode extra field
    if (m_currentComponent->type() == ComponentType::ZenerDiode) {
        auto* zd = static_cast<ZenerDiode*>(m_currentComponent);

        bool ok = false;
        double vz = m_zenerVoltageEdit->text().toDouble(&ok);
        if (ok && vz > 0.0)
            zd->setZenerVoltage(vz);
    }

    // Refresh display to show formatted value
    showProperties(m_currentComponent);

    emit propertyChanged();
}

void PropertyPanel::setSimulationResult(const SimulationResult& result)
{
    m_simResult = result;
    m_hasSimResult = result.success;
    updateSimulationStats();
}

void PropertyPanel::setSimulationConfig(const SimulationConfig& config)
{
    m_simConfig = config;
    updateSimulationStats();
}

void PropertyPanel::setCircuit(Circuit* circuit)
{
    m_circuit = circuit;
}

void PropertyPanel::clearSimulationResult()
{
    m_simResult = SimulationResult();
    m_hasSimResult = false;
    m_statsFrame->setVisible(false);
}

void PropertyPanel::updateSimulationStats()
{
    if (!m_hasSimResult || !m_currentComponent) {
        m_statsFrame->setVisible(false);
        return;
    }

    int compId = m_currentComponent->id();
    auto it = m_simResult.branchResults.find(compId);
    if (it == m_simResult.branchResults.end()) {
        m_statsFrame->setVisible(false);
        return;
    }

    const BranchResult& br = it->second;

    if (br.isMixed) {
        // Mixed analysis: show DC and AC breakdown
        m_statsVoltage->setText(tr("Voltage: %1").arg(
            formatEngineering(br.voltageDrop, "V")));
        m_statsCurrent->setText(tr("Current: %1").arg(
            formatEngineering(br.current, "A")));
        m_statsPower->setText(tr("Power: %1").arg(
            formatEngineering(br.power, "W")));
        m_statsDCLabel->setText(tr("DC: %1 / %2").arg(
            formatEngineering(br.dcVoltage, "V"),
            formatEngineering(br.dcCurrent, "A")));
        m_statsACLabel->setText(tr("AC: %1 / %2").arg(
            formatEngineering(br.acVoltage, "V"),
            formatEngineering(br.acCurrent, "A")));
        m_statsDCLabel->setVisible(true);
        m_statsACLabel->setVisible(true);
    } else {
        m_statsVoltage->setText(tr("Voltage: %1").arg(
            formatEngineering(br.voltageDrop, "V")));
        m_statsCurrent->setText(tr("Current: %1").arg(
            formatEngineering(br.current, "A")));
        m_statsPower->setText(tr("Power: %1").arg(
            formatEngineering(br.power, "W")));
        m_statsDCLabel->setVisible(false);
        m_statsACLabel->setVisible(false);
    }

    // LC circuit analysis (for inductors and capacitors)
    bool showLC = false;
    if (m_circuit && (m_currentComponent->type() == ComponentType::Inductor
                   || m_currentComponent->type() == ComponentType::Capacitor)) {
        double freq = m_simConfig.acFrequency;
        if (freq > 0) {
            double omega = 2.0 * M_PI * freq;

            // Scan circuit for total L and C
            double totalL = 0.0, totalC = 0.0;
            for (auto& [cid, c] : m_circuit->components()) {
                if (c->type() == ComponentType::Inductor) totalL += c->value();
                if (c->type() == ComponentType::Capacitor) totalC += c->value();
            }

            double xL = omega * totalL;
            double xC = (totalC > 0) ? 1.0 / (omega * totalC) : 0.0;
            double zNet = std::abs(xL - xC);

            m_statsReactance->setText(tr("X\u2097=%1  X\u1d04=%2")
                .arg(formatEngineering(xL, "\xCE\xA9"),
                     formatEngineering(xC, "\xCE\xA9")));

            m_statsNetImpedance->setText(tr("Net Z: %1")
                .arg(formatEngineering(zNet, "\xCE\xA9")));

            if (totalL > 0 && totalC > 0) {
                double fRes = 1.0 / (2.0 * M_PI * std::sqrt(totalL * totalC));
                m_statsResonantFreq->setText(tr("f\u2080: %1")
                    .arg(formatEngineering(fRes, "Hz")));
            } else {
                m_statsResonantFreq->setText(tr("f\u2080: N/A"));
            }

            // Find AC source peak voltage for I_peak
            double vPeak = 0.0;
            for (auto& [cid, c] : m_circuit->components()) {
                if (c->type() == ComponentType::ACSource) {
                    vPeak = c->value();
                    break;
                }
            }
            double iPeak = (zNet > 1e-12) ? vPeak / zNet : 0.0;
            m_statsPeakCurrent->setText(tr("I_peak: %1")
                .arg(formatEngineering(iPeak, "A")));

            showLC = true;
        }
    }
    m_statsReactance->setVisible(showLC);
    m_statsNetImpedance->setVisible(showLC);
    m_statsResonantFreq->setVisible(showLC);
    m_statsPeakCurrent->setVisible(showLC);

    m_statsFrame->setVisible(true);
}
