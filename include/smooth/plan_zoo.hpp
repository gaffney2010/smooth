#pragma once

// Convenience header pulling in every Plan subclass in plan_zoo/: each
// implements Plan's computation strategy (name()/buildBlueprint()) to
// route through a specific RepresentationBase. Plan itself is an
// interface -- see plan.hpp, which also defines DefaultPlan, the
// ScalarRepresentation-based strategy most code reaches for by default
// (kept alongside Plan rather than living here, since Plan::scalar()
// already depends on ScalarRepresentation directly). More plan_zoo/
// subclasses are meant to be added here over time as this library
// explores which representation -- and which transformation clusters --
// are fastest for what.
#include "smooth/plan_zoo/matrix_plan.hpp"
#include "smooth/plan_zoo/merging_sparse_plan.hpp"
#include "smooth/plan_zoo/row_values_plan.hpp"
#include "smooth/plan_zoo/sparse_plan.hpp"
