#include "Circuit.h"
#include "components/Resistor.h"
#include "components/Capacitor.h"
#include "components/DCSource.h"
#include "components/ACSource.h"
#include "components/PulseSource.h"
#include "components/DCCurrentSource.h"
#include "components/Lamp.h"
#include "components/Junction.h"
#include "components/Switch.h"
#include "components/Switch3Way.h"
#include "components/Switch4Way.h"
#include "components/Inductor.h"
#include "components/Diode.h"
#include "components/ZenerDiode.h"
#include "components/NMosfet.h"
#include "components/PMosfet.h"
#include "components/NOTGate.h"
#include "components/ANDGate.h"
#include "components/ORGate.h"
#include "components/XORGate.h"
#include "components/Ground.h"

#include <QJsonArray>
#include <QJsonObject>
#include <map>
#include <set>

Circuit::Circuit()
{
}

int Circuit::addComponent(std::unique_ptr<Component> comp)
{
    int id = comp->id();
    if (comp->name().isEmpty()) {
        comp->setName(generateName(comp->type()));
    }
    m_components[id] = std::move(comp);
    return id;
}

void Circuit::removeComponent(int id)
{
    // Remove all wires connected to this component
    m_wires.erase(
        std::remove_if(m_wires.begin(), m_wires.end(),
            [id](const std::unique_ptr<Wire>& w) {
                return w->from().componentId == id || w->to().componentId == id;
            }),
        m_wires.end());

    m_components.erase(id);
}

Component* Circuit::component(int id) const
{
    auto it = m_components.find(id);
    return it != m_components.end() ? it->second.get() : nullptr;
}

int Circuit::addWire(int comp1, int pin1, int comp2, int pin2)
{
    auto wire = std::make_unique<Wire>(comp1, pin1, comp2, pin2);
    int id = wire->id();
    m_wires.push_back(std::move(wire));
    return id;
}

void Circuit::removeWire(int wireId)
{
    m_wires.erase(
        std::remove_if(m_wires.begin(), m_wires.end(),
            [wireId](const std::unique_ptr<Wire>& w) {
                return w->id() == wireId;
            }),
        m_wires.end());
}

int Circuit::buildNetlist()
{
    // Union-Find for merging pins into nodes
    // Key: flat index = componentId * 100 + pinIndex
    std::map<int, int> parent;

    auto makeKey = [](int compId, int pinIdx) {
        return compId * 100 + pinIdx;
    };

    // Initialize: each pin is its own parent
    for (auto& [id, comp] : m_components) {
        for (int i = 0; i < comp->pinCount(); ++i) {
            int key = makeKey(id, i);
            parent[key] = key;
        }
    }

    // Find with path compression
    std::function<int(int)> find = [&](int x) -> int {
        if (parent[x] != x)
            parent[x] = find(parent[x]);
        return parent[x];
    };

    // Union
    auto unite = [&](int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra != rb)
            parent[ra] = rb;
    };

    // Merge connected pins via wires
    for (auto& wire : m_wires) {
        int keyFrom = makeKey(wire->from().componentId, wire->from().pinIndex);
        int keyTo = makeKey(wire->to().componentId, wire->to().pinIndex);
        unite(keyFrom, keyTo);
    }

    // Merge all pins of Junction components into the same node
    for (auto& [id, comp] : m_components) {
        if (comp->type() == ComponentType::Junction && comp->pinCount() > 1) {
            int keyFirst = makeKey(id, 0);
            for (int i = 1; i < comp->pinCount(); ++i) {
                unite(keyFirst, makeKey(id, i));
            }
        }
    }

    // Determine which pin groups are actually connected via wires
    // (pins not in any wire get nodeId = -1 to avoid floating nodes)
    std::set<int> connectedRoots;
    for (auto& wire : m_wires) {
        connectedRoots.insert(find(makeKey(wire->from().componentId, wire->from().pinIndex)));
        connectedRoots.insert(find(makeKey(wire->to().componentId, wire->to().pinIndex)));
    }

    // Collect equivalence classes into nodes
    std::map<int, int> rootToNodeId;
    m_nodes.clear();
    int nextNodeId = 1; // 0 reserved for ground

    // Find ground node:
    // Priority 1: Ground component (explicit ground reference)
    // Priority 2: Negative terminal of first voltage source (fallback)
    m_groundNodeId = -1;

    // Check for explicit Ground component first
    for (auto& [id, comp] : m_components) {
        if (comp->type() == ComponentType::Ground) {
            int groundKey = makeKey(id, 0);
            int root = find(groundKey);
            rootToNodeId[root] = 0;
            m_groundNodeId = 0;
            connectedRoots.insert(root); // ensure it's treated as connected
        }
    }

    // Fallback: if no Ground component, use negative terminal of first voltage source
    if (m_groundNodeId < 0) {
        for (auto& [id, comp] : m_components) {
            if (comp->isVoltageSource()) {
                int groundKey = makeKey(id, 1); // pin 1 = negative terminal
                int root = find(groundKey);
                rootToNodeId[root] = 0;
                m_groundNodeId = 0;
                break;
            }
        }
    }

    // Assign node IDs to connected pins only
    for (auto& [id, comp] : m_components) {
        for (int i = 0; i < comp->pinCount(); ++i) {
            int key = makeKey(id, i);
            int root = find(key);

            // Pin not connected to any wire — mark as unconnected
            if (connectedRoots.find(root) == connectedRoots.end()) {
                comp->pin(i).nodeId = -1;
                continue;
            }

            if (rootToNodeId.find(root) == rootToNodeId.end()) {
                rootToNodeId[root] = nextNodeId++;
            }
            comp->pin(i).nodeId = rootToNodeId[root];
        }
    }

    // Create Node objects
    int totalNodes = nextNodeId; // includes ground at 0
    for (int i = 0; i < totalNodes; ++i) {
        m_nodes.push_back(std::make_unique<Node>(i));
    }

    // Populate node connections
    for (auto& [id, comp] : m_components) {
        for (int i = 0; i < comp->pinCount(); ++i) {
            int nodeId = comp->pin(i).nodeId;
            if (nodeId >= 0 && nodeId < static_cast<int>(m_nodes.size())) {
                m_nodes[nodeId]->addPin(id, i);
            }
        }
    }

    // Set wire node IDs
    for (auto& wire : m_wires) {
        int keyFrom = makeKey(wire->from().componentId, wire->from().pinIndex);
        int root = find(keyFrom);
        wire->setNodeId(rootToNodeId[root]);
    }

    return totalNodes - 1; // number of non-ground nodes
}

int Circuit::voltageSourceCount() const
{
    int count = 0;
    for (auto& [id, comp] : m_components) {
        if (comp->isVoltageSource())
            ++count;
    }
    return count;
}

bool Circuit::isValid(QString& errorMsg) const
{
    if (m_components.empty()) {
        errorMsg = "Circuit is empty";
        return false;
    }

    // Check for at least one source (voltage or current)
    bool hasSource = false;
    for (auto& [id, comp] : m_components) {
        if (comp->isSource()) {
            hasSource = true;
            break;
        }
    }
    if (!hasSource) {
        errorMsg = "Circuit has no source";
        return false;
    }

    return true;
}

QString Circuit::generateName(ComponentType type)
{
    QString prefix;
    switch (type) {
        case ComponentType::Resistor:  prefix = "R"; break;
        case ComponentType::Capacitor: prefix = "C"; break;
        case ComponentType::DCSource:  prefix = "V"; break;
        case ComponentType::ACSource:  prefix = "V"; break;
        case ComponentType::Lamp:      prefix = "L"; break;
        case ComponentType::Junction:  prefix = "J"; break;
        case ComponentType::Switch2Way: prefix = "SW"; break;
        case ComponentType::Switch3Way: prefix = "SW"; break;
        case ComponentType::Switch4Way: prefix = "SW"; break;
        case ComponentType::Inductor:  prefix = "L"; break;
        case ComponentType::Diode:     prefix = "D"; break;
        case ComponentType::Ground:    prefix = "GND"; break;
        case ComponentType::PulseSource: prefix = "SQ"; break;
        case ComponentType::DCCurrentSource: prefix = "I"; break;
        case ComponentType::ZenerDiode: prefix = "ZD"; break;
        case ComponentType::NMosfet:  prefix = "M"; break;
        case ComponentType::PMosfet:  prefix = "M"; break;
        case ComponentType::NOTGate:  prefix = "U"; break;
        case ComponentType::ANDGate:  prefix = "U"; break;
        case ComponentType::ORGate:   prefix = "U"; break;
        case ComponentType::XORGate:  prefix = "U"; break;
    }
    int num = ++m_nameCounters[type];
    return prefix + QString::number(num);
}

void Circuit::clear()
{
    m_components.clear();
    m_wires.clear();
    m_nodes.clear();
    m_nameCounters.clear();
    m_groundNodeId = 0;
}

// --- Serialization ---

static QString componentTypeToString(ComponentType type)
{
    switch (type) {
        case ComponentType::Resistor:  return "Resistor";
        case ComponentType::Capacitor: return "Capacitor";
        case ComponentType::DCSource:  return "DCSource";
        case ComponentType::ACSource:  return "ACSource";
        case ComponentType::Lamp:      return "Lamp";
        case ComponentType::Junction:  return "Junction";
        case ComponentType::Switch2Way: return "Switch2Way";
        case ComponentType::Switch3Way: return "Switch3Way";
        case ComponentType::Switch4Way: return "Switch4Way";
        case ComponentType::Inductor:  return "Inductor";
        case ComponentType::Diode:     return "Diode";
        case ComponentType::Ground:    return "Ground";
        case ComponentType::PulseSource: return "PulseSource";
        case ComponentType::DCCurrentSource: return "DCCurrentSource";
        case ComponentType::ZenerDiode: return "ZenerDiode";
        case ComponentType::NMosfet:  return "NMosfet";
        case ComponentType::PMosfet:  return "PMosfet";
        case ComponentType::NOTGate:  return "NOTGate";
        case ComponentType::ANDGate:  return "ANDGate";
        case ComponentType::ORGate:   return "ORGate";
        case ComponentType::XORGate:  return "XORGate";
    }
    return "Unknown";
}

static ComponentType stringToComponentType(const QString& s)
{
    if (s == "Resistor")  return ComponentType::Resistor;
    if (s == "Capacitor") return ComponentType::Capacitor;
    if (s == "DCSource")  return ComponentType::DCSource;
    if (s == "ACSource")  return ComponentType::ACSource;
    if (s == "Lamp")      return ComponentType::Lamp;
    if (s == "Junction")  return ComponentType::Junction;
    if (s == "Switch2Way") return ComponentType::Switch2Way;
    if (s == "Switch3Way") return ComponentType::Switch3Way;
    if (s == "Switch4Way") return ComponentType::Switch4Way;
    if (s == "Inductor")  return ComponentType::Inductor;
    if (s == "Diode")     return ComponentType::Diode;
    if (s == "Ground")    return ComponentType::Ground;
    if (s == "PulseSource" || s == "SquareWaveSource") return ComponentType::PulseSource;
    if (s == "DCCurrentSource") return ComponentType::DCCurrentSource;
    if (s == "ZenerDiode") return ComponentType::ZenerDiode;
    if (s == "NMosfet")  return ComponentType::NMosfet;
    if (s == "PMosfet")  return ComponentType::PMosfet;
    if (s == "NOTGate")  return ComponentType::NOTGate;
    if (s == "ANDGate")  return ComponentType::ANDGate;
    if (s == "ORGate")   return ComponentType::ORGate;
    if (s == "XORGate")  return ComponentType::XORGate;
    return ComponentType::Resistor; // fallback
}

QJsonObject Circuit::toJson() const
{
    QJsonObject root;
    root["version"] = 1;

    QJsonArray compsArray;
    for (auto& [id, comp] : m_components) {
        QJsonObject obj;
        obj["id"] = id;
        obj["type"] = componentTypeToString(comp->type());
        obj["name"] = comp->name();
        obj["value"] = comp->value();
        obj["x"] = comp->position().x();
        obj["y"] = comp->position().y();
        obj["rotation"] = comp->rotation();

        if (comp->type() == ComponentType::ACSource) {
            auto* ac = static_cast<ACSource*>(comp.get());
            obj["frequency"] = ac->frequency();
            obj["phase"] = ac->phase();
        }
        if (comp->type() == ComponentType::PulseSource) {
            auto* sq = static_cast<PulseSource*>(comp.get());
            obj["frequency"] = sq->frequency();
            obj["phase"] = sq->phase();
            obj["dutyCycle"] = sq->dutyCycle();
        }

        if (comp->type() == ComponentType::ZenerDiode) {
            auto* zd = static_cast<ZenerDiode*>(comp.get());
            obj["zenerVoltage"] = zd->zenerVoltage();
        }

        if (comp->type() == ComponentType::NMosfet) {
            auto* m = static_cast<NMosfet*>(comp.get());
            obj["thresholdVoltage"] = m->thresholdVoltage();
        }
        if (comp->type() == ComponentType::PMosfet) {
            auto* m = static_cast<PMosfet*>(comp.get());
            obj["thresholdVoltage"] = m->thresholdVoltage();
        }

        if (comp->type() == ComponentType::NOTGate) {
            auto* g = static_cast<NOTGate*>(comp.get());
            obj["thresholdVoltage"] = g->thresholdVoltage();
        }
        if (comp->type() == ComponentType::ANDGate) {
            auto* g = static_cast<ANDGate*>(comp.get());
            obj["thresholdVoltage"] = g->thresholdVoltage();
        }
        if (comp->type() == ComponentType::ORGate) {
            auto* g = static_cast<ORGate*>(comp.get());
            obj["thresholdVoltage"] = g->thresholdVoltage();
        }
        if (comp->type() == ComponentType::XORGate) {
            auto* g = static_cast<XORGate*>(comp.get());
            obj["thresholdVoltage"] = g->thresholdVoltage();
        }

        if (comp->type() == ComponentType::Switch2Way) {
            auto* sw = static_cast<Switch*>(comp.get());
            obj["closed"] = sw->isClosed();
        }
        if (comp->type() == ComponentType::Switch3Way) {
            auto* sw = static_cast<Switch3Way*>(comp.get());
            obj["position"] = sw->activePosition();
        }
        if (comp->type() == ComponentType::Switch4Way) {
            auto* sw = static_cast<Switch4Way*>(comp.get());
            obj["crossed"] = sw->isCrossed();
        }

        compsArray.append(obj);
    }
    root["components"] = compsArray;

    QJsonArray wiresArray;
    for (auto& wire : m_wires) {
        QJsonObject obj;
        obj["comp1"] = wire->from().componentId;
        obj["pin1"] = wire->from().pinIndex;
        obj["comp2"] = wire->to().componentId;
        obj["pin2"] = wire->to().pinIndex;

        if (!wire->waypoints().isEmpty()) {
            QJsonArray wpArray;
            for (const auto& wp : wire->waypoints()) {
                QJsonObject wpObj;
                wpObj["x"] = wp.x();
                wpObj["y"] = wp.y();
                wpArray.append(wpObj);
            }
            obj["waypoints"] = wpArray;
        }

        wiresArray.append(obj);
    }
    root["wires"] = wiresArray;

    return root;
}

std::unique_ptr<Circuit> Circuit::fromJson(const QJsonObject& root)
{
    auto circuit = std::make_unique<Circuit>();

    // Map old IDs to new IDs
    std::map<int, int> idMap;

    QJsonArray compsArray = root["components"].toArray();
    for (const auto& val : compsArray) {
        QJsonObject obj = val.toObject();
        int oldId = obj["id"].toInt();
        ComponentType type = stringToComponentType(obj["type"].toString());
        double value = obj["value"].toDouble();

        std::unique_ptr<Component> comp;
        switch (type) {
            case ComponentType::Resistor:
                comp = std::make_unique<Resistor>(value);
                break;
            case ComponentType::Capacitor:
                comp = std::make_unique<Capacitor>(value);
                break;
            case ComponentType::DCSource:
                comp = std::make_unique<DCSource>(value);
                break;
            case ComponentType::ACSource: {
                double freq = obj["frequency"].toDouble(50.0);
                double phase = obj["phase"].toDouble(0.0);
                auto ac = std::make_unique<ACSource>(value, freq, phase);
                comp = std::move(ac);
                break;
            }
            case ComponentType::Lamp:
                comp = std::make_unique<Lamp>(value);
                break;
            case ComponentType::Junction:
                comp = std::make_unique<Junction>();
                break;
            case ComponentType::Switch2Way: {
                auto sw = std::make_unique<Switch>(value);
                sw->setClosed(obj["closed"].toBool(false));
                comp = std::move(sw);
                break;
            }
            case ComponentType::Switch3Way: {
                auto sw = std::make_unique<Switch3Way>(value);
                sw->setActivePosition(obj["position"].toInt(0));
                comp = std::move(sw);
                break;
            }
            case ComponentType::Switch4Way: {
                auto sw = std::make_unique<Switch4Way>(value);
                sw->setCrossed(obj["crossed"].toBool(false));
                comp = std::move(sw);
                break;
            }
            case ComponentType::Inductor:
                comp = std::make_unique<Inductor>(value);
                break;
            case ComponentType::Diode:
                comp = std::make_unique<Diode>(value);
                break;
            case ComponentType::Ground:
                comp = std::make_unique<Ground>();
                break;
            case ComponentType::PulseSource: {
                double freq = obj["frequency"].toDouble(1000.0);
                double phase = obj["phase"].toDouble(0.0);
                double duty = obj["dutyCycle"].toDouble(0.5);
                comp = std::make_unique<PulseSource>(value, freq, phase, duty);
                break;
            }
            case ComponentType::DCCurrentSource:
                comp = std::make_unique<DCCurrentSource>(value);
                break;
            case ComponentType::ZenerDiode: {
                double vz = obj["zenerVoltage"].toDouble(5.1);
                comp = std::make_unique<ZenerDiode>(value, vz);
                break;
            }
            case ComponentType::NMosfet: {
                double vth = obj["thresholdVoltage"].toDouble(2.0);
                comp = std::make_unique<NMosfet>(vth);
                break;
            }
            case ComponentType::PMosfet: {
                double vth = obj["thresholdVoltage"].toDouble(2.0);
                comp = std::make_unique<PMosfet>(vth);
                break;
            }
            case ComponentType::NOTGate: {
                double vth = obj["thresholdVoltage"].toDouble(2.5);
                comp = std::make_unique<NOTGate>(vth);
                break;
            }
            case ComponentType::ANDGate: {
                double vth = obj["thresholdVoltage"].toDouble(2.5);
                comp = std::make_unique<ANDGate>(vth);
                break;
            }
            case ComponentType::ORGate: {
                double vth = obj["thresholdVoltage"].toDouble(2.5);
                comp = std::make_unique<ORGate>(vth);
                break;
            }
            case ComponentType::XORGate: {
                double vth = obj["thresholdVoltage"].toDouble(2.5);
                comp = std::make_unique<XORGate>(vth);
                break;
            }
        }

        comp->setName(obj["name"].toString());
        comp->setPosition(QPointF(obj["x"].toDouble(), obj["y"].toDouble()));
        comp->setRotation(obj["rotation"].toDouble());

        int newId = circuit->addComponent(std::move(comp));
        idMap[oldId] = newId;
    }

    QJsonArray wiresArray = root["wires"].toArray();
    for (const auto& val : wiresArray) {
        QJsonObject obj = val.toObject();
        int oldComp1 = obj["comp1"].toInt();
        int oldComp2 = obj["comp2"].toInt();
        int pin1 = obj["pin1"].toInt();
        int pin2 = obj["pin2"].toInt();

        // Map old IDs to new IDs
        auto it1 = idMap.find(oldComp1);
        auto it2 = idMap.find(oldComp2);
        if (it1 != idMap.end() && it2 != idMap.end()) {
            circuit->addWire(it1->second, pin1, it2->second, pin2);

            // Load waypoints
            if (obj.contains("waypoints")) {
                QVector<QPointF> waypoints;
                QJsonArray wpArray = obj["waypoints"].toArray();
                for (const auto& wpVal : wpArray) {
                    QJsonObject wpObj = wpVal.toObject();
                    waypoints.append(QPointF(wpObj["x"].toDouble(), wpObj["y"].toDouble()));
                }
                circuit->wires().back()->setWaypoints(waypoints);
            }
        }
    }

    return circuit;
}
