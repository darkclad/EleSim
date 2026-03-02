#pragma once

#include "Component.h"
#include "Node.h"
#include "Wire.h"

#include <QString>
#include <QJsonObject>
#include <map>
#include <vector>
#include <memory>

class Circuit
{
public:
    Circuit();

    // Component management
    int addComponent(std::unique_ptr<Component> comp);
    void removeComponent(int id);
    Component* component(int id) const;
    const std::map<int, std::unique_ptr<Component>>& components() const { return m_components; }

    // Wire management
    int addWire(int comp1, int pin1, int comp2, int pin2);
    void removeWire(int wireId);
    const std::vector<std::unique_ptr<Wire>>& wires() const { return m_wires; }

    // Netlist extraction (union-find)
    int buildNetlist();
    const std::vector<std::unique_ptr<Node>>& nodes() const { return m_nodes; }
    int groundNodeId() const { return m_groundNodeId; }

    // Counts
    int voltageSourceCount() const;

    // Validation
    bool isValid(QString& errorMsg) const;

    // Auto-naming
    QString generateName(ComponentType type);

    // Serialization
    QJsonObject toJson() const;
    static std::unique_ptr<Circuit> fromJson(const QJsonObject& obj);

    // Clear
    void clear();

private:
    std::map<int, std::unique_ptr<Component>> m_components;
    std::vector<std::unique_ptr<Wire>> m_wires;
    std::vector<std::unique_ptr<Node>> m_nodes;
    int m_groundNodeId = 0;

    // Auto-naming counters
    std::map<ComponentType, int> m_nameCounters;
};
