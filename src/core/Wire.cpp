#include "Wire.h"

int Wire::s_nextId = 1;

Wire::Wire(int comp1, int pin1, int comp2, int pin2)
    : m_id(s_nextId++)
    , m_from{comp1, pin1}
    , m_to{comp2, pin2}
{
}
