#pragma once

#include "../GraphicComponent.h"

class DCCurrentSource;

class GDCCurrentSource : public GraphicComponent
{
public:
    explicit GDCCurrentSource(DCCurrentSource* source);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
