#pragma once

#include "../GraphicComponent.h"

class PulseSource;

class GPulseSource : public GraphicComponent
{
public:
    explicit GPulseSource(PulseSource* source);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
