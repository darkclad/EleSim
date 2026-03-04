#pragma once

#include "../GraphicComponent.h"

class PMosfet;

class GPMosfet : public GraphicComponent
{
public:
    explicit GPMosfet(PMosfet* mosfet);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
