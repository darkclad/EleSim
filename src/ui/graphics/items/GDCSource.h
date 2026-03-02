#pragma once

#include "../GraphicComponent.h"

class DCSource;

class GDCSource : public GraphicComponent
{
public:
    explicit GDCSource(DCSource* source);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
