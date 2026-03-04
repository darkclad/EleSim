#pragma once

#include "../GraphicComponent.h"

class NOTGate;

class GNOTGate : public GraphicComponent
{
public:
    explicit GNOTGate(NOTGate* gate);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
