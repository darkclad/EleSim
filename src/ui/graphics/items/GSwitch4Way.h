#pragma once

#include "../GraphicComponent.h"

class Switch4Way;

class GSwitch4Way : public GraphicComponent
{
public:
    explicit GSwitch4Way(Switch4Way* sw);

    void updateSwitchVisual();

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
