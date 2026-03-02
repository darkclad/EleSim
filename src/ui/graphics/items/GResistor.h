#pragma once

#include "../GraphicComponent.h"

class Resistor;

class GResistor : public GraphicComponent
{
public:
    explicit GResistor(Resistor* resistor);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
