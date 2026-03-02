#pragma once

#include "../GraphicComponent.h"

class Inductor;

class GInductor : public GraphicComponent
{
public:
    explicit GInductor(Inductor* inductor);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
