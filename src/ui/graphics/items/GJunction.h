#pragma once

#include "../GraphicComponent.h"

class Junction;

class GJunction : public GraphicComponent
{
public:
    explicit GJunction(Junction* junction);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
