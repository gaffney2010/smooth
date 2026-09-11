#pragma once

// Convenience header pulling in every concrete RepresentationBase
// implementation in representation_zoo/ (see representation_base.hpp for
// the interface itself): SparseRepresentation, DynamicMatrixRepresentation,
// RowValuesRepresentation, and ScalarRepresentation. Mirrors plan_zoo.hpp/
// transformation_zoo.hpp.
#include "smooth/representation_zoo/dynamic_matrix_representation.hpp"
#include "smooth/representation_zoo/row_values_representation.hpp"
#include "smooth/representation_zoo/scalar_representation.hpp"
#include "smooth/representation_zoo/sparse_representation.hpp"
