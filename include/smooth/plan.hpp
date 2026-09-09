#pragma once

#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "smooth/metrics.hpp"

namespace smooth {

// A fluent builder for a scalar arithmetic expression over 3-smooth
// numbers, e.g.:
//
//   double result = Plan()
//       .scalar(3)
//       .times()
//       .left()
//         .scalar(4)
//         .plus()
//         .scalar(2)
//       .right()
//       .calculate();  // 3 * (4 + 2) = 18
//
// Building never computes anything -- it just records an expression tree.
// Actually compiling that tree into a concrete sequence of steps (and, for
// now, running the arithmetic) happens lazily, the first time plan() or
// calculate() is called.
//
// - scalar(x) / number(n): a leaf. scalar() takes a plain int/float value;
//   number() takes any existing SmoothNumberBase-derived object and uses
//   its value(). number() is a template specifically so T::value() is
//   resolved at compile time against T's *own* type -- required for signed
//   types, whose value() intentionally hides (isn't a virtual override of)
//   SmoothNumberBase::value() (see smooth_number_base.hpp); calling it
//   through a SmoothNumberBase& would silently drop the sign.
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
// - calculate(): the computed result, as a double.
// - plan(os): prints the compiled steps as a tree (see below).
//
// For now, this base class computes by converting every leaf to a plain
// scalar and evaluating the whole thing with ordinary double arithmetic --
// no attempt is made to pick a smarter representation for the actual
// SmoothNumber machinery. That's what name()/convertLeaf()/combine() below
// are for: a derived class overrides them to compute via a specific
// RepresentationBase instead (see include/smooth/plan_zoo/ for a few --
// SparsePlan, MatrixPlan, RowValuesPlan -- with more meant to follow as
// this library explores which representation is actually fastest for
// what). This base implementation is effectively "ScalarPlan" in that
// family, just built directly into Plan itself rather than living in
// plan_zoo/, since a plain double *is* how a scalar is represented.
//
// Like every concrete SmoothNumberBase-derived type, a Plan optionally
// takes a shared Metrics at construction (see metrics.hpp). Compiling
// increments one counter per step -- convert_to_<name()>, add, or multiply
// -- mirroring how SmoothNumberBase counts each representation conversion,
// so a Metrics shared between a Plan and the numbers that feed it (via
// number()) tallies both under the same counters.
class Plan {
public:
    explicit Plan(std::shared_ptr<Metrics> metrics = nullptr) : metrics_(std::move(metrics)) {
        stack_.emplace_back();
    }

    virtual ~Plan() = default;

    // Identifies which strategy this Plan (or Plan subclass) uses to
    // perform its arithmetic -- "scalar" here; "sparse"/"matrix"/
    // "row_values" for the plan_zoo/ subclasses that override it. Also
    // drives the "convert to <name>" step label (see printStep()) and the
    // convert_to_<name> metrics counter, so a subclass overriding name()
    // alone (with no other changes) would still show up distinctly in
    // both the printed plan and the metrics.
    virtual std::string name() const { return "scalar"; }

    bool hasMetrics() const { return static_cast<bool>(metrics_); }
    const std::shared_ptr<Metrics>& metricsPtr() const { return metrics_; }

    // A single double overload (rather than separate long long/double
    // overloads, as setValue() has elsewhere in this library): both would
    // do exactly the same thing here, so a second overload would only add
    // the risk of an ambiguous call for a plain int literal like
    // scalar(3) -- int converts to double as easily as to long long.
    Plan& scalar(double value) {
        placeLeaf(value);
        return *this;
    }

    template <typename T>
    Plan& number(const T& n) {
        placeLeaf(n.value());
        return *this;
    }

    Plan& plus() {
        applyOperator(Op::Add);
        return *this;
    }

    Plan& times() {
        applyOperator(Op::Multiply);
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
        return steps_.back().result;
    }

    // Prints the compiled steps as a tree, e.g.:
    //
    //   multiply
    //   ├─ convert to scalar: 3
    //   └─ add
    //      ├─ convert to scalar: 4
    //      └─ convert to scalar: 2
    //   = 18
    void plan(std::ostream& os = std::cout) {
        ensureCompiled();
        printStep(os, steps_.size() - 1, "", true, true);
        os << "= " << steps_.back().result << "\n";
    }

protected:
    // protected (not private) so an overridden combine() can be declared
    // in terms of it -- see plan_zoo/ for examples.
    enum class Op { Add, Multiply };

    // Converts a leaf's raw value into whatever this Plan variant actually
    // computes with, returning it back out as a double (the type
    // steps_/printing/calculate() all deal in regardless of subclass). The
    // default does nothing -- a plain double already *is* how this base
    // class computes. A subclass that wants to genuinely round-trip
    // through a RepresentationBase (e.g. to exercise its real encode/carry
    // logic) does so here, converting back to double at the end since
    // that's what the rest of Plan expects.
    virtual double convertLeaf(double raw) const { return raw; }

    // Combines two already-converted values with the given operator,
    // again returning a double. The default is ordinary double add/
    // multiply; a subclass overriding this to route through a
    // RepresentationBase's own addInPlace()/multiplyInPlace() is what
    // "converts everything to <representation> and computes that way"
    // actually means in practice -- see plan_zoo/.
    virtual double combine(Op op, double left, double right) const {
        return op == Op::Add ? (left + right) : (left * right);
    }

private:
    // The expression tree as built so far, in its rawest form.
    struct Node {
        bool isLeaf = false;
        double value = 0.0;                      // when isLeaf
        Op op = Op::Add;                          // when !isLeaf
        std::unique_ptr<Node> left, right;         // when !isLeaf
    };

    // One independent, in-progress sub-expression -- the stack of these is
    // what left()/right() push and pop.
    struct Frame {
        std::unique_ptr<Node> root;  // null until something's been built here
    };

    // The compiled plan: one entry per node of the tree, in post-order (a
    // node's operands are always compiled -- and thus already present in
    // this list -- before the node itself), each carrying its already-
    // computed result. The last entry is always the overall root.
    struct Step {
        enum class Kind { Convert, Add, Multiply } kind;
        double leafValue = 0.0;                 // when kind == Convert
        std::size_t leftStep = 0, rightStep = 0;  // when kind == Add/Multiply
        double result = 0.0;
    };

    // Returns the slot the next leaf/group-result should be written to: the
    // current frame's root, if nothing's there yet, or a still-empty right
    // child of a pending operator. Returns nullptr if the current frame
    // already holds a complete expression with no pending operator -- i.e.
    // there's nowhere left to put a new value without an operator first.
    std::unique_ptr<Node>* writableSlot() {
        Frame& top = stack_.back();
        if (!top.root) return &top.root;
        if (!top.root->isLeaf && !top.root->right) return &top.root->right;
        return nullptr;
    }

    void placeLeaf(double value) {
        std::unique_ptr<Node>* slot = writableSlot();
        if (!slot) {
            throw std::invalid_argument(
                "Plan: expected an operator (plus()/times()) before this value -- "
                "the current expression is already complete");
        }
        auto node = std::make_unique<Node>();
        node->isLeaf = true;
        node->value = value;
        *slot = std::move(node);
    }

    void applyOperator(Op op) {
        Frame& top = stack_.back();
        if (!top.root) {
            throw std::invalid_argument("Plan: need a value before plus()/times()");
        }
        if (!top.root->isLeaf && !top.root->right) {
            throw std::invalid_argument("Plan: the pending operator is still missing its right-hand value");
        }
        auto node = std::make_unique<Node>();
        node->isLeaf = false;
        node->op = op;
        node->left = std::move(top.root);
        top.root = std::move(node);
    }

    // Builds steps_ from the tree via a post-order walk, delegating the
    // actual arithmetic to convertLeaf()/combine() (see above) -- so this
    // one method is shared by every Plan variant; only those two hooks
    // differ per subclass. Each step increments a matching counter on
    // metrics_, if one was given.
    std::size_t compileNode(const Node& node) {
        if (node.isLeaf) {
            if (metrics_) metrics_->increment("convert_to_" + name());
            double converted = convertLeaf(node.value);
            steps_.push_back(Step{Step::Kind::Convert, converted, 0, 0, converted});
            return steps_.size() - 1;
        }
        std::size_t leftStep = compileNode(*node.left);
        std::size_t rightStep = compileNode(*node.right);
        double leftVal = steps_[leftStep].result;
        double rightVal = steps_[rightStep].result;
        Step::Kind kind = (node.op == Op::Add) ? Step::Kind::Add : Step::Kind::Multiply;
        double result = combine(node.op, leftVal, rightVal);
        if (metrics_) metrics_->increment(node.op == Op::Add ? "add" : "multiply");
        steps_.push_back(Step{kind, 0.0, leftStep, rightStep, result});
        return steps_.size() - 1;
    }

    void ensureCompiled() {
        if (compiled_) return;
        if (stack_.size() != 1) {
            throw std::invalid_argument("Plan: an open left() group is missing its matching right()");
        }
        if (!stack_.front().root) {
            throw std::invalid_argument("Plan: nothing has been built yet");
        }
        if (!stack_.front().root->isLeaf && !stack_.front().root->right) {
            throw std::invalid_argument("Plan: the last operator is missing its right-hand value");
        }
        compileNode(*stack_.front().root);
        compiled_ = true;
    }

    static const char* opLabel(Step::Kind kind) {
        switch (kind) {
            case Step::Kind::Add:
                return "add";
            case Step::Kind::Multiply:
                return "multiply";
            case Step::Kind::Convert:
                return "";
        }
        return "";
    }

    void printStep(std::ostream& os, std::size_t index, const std::string& indent, bool isLast,
                   bool isRoot) const {
        const Step& step = steps_[index];
        os << indent;
        if (!isRoot) os << (isLast ? "└─ " : "├─ ");
        if (step.kind == Step::Kind::Convert) {
            os << "convert to " << name() << ": " << step.leafValue << "\n";
            return;
        }
        os << opLabel(step.kind) << "\n";
        std::string childIndent = indent + (isRoot ? "" : (isLast ? "   " : "│  "));
        printStep(os, step.leftStep, childIndent, false, false);
        printStep(os, step.rightStep, childIndent, true, false);
    }

    std::vector<Frame> stack_;
    std::vector<Step> steps_;
    bool compiled_ = false;
    std::shared_ptr<Metrics> metrics_;
};

}  // namespace smooth
