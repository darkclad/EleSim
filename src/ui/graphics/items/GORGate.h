#pragma once

#include "../GraphicComponent.h"

class ORGate;

class GORGate : public GraphicComponent
{
public:
    explicit GORGate(ORGate* gate);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
