#pragma once

#include "../GraphicComponent.h"

class NMosfet;

class GNMosfet : public GraphicComponent
{
public:
    explicit GNMosfet(NMosfet* mosfet);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
