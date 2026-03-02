#pragma once

#include <QString>
#include <QPointF>
#include <vector>
#include <Eigen/Dense>

enum class ComponentType {
    Resistor,
    Capacitor,
    DCSource,
    ACSource,
    Lamp,
    Junction,
    Switch2Way,
    Switch3Way,
    Switch4Way,
    Inductor,
    Diode,
    Ground,
    PulseSource,
    DCCurrentSource,
    ZenerDiode
};

struct Pin {
    int index = 0;
    int nodeId = -1; // -1 = unconnected
};

class Component
{
public:
    Component(ComponentType type, int pinCount, double defaultValue);
    virtual ~Component() = default;

    // Identity
    int id() const { return m_id; }
    ComponentType type() const { return m_type; }
    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    // Pins
    int pinCount() const { return static_cast<int>(m_pins.size()); }
    Pin& pin(int index) { return m_pins[index]; }
    const Pin& pin(int index) const { return m_pins[index]; }

    // Value
    double value() const { return m_value; }
    void setValue(double val) { m_value = val; }
    virtual QString valueString() const = 0;
    virtual QString valueUnit() const = 0;

    // MNA stamping
    virtual void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const = 0;
    virtual void stampAC(Eigen::MatrixXcd& Y, Eigen::VectorXcd& rhs,
                         double omega, int size) const;

    // Voltage source branch index (only used by voltage sources)
    int branchIndex() const { return m_branchIndex; }
    void setBranchIndex(int idx) { m_branchIndex = idx; }
    virtual bool isVoltageSource() const { return false; }

    // Position (for serialization and graphic sync)
    QPointF position() const { return m_position; }
    void setPosition(const QPointF& pos) { m_position = pos; }
    double rotation() const { return m_rotation; }
    void setRotation(double degrees) { m_rotation = degrees; }

protected:
    static int s_nextId;

    int m_id;
    ComponentType m_type;
    QString m_name;
    std::vector<Pin> m_pins;
    double m_value;
    int m_branchIndex = -1;
    QPointF m_position;
    double m_rotation = 0.0;
};

// Format a value with SI prefix: 4700.0, "Ω" -> "4.7kΩ"
QString formatEngineering(double value, const QString& unit);
