// Compiled fresh by evolve/evaluator.py for every candidate EvolvedPlan.
// Runs every profile in smooth::allProfiles() (include/smooth/profile_zoo.hpp
// -- the same set src/profile_runner.cpp measures) against both
// smooth::DefaultPlan (trusted ground truth) and smooth::EvolvedPlan
// (evolved_plan.hpp, copied into this compile's -I directory by
// evaluator.py), sums EvolvedPlan's Metrics counters across all of them, and
// prints one JSON line to stdout:
//
//   {"ok": true, "correct": true, "counters": {...}}
//   {"ok": true, "correct": false, "mismatches": ["profile_name", ...]}
//   {"ok": false, "error": "..."}
//
// "ok": false means an exception escaped (a malformed blueprint, an
// unsupported representation/reduction combination, etc.) -- evaluator.py
// treats that the same as a compile failure. "correct": false means it ran
// but produced a different value than DefaultPlan on at least one profile
// (a real bug -- Plan's own representation math is supposed to be
// value-preserving regardless of which representations/reductions a
// strategy picks).
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "evolved_plan.hpp"
#include "smooth/plan_zoo.hpp"
#include "smooth/profile_zoo.hpp"

using namespace smooth;

namespace {

// The counters evolve/evaluator.py actually scores. Anything else a plan's
// Metrics might tally is ignored, same idea as profile_runner.cpp's own
// counterColumns() (see its comment) but hardcoded here since the harness
// itself only ever reports these five.
const std::vector<std::string>& scoredCounters() {
    static const std::vector<std::string> names = {
        "bit_operations", "scalar_operations", "carries", "total_converts", "atomic_transforms",
    };
    return names;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            default:
                out += c;
        }
    }
    return out;
}

}  // namespace

int main() {
    std::map<std::string, long long> totals;
    for (const auto& name : scoredCounters()) totals[name] = 0;

    std::vector<std::string> mismatches;

    try {
        for (const Profile& profile : allProfiles()) {
            DefaultPlan reference;
            double referenceValue = profile.run(reference);

            auto metrics = std::make_shared<Metrics>();
            EvolvedPlan candidate(metrics);
            double candidateValue = profile.run(candidate);

            if (std::fabs(candidateValue - referenceValue) > 1e-6) {
                mismatches.push_back(profile.name);
            }

            for (const auto& name : scoredCounters()) totals[name] += metrics->get(name);
        }
    } catch (const std::exception& e) {
        std::cout << "{\"ok\": false, \"error\": \"" << jsonEscape(e.what()) << "\"}\n";
        return 0;
    } catch (...) {
        std::cout << "{\"ok\": false, \"error\": \"unknown exception\"}\n";
        return 0;
    }

    if (!mismatches.empty()) {
        std::cout << "{\"ok\": true, \"correct\": false, \"mismatches\": [";
        for (std::size_t i = 0; i < mismatches.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << "\"" << jsonEscape(mismatches[i]) << "\"";
        }
        std::cout << "]}\n";
        return 0;
    }

    std::cout << "{\"ok\": true, \"correct\": true, \"counters\": {";
    bool first = true;
    for (const auto& name : scoredCounters()) {
        if (!first) std::cout << ", ";
        first = false;
        std::cout << "\"" << name << "\": " << totals[name];
    }
    std::cout << "}}\n";
    return 0;
}
