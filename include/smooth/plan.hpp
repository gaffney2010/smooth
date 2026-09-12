#pragma once

#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "smooth/metrics.hpp"
#include "smooth/reduction.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/representation_zoo.hpp"
#include "smooth/smooth_number_base.hpp"

namespace smooth {

// A fluent builder for an arithmetic expression over 3-smooth numbers.
// Building it produces a **declaration** -- a pure, representation-agnostic
// record of what was asked for (scalar/number leaves combined by
// add/multiply); building never computes anything. Compiling (the first
// call to plan()/calculate()) turns that into a **blueprint**: the same
// tree with explicit Ensure(target) steps spliced in wherever a leaf needs
// a specific representation. The blueprint is what's executed -- and
// printed by plan() -- so every conversion shows up as a real,
// inspectable step:
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
// mapping a declaration node to a blueprint node. Every plan_zoo strategy's
// override is one line, built on the shared wrapLeavesWithEnsure() helper
// below. Immediately after buildBlueprint() runs, validateBlueprint()
// (below) confirms that stripping every Ensure node back out yields the
// declaration's exact shape again -- so an override can only *decorate*
// what's being computed, never change it.
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
// - scalar(x): a leaf. Throws std::invalid_argument for a negative x --
//   every RepresentationBase is magnitude-only, and a negative value would
//   otherwise infinite-loop the first bit decomposition -- so this is the
//   one place a raw literal enters the system.
// - number(n): a leaf snapshotting an existing number's value immediately
//   (via scalar(n.value())). Templated so T::value() resolves at T's own
//   type -- required for signed types, whose value() hides rather than
//   overrides SmoothNumberBase::value().
// - numberVia(n): keeps a live reference to n (must outlive
//   calculate()/plan()) instead. Its Ensure step calls n's own
//   representationAs(target) directly -- a genuine conversion through n's
//   own cache -- so n's own Metrics sees the resulting counter.
// - plus() / times(): wraps everything built so far at this nesting level
//   into a new operator node, as its left side.
// - left() / right(): open/close a nested group, like `(`/`)` -- left()
//   starts a fresh sub-expression, right() finishes the most recent one
//   and plugs it into whatever slot is open one level up.
// - calculate(): the result, as a double.
// - plan(os): prints the compiled blueprint as a tree.
//
// name() identifies the strategy ("scalar" for DefaultPlan; others in
// plan_zoo/) and drives the convert_to_<name> metrics counter.
//
// Like every concrete SmoothNumberBase-derived type, a Plan optionally
// takes a shared Metrics. One shared between a Plan and the numbers
// feeding it (via number()/numberVia()) tallies both under the same
// counters: convert_to_<name()>/add/multiply from the Plan, whatever
// representation-level work executing produces, and -- for numberVia()
// specifically -- the fed-in number's own convert_<canonical>_to_<target>
// counter too.
class Plan {
public:
    explicit Plan(std::shared_ptr<Metrics> metrics = nullptr) : metrics_(std::move(metrics)) {
        stack_.emplace_back();
    }

    virtual ~Plan() = default;

    virtual std::string name() const = 0;

    bool hasMetrics() const { return static_cast<bool>(metrics_); }
    const std::shared_ptr<Metrics>& metricsPtr() const { return metrics_; }

    // A single double overload rather than separate long long/double
    // overloads (as setValue() has elsewhere): a second overload would
    // only risk an ambiguous call for a plain int literal like scalar(3).
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
    // Node serves as both the declaration (ScalarLeaf/NumberLeaf/Add/
    // Multiply only) and, after buildBlueprint() runs, the blueprint
    // itself (the same shape with Ensure/Reduce nodes spliced in).
    struct Node {
        enum class Kind { ScalarLeaf, NumberLeaf, Ensure, Reduce, Add, Multiply };
        Kind kind;
        double scalarValue = 0.0;                        // ScalarLeaf
        SmoothNumberBase* numberSource = nullptr;          // NumberLeaf
        SmoothNumberBase::Representation ensureTarget{};    // Ensure
        const Reduction* reduction = nullptr;  // Reduce
        std::unique_ptr<Node> child;                         // Ensure, Reduce: the node being wrapped
        std::unique_ptr<Node> left, right;                    // Add, Multiply
    };

    // Maps the declaration into an executable blueprint by inserting
    // Ensure(target) nodes wherever this Plan's strategy needs a
    // conversion. The one hook every concrete Plan overrides -- pure
    // virtual, so even DefaultPlan (below) spells out its own target.
    virtual std::unique_ptr<Node> buildBlueprint(const Node& declaration) const = 0;

    // Shared by every strategy that wants every leaf converted into one
    // target representation: walks `declaration`, wrapping each leaf in
    // Ensure{target}. Typically a buildBlueprint() override's entire body.
    // Built on the per-leaf overload below, fixed to always answer with
    // the same `target`.
    std::unique_ptr<Node> wrapLeavesWithEnsure(const Node& declaration,
                                                SmoothNumberBase::Representation target) const {
        return wrapLeavesWithEnsure(declaration, [target](const Node&) { return target; });
    }

    // Same idea, but `chooseTarget` picks the representation for each leaf
    // individually -- e.g. plan_zoo/representation_aware_plan.hpp reuses
    // whatever representation a numberVia() leaf's own number already has
    // cached, instead of forcing every leaf into the same one.
    std::unique_ptr<Node> wrapLeavesWithEnsure(
        const Node& declaration,
        const std::function<SmoothNumberBase::Representation(const Node&)>& chooseTarget) const {
        auto node = std::make_unique<Node>();
        if (isLeaf(declaration)) {
            node->kind = Node::Kind::Ensure;
            node->ensureTarget = chooseTarget(declaration);
            node->child = cloneNode(declaration);
            return node;
        }
        node->kind = declaration.kind;
        node->left = wrapLeavesWithEnsure(*declaration.left, chooseTarget);
        node->right = wrapLeavesWithEnsure(*declaration.right, chooseTarget);
        return node;
    }

    // Wraps an already-built blueprint node in a Reduce node that, at
    // execution time, runs `reduction` against a clone of that node's own
    // representation (so the un-reduced node still prints its original
    // bits). `reduction` is any Reduction (reduction.hpp), so a
    // buildBlueprint() override never has to name which concrete kind it's
    // using.
    std::unique_ptr<Node> wrapWithReduction(std::unique_ptr<Node> node, const Reduction& reduction) const {
        auto wrapped = std::make_unique<Node>();
        wrapped->kind = Node::Kind::Reduce;
        wrapped->reduction = &reduction;
        wrapped->child = std::move(node);
        return wrapped;
    }

    // Walks an already-built blueprint, wrapping both operands of every
    // Multiply node (however deeply nested) in a Reduce node running
    // `reduction`. Add nodes are left alone -- see MergingSparsePlan's own
    // comment for why only Multiply operands are worth reducing. Built on
    // the per-operand overload below, fixed to always answer with the
    // same `reduction`.
    std::unique_ptr<Node> wrapMultiplyOperandsWithReduction(std::unique_ptr<Node> node,
                                                             const Reduction& reduction) const {
        return wrapMultiplyOperandsWithReduction(std::move(node),
                                                  [&reduction](const Node&) { return &reduction; });
    }

    // Same idea, but `chooseReduction` picks (or declines, by returning
    // nullptr) a reduction for each multiply operand individually, based
    // on that operand's own already-built blueprint subtree -- what
    // plan_zoo/size_adaptive_sparse_plan.hpp (by estimated bit count) and
    // plan_zoo/representation_aware_plan.hpp (by representation) use
    // instead of applying one fixed reduction everywhere.
    std::unique_ptr<Node> wrapMultiplyOperandsWithReduction(
        std::unique_ptr<Node> node, const std::function<const Reduction*(const Node&)>& chooseReduction) const {
        if (node->kind == Node::Kind::Add || node->kind == Node::Kind::Multiply) {
            node->left = wrapMultiplyOperandsWithReduction(std::move(node->left), chooseReduction);
            node->right = wrapMultiplyOperandsWithReduction(std::move(node->right), chooseReduction);
            if (node->kind == Node::Kind::Multiply) {
                if (const Reduction* r = chooseReduction(*node->left)) {
                    node->left = wrapWithReduction(std::move(node->left), *r);
                }
                if (const Reduction* r = chooseReduction(*node->right)) {
                    node->right = wrapWithReduction(std::move(node->right), *r);
                }
            }
        }
        return node;
    }

private:
    enum class Op { Add, Multiply };

    // One independent, in-progress sub-expression -- what left()/right()
    // push and pop.
    struct Frame {
        std::unique_ptr<Node> root;  // null until something's been built here
    };

    // The compiled blueprint, executed: one entry per blueprint node, in
    // post-order, each carrying its already-computed representation. The
    // last entry is always the overall root.
    struct Step {
        enum class Kind { Scalar, Ensure, EnsureNumber, Reduce, Add, Multiply } kind;
        std::unique_ptr<RepresentationBase> rep;
        std::size_t childStep = 0;                 // Ensure, Reduce
        std::size_t leftStep = 0, rightStep = 0;    // Add, Multiply
        SmoothNumberBase::Representation ensureTarget{};  // Ensure, EnsureNumber
        const Reduction* reduction = nullptr;  // Reduce
    };

    static bool isLeaf(const Node& n) { return n.kind == Node::Kind::ScalarLeaf || n.kind == Node::Kind::NumberLeaf; }
    static bool isOperator(const Node& n) { return n.kind == Node::Kind::Add || n.kind == Node::Kind::Multiply; }

    static std::unique_ptr<Node> cloneNode(const Node& n) {
        auto copy = std::make_unique<Node>();
        copy->kind = n.kind;
        copy->scalarValue = n.scalarValue;
        copy->numberSource = n.numberSource;
        copy->ensureTarget = n.ensureTarget;
        copy->reduction = n.reduction;
        if (n.child) copy->child = cloneNode(*n.child);
        if (n.left) copy->left = cloneNode(*n.left);
        if (n.right) copy->right = cloneNode(*n.right);
        return copy;
    }

    // Confirms that stripping every Ensure/Reduce node out of `blueprint`
    // yields back exactly `declaration`'s own shape and leaf contents.
    // Throws std::invalid_argument -- a bug in the Plan subclass -- if not.
    void validateBlueprint(const Node& declaration, const Node& blueprint) const {
        const Node* b = &blueprint;
        while (b->kind == Node::Kind::Ensure || b->kind == Node::Kind::Reduce) {
            if (!b->child) {
                throw std::invalid_argument("Plan: " + name() +
                                             "::buildBlueprint() produced an Ensure/Reduce node with no child");
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
            case Node::Kind::Reduce:
                return;  // unreachable: declaration never contains Ensure/Reduce nodes
        }
    }

    // The slot the next leaf/group-result should be written to: the
    // current frame's root if empty, or a pending operator's empty right
    // child. Returns nullptr if there's nowhere left to put a new value
    // without an operator first.
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
    // what an Ensure step converts into, driven by the Representation
    // value carried in the blueprint rather than a per-subclass method.
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
    // RepresentationBase's own addInPlace()/multiplyInPlace() -- one
    // implementation covers every strategy, no per-strategy override
    // needed.
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

    // Executes one blueprint node (and, recursively, its dependencies),
    // appending each result to steps_ in post-order and returning the
    // index just appended. Each Ensure/Add/Multiply step increments a
    // matching counter on metrics_, if one was given.
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
                // there's no meaningful "unconverted" form for an existing
                // number, so a well-formed blueprint never compiles one on
                // its own (the Ensure case below special-cases it instead).
                throw std::invalid_argument("Plan: " + name() +
                                             "::buildBlueprint() left a numberVia() leaf unwrapped by Ensure()");
            case Node::Kind::Ensure: {
                if (metrics_) metrics_->increment("convert_to_" + name());
                if (node.child->kind == Node::Kind::NumberLeaf) {
                    // A genuine conversion through the number's own cache
                    // (representationAs()); re-pointed at this Plan's own
                    // Metrics before anything downstream touches it.
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
            case Node::Kind::Reduce: {
                std::size_t childStep = compileBlueprintNode(*node.child);
                // Cloned, not mutated in place, so the child step still
                // shows its pre-reduction bits.
                std::unique_ptr<RepresentationBase> rep = steps_[childStep].rep->clone();
                node.reduction->run(*rep, metrics_);
                Step step;
                step.kind = Step::Kind::Reduce;
                step.rep = std::move(rep);
                step.childStep = childStep;
                step.reduction = node.reduction;
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
            case Step::Kind::Reduce: {
                os << "reduce(" << step.reduction->name() << ")\n";
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

// The default strategy: every leaf is ensured into a plain
// ScalarRepresentation. This is "ScalarPlan" in the same sense
// SparsePlan/MatrixPlan/RowValuesPlan (plan_zoo/) are, just given the name
// most code reaches for by default, and defined here rather than there.
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
