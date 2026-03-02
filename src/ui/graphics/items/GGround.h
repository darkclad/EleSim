#pragma once

#include "../GraphicComponent.h"

class Ground;

class GGround : public GraphicComponent
{
public:
    explicit GGround(Ground* ground);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
