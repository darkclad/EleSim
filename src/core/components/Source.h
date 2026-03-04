#pragma once

#include "../Component.h"

class Source : public Component
{
public:
    using Component::Component; // inherit constructors
    bool isSource() const override { return true; }
};
