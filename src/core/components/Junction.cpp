#include "Junction.h"

Junction::Junction()
    : Component(ComponentType::Junction, 3, 0.0)
{
}

void Junction::stampDC(Eigen::MatrixXd& /*G*/, Eigen::VectorXd& /*rhs*/, int /*size*/) const
{
    // Junction stamps nothing -- it's purely a connectivity node.
    // All pins will be merged into the same electrical node via wires.
}
