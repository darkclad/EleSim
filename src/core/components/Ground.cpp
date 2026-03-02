#include "Ground.h"

Ground::Ground()
    : Component(ComponentType::Ground, 1, 0.0)
{
}

void Ground::stampDC(Eigen::MatrixXd& /*G*/, Eigen::VectorXd& /*rhs*/, int /*size*/) const
{
    // Ground stamps nothing -- its pin is forced to node 0 during netlist building.
}
