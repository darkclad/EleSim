#pragma once

#include "../GraphicComponent.h"

class XORGate;

class GXORGate : public GraphicComponent
{
public:
    explicit GXORGate(XORGate* gate);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
