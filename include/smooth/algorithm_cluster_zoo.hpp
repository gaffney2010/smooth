#pragma once

// Convenience header pulling in everything in algorithm_cluster_zoo/:
// TransformationAlgorithmCluster, the generic engine, and the named
// presets built from it or (TernaryFormCluster) composing one --
// MergeCluster, BinaryFormCluster, TernaryFormCluster. AlgorithmCluster
// itself (algorithm_cluster.hpp) is just the shared interface -- see each
// file's own class comment for building a custom cluster directly.
// Mirrors plan_zoo.hpp/transformation_zoo.hpp/representation_zoo.hpp.
#include "smooth/algorithm_cluster_zoo/binary_form_cluster.hpp"
#include "smooth/algorithm_cluster_zoo/merge_cluster.hpp"
#include "smooth/algorithm_cluster_zoo/ternary_form_cluster.hpp"
#include "smooth/algorithm_cluster_zoo/transformation_algorithm_cluster.hpp"
