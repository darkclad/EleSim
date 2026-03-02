#pragma once

#include "../GraphicComponent.h"

class Lamp;

class GLamp : public GraphicComponent
{
public:
    explicit GLamp(Lamp* lamp);

    // Set current flowing through the lamp (amps). Controls brightness.
    void setCurrent(double amps);
    void clearCurrent();

protected:
    void drawSymbol(QPainter* painter) override;
    void setupPins() override;

private:
    double m_current = 0.0;
    bool m_hasSimData = false;
};
