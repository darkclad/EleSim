#pragma once

#include "../GraphicComponent.h"

class Switch;

class GSwitch : public GraphicComponent
{
public:
    explicit GSwitch(Switch* sw);

    void updateSwitchVisual();

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
