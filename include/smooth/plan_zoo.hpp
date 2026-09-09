#pragma once

// Convenience header pulling in every Plan subclass in plan_zoo/: each
// overrides Plan's computation strategy (name()/convertLeaf()/combine())
// to route through a specific RepresentationBase instead of Plan's default
// plain double arithmetic. More are meant to be added here over time as
// this library explores which representation is fastest for what.
#include "smooth/plan_zoo/matrix_plan.hpp"
#include "smooth/plan_zoo/row_values_plan.hpp"
#include "smooth/plan_zoo/sparse_plan.hpp"
