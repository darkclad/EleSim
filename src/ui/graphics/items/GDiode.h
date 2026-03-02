#pragma once

#include "../GraphicComponent.h"

class Diode;

class GDiode : public GraphicComponent
{
public:
    explicit GDiode(Diode* diode);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
