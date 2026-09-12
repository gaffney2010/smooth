#pragma once

// Convenience header pulling in every Plan subclass in plan_zoo/: each
// routes Plan's strategy (name()/buildBlueprint()) through a specific
// RepresentationBase. Plan itself is an interface -- see plan.hpp, which
// also defines DefaultPlan, the ScalarRepresentation-based strategy most
// code reaches for by default.
#include "smooth/plan_zoo/matrix_plan.hpp"
#include "smooth/plan_zoo/merging_sparse_plan.hpp"
#include "smooth/plan_zoo/representation_aware_plan.hpp"
#include "smooth/plan_zoo/row_values_plan.hpp"
#include "smooth/plan_zoo/size_adaptive_sparse_plan.hpp"
#include "smooth/plan_zoo/sparse_plan.hpp"
#include "smooth/plan_zoo/ternary_form_sparse_plan.hpp"
