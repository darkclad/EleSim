#pragma once

#include "../GraphicComponent.h"

class Switch3Way;

class GSwitch3Way : public GraphicComponent
{
public:
    explicit GSwitch3Way(Switch3Way* sw);

    void updateSwitchVisual();

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
