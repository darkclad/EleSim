#pragma once

#include "../GraphicComponent.h"

class ACSource;

class GACSource : public GraphicComponent
{
public:
    explicit GACSource(ACSource* source);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
