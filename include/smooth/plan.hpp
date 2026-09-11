#pragma once

#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "smooth/dynamic_matrix_representation.hpp"
#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/row_values_representation.hpp"
#include "smooth/scalar_representation.hpp"
#include "smooth/smooth_number_base.hpp"
#include "smooth/sparse_representation.hpp"

namespace smooth {

// A fluent builder for an arithmetic expression over 3-smooth numbers.
// Building it produces a **declaration** -- a pure, representation-agnostic
// record of exactly what was asked for (a tree of scalar/number leaves
// combined by add/multiply), and nothing else; building never computes
// anything, and never touches a RepresentationBase. Compiling (the first
// call to plan()/calculate()) turns that declaration into a **blueprint**:
// the same tree, but with explicit Ensure(target) steps spliced in
// wherever a leaf needs to become a specific representation. The blueprint
// is what's actually executed -- and printed by plan() -- so every
// conversion this Plan performs shows up as a real, inspectable step, not
// something hidden inside a virtual call:
//
//   smooth::SparsePlan().scalar(1).plus().scalar(2).plan();
//   // add
//   // ├─ ensure(sparse)
//   // │  └─ scalar: 1
//   // └─ ensure(sparse)
//   //    └─ scalar: 2
//   // = 3
//
// A concrete Plan's *entire* strategy is one method: buildBlueprint(),
// which maps a declaration node to a blueprint node. Every plan_zoo
// strategy's override is one line, built on the shared
// wrapLeavesWithEnsure() helper below (see DefaultPlan in this file, and
// plan_zoo/ for the others) -- there is no convertLeaf()/convertNumberLeaf()/
// targetRepresentation() split to keep in sync; a strategy either wraps
// leaves in Ensure(some representation), or (for a future strategy that
// wants to do something else -- pick a representation per-node, insert some
// other kind of step, ...) writes its own buildBlueprint() from scratch.
// Immediately after buildBlueprint() runs, validateBlueprint() (below)
// confirms that stripping every Ensure node back out of the blueprint
// yields the declaration's exact shape and leaf contents again -- so a
// buildBlueprint() override can only ever *decorate* what's being
// computed, never change it.
//
//   double result = smooth::DefaultPlan()
//       .scalar(3)
//       .times()
//       .left()
//         .scalar(4)
//         .plus()
//         .scalar(2)
//       .right()
//       .calculate();  // 3 * (4 + 2) = 18
//
// - scalar(x): a leaf. Throws std::invalid_argument for a negative x:
//   every RepresentationBase is magnitude-only, and a negative value would
//   otherwise infinite-loop the first time something tries to decompose it
//   into bits (see ScalarRepresentation::forEachSet()/
//   representation_base.hpp's decomposeColumnValue()) -- so this is the
//   one place a raw literal enters the system, and the one place that gets
//   checked. Nothing is built yet -- just the raw value, recorded in the
//   declaration.
// - number(n): a leaf holding an existing SmoothNumberBase-derived
//   object's value, snapshotted immediately (via scalar(n.value())) -- so
//   it's subject to the same non-negative restriction. Templated
//   specifically so T::value() is resolved at compile time against T's
//   *own* type -- required for signed types, whose value() intentionally
//   hides (isn't a virtual override of) SmoothNumberBase::value(); calling
//   it through a SmoothNumberBase& would silently read the unsigned
//   magnitude instead of throwing for a negative value.
// - numberVia(n): keeps a live reference to n (which must outlive
//   calculate()/plan()) instead. Compiling always wraps a numberVia() leaf
//   in Ensure(target) too (see wrapLeavesWithEnsure()), and executing that
//   Ensure step calls n's own SmoothNumberBase::representationAs(target)
//   directly -- a genuine ensure()-driven conversion through n's own
//   internal representation cache, not a value-then-rebuild round trip --
//   so n's own Metrics (if it has any) sees the resulting
//   convert_<canonical>_to_<target> counter.
// - plus() / times(): wraps whatever's been built so far at the current
//   nesting level into a new operator node (as its left side), and expects
//   the next thing you build to become its right side.
// - left() / right(): open and close a nested group, the way `(` and `)`
//   do -- left() always starts a fresh, independent sub-expression;
//   right() always finishes the most recently opened one and plugs its
//   completed value into whichever slot is open one level up (which slot
//   that is -- a pending operator's right side, or the top-level result --
//   is whatever's actually open there; left()/right() name the bracket
//   pair, not a side of the parent).
// - calculate(): the computed result, as a double -- the final blueprint
//   step's representation's own value().
// - plan(os): prints the compiled blueprint as a tree (see above).
//
// name() identifies which strategy is in use ("scalar" for DefaultPlan;
// "sparse"/"matrix"/"row_values" for the plan_zoo/ subclasses). It drives
// the convert_to_<name> metrics counter (incremented once per Ensure step),
// so overriding it alone already shows up distinctly in the metrics.
//
// Like every concrete SmoothNumberBase-derived type, a Plan optionally
// takes a shared Metrics at construction (see metrics.hpp). A Metrics
// shared between a Plan and the numbers that feed it (via number() or
// numberVia() -- the latter via setMetricsPtr(), since a numberVia()
// argument isn't constructed by the Plan) tallies both under the same
// counters: convert_to_<name()>/add/multiply from the Plan itself,
// carries/bit_operations/scalar_operations/bit_iterations from whatever
// representation-level work executing the blueprint actually does (an
// Ensure step re-points its result at the Plan's own Metrics via
// RepresentationBase::setMetricsPtr(), so this is true even for a
// numberVia() leaf's forced conversion, not just scalar()/number() ones),
// and, for numberVia() specifically, the fed-in number's own
// convert_<canonical>_to_<target> counter too.
class Plan {
public:
    explicit Plan(std::shared_ptr<Metrics> metrics = nullptr) : metrics_(std::move(metrics)) {
        stack_.emplace_back();
    }

    virtual ~Plan() = default;

    virtual std::string name() const = 0;

    bool hasMetrics() const { return static_cast<bool>(metrics_); }
    const std::shared_ptr<Metrics>& metricsPtr() const { return metrics_; }

    // A single double overload (rather than separate long long/double
    // overloads, as setValue() has elsewhere in this library): both would
    // do exactly the same thing here, so a second overload would only add
    // the risk of an ambiguous call for a plain int literal like
    // scalar(3) -- int converts to double as easily as to long long.
    Plan& scalar(double value) {
        if (value < 0.0) {
            throw std::invalid_argument(
                "Plan::scalar(): value must be non-negative -- every RepresentationBase is magnitude-only");
        }
        auto node = std::make_unique<Node>();
        node->kind = Node::Kind::ScalarLeaf;
        node->scalarValue = value;
        placeLeaf(std::move(node));
        return *this;
    }

    template <typename T>
    Plan& number(const T& n) {
        return scalar(n.value());
    }

    // See the class comment above for how this differs from number().
    Plan& numberVia(SmoothNumberBase& n) {
        auto node = std::make_unique<Node>();
        node->kind = Node::Kind::NumberLeaf;
        node->numberSource = &n;
        placeLeaf(std::move(node));
        return *this;
    }

    Plan& plus() {
        applyOperator(Node::Kind::Add);
        return *this;
    }

    Plan& times() {
        applyOperator(Node::Kind::Multiply);
        return *this;
    }

    // Opens a new, independent nested group -- like a `(`.
    Plan& left() {
        stack_.emplace_back();
        return *this;
    }

    // Closes the most recently opened group -- like a `)` -- and plugs its
    // completed expression into whatever slot is open one level up.
    Plan& right() {
        if (stack_.size() <= 1) {
            throw std::invalid_argument("Plan::right(): no open left() group to close");
        }
        std::unique_ptr<Node> finished = std::move(stack_.back().root);
        stack_.pop_back();
        if (!finished) {
            throw std::invalid_argument("Plan::right(): the group opened by left() is empty");
        }
        std::unique_ptr<Node>* slot = writableSlot();
        if (!slot) {
            throw std::invalid_argument(
                "Plan::right(): nothing here is waiting for a value (the enclosing expression is "
                "already complete -- missing an operator?)");
        }
        *slot = std::move(finished);
        return *this;
    }

    // The computed result. Compiles (see the class comment) on first call,
    // or reuses the compilation from an earlier calculate()/plan() call.
    double calculate() {
        ensureCompiled();
        return steps_.back().rep->value();
    }

    // Prints the compiled blueprint as a tree, e.g. (for SparsePlan,
    // 3 * (4 + 2)):
    //
    //   multiply
    //   ├─ ensure(sparse)
    //   │  └─ scalar: 3
    //   └─ add
    //      ├─ ensure(sparse)
    //      │  └─ scalar: 4
    //      └─ ensure(sparse)
    //         └─ scalar: 2
    //   = 18
    void plan(std::ostream& os = std::cout) {
        ensureCompiled();
        printStep(os, steps_.size() - 1, "", true, true);
        os << "= " << steps_.back().rep->value() << "\n";
    }

protected:
    // Node serves as both the declaration (the pure, representation-
    // agnostic expression tree scalar()/number()/numberVia()/plus()/
    // times()/left()/right() build -- ScalarLeaf/NumberLeaf/Add/Multiply
    // only, never Ensure) and, after buildBlueprint() runs, the blueprint
    // itself (the same shape, but with Ensure nodes spliced in wherever a
    // conversion is needed).
    struct Node {
        enum class Kind { ScalarLeaf, NumberLeaf, Ensure, Add, Multiply };
        Kind kind;
        double scalarValue = 0.0;                        // ScalarLeaf
        SmoothNumberBase* numberSource = nullptr;          // NumberLeaf
        SmoothNumberBase::Representation ensureTarget{};    // Ensure
        std::unique_ptr<Node> child;                         // Ensure: the node being converted
        std::unique_ptr<Node> left, right;                    // Add, Multiply
    };

    // Maps the declaration into an executable blueprint by inserting
    // Ensure(target) nodes wherever this Plan's strategy needs a
    // conversion. The one hook every concrete Plan overrides to declare
    // its strategy -- pure virtual, so there is no default "do nothing
    // special" behavior hiding in Plan itself; even DefaultPlan (below)
    // spells out that its target is Scalar.
    virtual std::unique_ptr<Node> buildBlueprint(const Node& declaration) const = 0;

    // Shared by every strategy that just wants every leaf -- scalar() or
    // numberVia() alike -- converted into one target representation: walks
    // `declaration`, wrapping each leaf it finds in Ensure{target}. This is
    // typically a buildBlueprint() override's entire body (see DefaultPlan
    // below and plan_zoo/ for examples).
    std::unique_ptr<Node> wrapLeavesWithEnsure(const Node& declaration,
                                                SmoothNumberBase::Representation target) const {
        auto node = std::make_unique<Node>();
        if (declaration.kind == Node::Kind::ScalarLeaf || declaration.kind == Node::Kind::NumberLeaf) {
            node->kind = Node::Kind::Ensure;
            node->ensureTarget = target;
            node->child = cloneNode(declaration);
            return node;
        }
        node->kind = declaration.kind;
        node->left = wrapLeavesWithEnsure(*declaration.left, target);
        node->right = wrapLeavesWithEnsure(*declaration.right, target);
        return node;
    }

private:
    enum class Op { Add, Multiply };

    // One independent, in-progress sub-expression -- the stack of these is
    // what left()/right() push and pop.
    struct Frame {
        std::unique_ptr<Node> root;  // null until something's been built here
    };

    // The compiled blueprint, executed: one entry per blueprint node, in
    // post-order (a node's operands/child are always compiled -- and thus
    // already present in this list -- before the node itself), each
    // carrying its already-computed representation. The last entry is
    // always the overall root.
    struct Step {
        enum class Kind { Scalar, Ensure, EnsureNumber, Add, Multiply } kind;
        std::unique_ptr<RepresentationBase> rep;
        std::size_t childStep = 0;                 // Ensure
        std::size_t leftStep = 0, rightStep = 0;    // Add, Multiply
        SmoothNumberBase::Representation ensureTarget{};  // Ensure, EnsureNumber
    };

    static bool isLeaf(const Node& n) { return n.kind == Node::Kind::ScalarLeaf || n.kind == Node::Kind::NumberLeaf; }
    static bool isOperator(const Node& n) { return n.kind == Node::Kind::Add || n.kind == Node::Kind::Multiply; }

    static std::unique_ptr<Node> cloneNode(const Node& n) {
        auto copy = std::make_unique<Node>();
        copy->kind = n.kind;
        copy->scalarValue = n.scalarValue;
        copy->numberSource = n.numberSource;
        copy->ensureTarget = n.ensureTarget;
        if (n.child) copy->child = cloneNode(*n.child);
        if (n.left) copy->left = cloneNode(*n.left);
        if (n.right) copy->right = cloneNode(*n.right);
        return copy;
    }

    // Confirms that stripping every Ensure node out of `blueprint` yields
    // back exactly `declaration`'s own shape and leaf contents -- i.e. a
    // buildBlueprint() override may only ever *decorate* the declaration
    // with conversions, never change what's actually being computed.
    // Throws std::invalid_argument -- a bug in the Plan subclass, not a
    // usage error -- if it doesn't match.
    void validateBlueprint(const Node& declaration, const Node& blueprint) const {
        const Node* b = &blueprint;
        while (b->kind == Node::Kind::Ensure) {
            if (!b->child) {
                throw std::invalid_argument("Plan: " + name() +
                                             "::buildBlueprint() produced an Ensure node with no child");
            }
            b = b->child.get();
        }
        if (b->kind != declaration.kind) {
            throw std::invalid_argument("Plan: " + name() +
                                         "::buildBlueprint() changed the shape of the declaration");
        }
        switch (declaration.kind) {
            case Node::Kind::ScalarLeaf:
                if (b->scalarValue != declaration.scalarValue) {
                    throw std::invalid_argument("Plan: " + name() +
                                                 "::buildBlueprint() changed a scalar() leaf's value");
                }
                return;
            case Node::Kind::NumberLeaf:
                if (b->numberSource != declaration.numberSource) {
                    throw std::invalid_argument("Plan: " + name() +
                                                 "::buildBlueprint() changed a numberVia() leaf's number");
                }
                return;
            case Node::Kind::Add:
            case Node::Kind::Multiply:
                validateBlueprint(*declaration.left, *b->left);
                validateBlueprint(*declaration.right, *b->right);
                return;
            case Node::Kind::Ensure:
                return;  // unreachable: declaration never contains Ensure nodes
        }
    }

    // Returns the slot the next leaf/group-result should be written to: the
    // current frame's root, if nothing's there yet, or a still-empty right
    // child of a pending operator. Returns nullptr if the current frame
    // already holds a complete expression with no pending operator -- i.e.
    // there's nowhere left to put a new value without an operator first.
    std::unique_ptr<Node>* writableSlot() {
        Frame& top = stack_.back();
        if (!top.root) return &top.root;
        if (isOperator(*top.root) && !top.root->right) return &top.root->right;
        return nullptr;
    }

    void placeLeaf(std::unique_ptr<Node> node) {
        std::unique_ptr<Node>* slot = writableSlot();
        if (!slot) {
            throw std::invalid_argument(
                "Plan: expected an operator (plus()/times()) before this value -- "
                "the current expression is already complete");
        }
        *slot = std::move(node);
    }

    void applyOperator(Node::Kind op) {
        Frame& top = stack_.back();
        if (!top.root) {
            throw std::invalid_argument("Plan: need a value before plus()/times()");
        }
        if (isOperator(*top.root) && !top.root->right) {
            throw std::invalid_argument("Plan: the pending operator is still missing its right-hand value");
        }
        auto node = std::make_unique<Node>();
        node->kind = op;
        node->left = std::move(top.root);
        top.root = std::move(node);
    }

    // Builds a fresh, empty representation of `target`'s concrete type --
    // the shared factory an Ensure step needs to know what to convert
    // *into*, driven purely by the SmoothNumberBase::Representation value
    // carried in the blueprint (data), rather than by a virtual call to
    // some per-subclass method.
    static std::unique_ptr<RepresentationBase> makeEmptyRepresentation(SmoothNumberBase::Representation target,
                                                                         std::shared_ptr<Metrics> metrics) {
        switch (target) {
            case SmoothNumberBase::Representation::Sparse:
                return std::make_unique<SparseRepresentation>(/*allow_fractional=*/true, std::move(metrics));
            case SmoothNumberBase::Representation::RowValues:
                return std::make_unique<RowValuesRepresentation>(/*allow_fractional=*/true, std::move(metrics));
            case SmoothNumberBase::Representation::Dynamic:
                return std::make_unique<DynamicMatrixRepresentation>(/*allow_fractional=*/true, std::move(metrics));
            case SmoothNumberBase::Representation::Scalar:
                return std::make_unique<ScalarRepresentation>(/*allow_fractional=*/true, std::move(metrics));
        }
        throw std::invalid_argument("Plan: unknown Representation");
    }

    static const char* representationLabel(SmoothNumberBase::Representation r) {
        switch (r) {
            case SmoothNumberBase::Representation::Sparse:
                return "sparse";
            case SmoothNumberBase::Representation::RowValues:
                return "row_values";
            case SmoothNumberBase::Representation::Dynamic:
                return "dynamic";
            case SmoothNumberBase::Representation::Scalar:
                return "scalar";
        }
        return "unknown";
    }

    // Clones `left` and combines `right` into the clone via
    // RepresentationBase's own addInPlace()/multiplyInPlace() -- since
    // those are already virtual and polymorphic over any RepresentationBase
    // (regardless of its concrete type), this one implementation covers
    // every strategy: no per-strategy override needed. `left` and `right`
    // are always the same concrete type in practice, since a Add/Multiply
    // node's operands were both produced by this same Plan's own blueprint.
    std::unique_ptr<RepresentationBase> combine(Op op, const RepresentationBase& left,
                                                 const RepresentationBase& right) const {
        std::unique_ptr<RepresentationBase> result = left.clone();
        if (op == Op::Add) {
            result->addInPlace(right);
        } else {
            result->multiplyInPlace(right);
        }
        return result;
    }

    // Executes one blueprint node (and, recursively, everything it depends
    // on), appending each result to steps_ in post-order and returning the
    // index of the one just appended. Each Ensure/Add/Multiply step
    // increments a matching counter on metrics_, if one was given.
    std::size_t compileBlueprintNode(const Node& node) {
        switch (node.kind) {
            case Node::Kind::ScalarLeaf: {
                auto rep = std::make_unique<ScalarRepresentation>(/*allow_fractional=*/true, metrics_);
                rep->setColumnValue(0, node.scalarValue);
                Step step;
                step.kind = Step::Kind::Scalar;
                step.rep = std::move(rep);
                steps_.push_back(std::move(step));
                return steps_.size() - 1;
            }
            case Node::Kind::NumberLeaf:
                // Only ever valid as the direct child of an Ensure node --
                // there's no meaningful "materialize, unconverted" form for
                // an existing number the way a raw scalar naturally becomes
                // Scalar, so a well-formed blueprint never compiles one on
                // its own (see the Ensure case below, which special-cases
                // this instead of recursing into it).
                throw std::invalid_argument("Plan: " + name() +
                                             "::buildBlueprint() left a numberVia() leaf unwrapped by Ensure()");
            case Node::Kind::Ensure: {
                if (metrics_) metrics_->increment("convert_to_" + name());
                if (node.child->kind == Node::Kind::NumberLeaf) {
                    // A genuine ensure()-driven conversion through the
                    // number's own internal representation cache -- see
                    // SmoothNumberBase::representationAs(). The clone comes
                    // back carrying *that number's* Metrics (or none), so
                    // it's re-pointed at this Plan's own Metrics before
                    // anything downstream (e.g. combine()) touches it.
                    std::unique_ptr<RepresentationBase> rep =
                        node.child->numberSource->representationAs(node.ensureTarget);
                    rep->setMetricsPtr(metrics_);
                    Step step;
                    step.kind = Step::Kind::EnsureNumber;
                    step.rep = std::move(rep);
                    step.ensureTarget = node.ensureTarget;
                    steps_.push_back(std::move(step));
                    return steps_.size() - 1;
                }
                std::size_t childStep = compileBlueprintNode(*node.child);
                std::unique_ptr<RepresentationBase> rep = makeEmptyRepresentation(node.ensureTarget, metrics_);
                steps_[childStep].rep->forEachSet([&rep](int i, int j) { rep->set(i, j, true); });
                Step step;
                step.kind = Step::Kind::Ensure;
                step.rep = std::move(rep);
                step.childStep = childStep;
                step.ensureTarget = node.ensureTarget;
                steps_.push_back(std::move(step));
                return steps_.size() - 1;
            }
            case Node::Kind::Add:
            case Node::Kind::Multiply: {
                std::size_t leftStep = compileBlueprintNode(*node.left);
                std::size_t rightStep = compileBlueprintNode(*node.right);
                Op op = node.kind == Node::Kind::Add ? Op::Add : Op::Multiply;
                std::unique_ptr<RepresentationBase> rep = combine(op, *steps_[leftStep].rep, *steps_[rightStep].rep);
                if (metrics_) metrics_->increment(op == Op::Add ? "add" : "multiply");
                Step step;
                step.kind = node.kind == Node::Kind::Add ? Step::Kind::Add : Step::Kind::Multiply;
                step.rep = std::move(rep);
                step.leftStep = leftStep;
                step.rightStep = rightStep;
                steps_.push_back(std::move(step));
                return steps_.size() - 1;
            }
        }
        throw std::invalid_argument("Plan: unreachable node kind");
    }

    void ensureCompiled() {
        if (compiled_) return;
        if (stack_.size() != 1) {
            throw std::invalid_argument("Plan: an open left() group is missing its matching right()");
        }
        if (!stack_.front().root) {
            throw std::invalid_argument("Plan: nothing has been built yet");
        }
        const Node& declaration = *stack_.front().root;
        if (isOperator(declaration) && !declaration.right) {
            throw std::invalid_argument("Plan: the last operator is missing its right-hand value");
        }
        std::unique_ptr<Node> blueprint = buildBlueprint(declaration);
        validateBlueprint(declaration, *blueprint);
        compileBlueprintNode(*blueprint);
        compiled_ = true;
    }

    static const char* opLabel(Step::Kind kind) {
        switch (kind) {
            case Step::Kind::Add:
                return "add";
            case Step::Kind::Multiply:
                return "multiply";
            default:
                return "";
        }
    }

    void printStep(std::ostream& os, std::size_t index, const std::string& indent, bool isLast,
                   bool isRoot) const {
        const Step& step = steps_[index];
        os << indent;
        if (!isRoot) os << (isLast ? "└─ " : "├─ ");
        switch (step.kind) {
            case Step::Kind::Scalar:
                os << "scalar: " << step.rep->value() << "\n";
                return;
            case Step::Kind::EnsureNumber:
                os << "ensure(" << representationLabel(step.ensureTarget) << "): " << step.rep->value() << "\n";
                return;
            case Step::Kind::Ensure: {
                os << "ensure(" << representationLabel(step.ensureTarget) << ")\n";
                std::string childIndent = indent + (isRoot ? "" : (isLast ? "   " : "│  "));
                printStep(os, step.childStep, childIndent, true, false);
                return;
            }
            case Step::Kind::Add:
            case Step::Kind::Multiply: {
                os << opLabel(step.kind) << "\n";
                std::string childIndent = indent + (isRoot ? "" : (isLast ? "   " : "│  "));
                printStep(os, step.leftStep, childIndent, false, false);
                printStep(os, step.rightStep, childIndent, true, false);
                return;
            }
        }
    }

    std::vector<Frame> stack_;
    std::vector<Step> steps_;
    bool compiled_ = false;
    std::shared_ptr<Metrics> metrics_;
};

// The default strategy: every leaf (scalar()/number() or numberVia()) is
// ensured into a plain ScalarRepresentation, and every add/multiply runs
// through ScalarRepresentation's own addInPlace()/multiplyInPlace() -- this
// is "ScalarPlan" in the same sense SparsePlan/MatrixPlan/RowValuesPlan
// (see plan_zoo/) are, just given the name most code reaches for by
// default, and defined here alongside Plan itself rather than living in
// plan_zoo/. buildBlueprint() is the entire strategy: wrap every leaf in
// Ensure(Scalar), via the shared wrapLeavesWithEnsure() helper.
class DefaultPlan : public Plan {
public:
    explicit DefaultPlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "scalar"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapLeavesWithEnsure(declaration, SmoothNumberBase::Representation::Scalar);
    }
};

}  // namespace smooth
