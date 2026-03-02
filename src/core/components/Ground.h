#pragma once

#include "../Component.h"

class Ground : public Component
{
public:
    Ground();

    QString valueString() const override { return QString(); }
    QString valueUnit() const override { return QString(); }

    void stampDC(Eigen::MatrixXd& G, Eigen::VectorXd& rhs, int size) const override;
};
