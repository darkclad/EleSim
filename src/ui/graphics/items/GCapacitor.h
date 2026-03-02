#pragma once

#include "../GraphicComponent.h"

class Capacitor;

class GCapacitor : public GraphicComponent
{
public:
    explicit GCapacitor(Capacitor* capacitor);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
