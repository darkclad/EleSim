#pragma once

#include "../GraphicComponent.h"

class ZenerDiode;

class GZenerDiode : public GraphicComponent
{
public:
    explicit GZenerDiode(ZenerDiode* zener);

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;
};
