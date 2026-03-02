#pragma once

#include <vector>
#include <utility>

class Node
{
public:
    explicit Node(int id);

    int id() const { return m_id; }

    double voltage() const { return m_voltage; }
    void setVoltage(double v) { m_voltage = v; }

    void addPin(int componentId, int pinIndex);
    const std::vector<std::pair<int,int>>& connectedPins() const { return m_connectedPins; }

private:
    int m_id;
    double m_voltage = 0.0;
    std::vector<std::pair<int,int>> m_connectedPins; // (componentId, pinIndex)
};
