#include "Component.h"
#include <cmath>

int Component::s_nextId = 1;

Component::Component(ComponentType type, int pinCount, double defaultValue)
    : m_id(s_nextId++)
    , m_type(type)
    , m_value(defaultValue)
{
    m_pins.resize(pinCount);
    for (int i = 0; i < pinCount; ++i) {
        m_pins[i].index = i;
    }
}

void Component::stampAC(Eigen::MatrixXcd& /*Y*/, Eigen::VectorXcd& /*rhs*/,
                        double /*omega*/, int /*size*/) const
{
    // Default: same as DC (real-valued stamp)
    // Subclasses override for frequency-dependent behavior
}

QString formatEngineering(double value, const QString& unit)
{
    struct Prefix {
        double scale;
        const char* symbol;
    };

    static const Prefix prefixes[] = {
        { 1e12,  "T" },
        { 1e9,   "G" },
        { 1e6,   "M" },
        { 1e3,   "k" },
        { 1.0,   ""  },
        { 1e-3,  "m" },
        { 1e-6,  "\xC2\xB5" }, // µ in UTF-8
        { 1e-9,  "n" },
        { 1e-12, "p" },
    };

    double absVal = std::abs(value);
    if (absVal == 0.0)
        return "0" + unit;

    for (const auto& p : prefixes) {
        if (absVal >= p.scale) {
            double scaled = value / p.scale;
            // Use up to 3 significant digits
            if (std::abs(scaled) >= 100.0)
                return QString::number(scaled, 'f', 0) + p.symbol + unit;
            else if (std::abs(scaled) >= 10.0)
                return QString::number(scaled, 'f', 1) + p.symbol + unit;
            else
                return QString::number(scaled, 'f', 2) + p.symbol + unit;
        }
    }

    return QString::number(value, 'g', 3) + unit;
}
