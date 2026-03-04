#pragma once

#include "../GraphicComponent.h"

class ANDGate;

class GANDGate : public GraphicComponent
{
public:
    explicit GANDGate(ANDGate* gate);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
