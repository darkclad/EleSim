#include "Node.h"

Node::Node(int id)
    : m_id(id)
{
}

void Node::addPin(int componentId, int pinIndex)
{
    m_connectedPins.emplace_back(componentId, pinIndex);
}
