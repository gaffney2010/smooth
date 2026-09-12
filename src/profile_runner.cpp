// Runs every (plan kind, profile) combination not already present in a
// SQLite database, and records the resulting Metrics counters. Re-running
// is always safe and cheap: anything already present is skipped.
//
// The set of counter columns is fixed (see counterColumns() below). If a
// new counter is added to the library later, this program's schema check
// refuses to run against an old database -- delete it and let it be
// recreated fresh, rather than migrating it in place.
#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "smooth/plan_zoo.hpp"
#include "smooth/smooth_integer.hpp"

using namespace smooth;

namespace {

// A named, fixed arithmetic expression to run against a Plan (or any Plan
// subclass), purely so the same expression can be measured identically
// across every representation. run() is written directly in terms of
// Plan's own builder methods, so it works unchanged against any concrete
// Plan. It builds *and* calls calculate() itself, so a profile that
// constructs its own SmoothNumberBase objects for numberVia() can keep
// them alive exactly as long as needed, within its own local scope.
//
// These are meant to look like ordinary, everyday arithmetic (no zeros,
// negatives, or single-leaf expressions), ranging up to the "typical"
// upper end (6-10 numbers) without going pathological.
struct Profile {
    std::string name;
    std::function<double(Plan&)> run;
};

// Ordered roughly from smallest/simplest to largest/most structurally
// involved.
std::vector<Profile> allProfiles() {
    std::vector<Profile> profiles;

    // 2 numbers: the simplest possible non-trivial expression.
    profiles.push_back({"two_number_sum", [](Plan& p) { return p.scalar(47).plus().scalar(35).calculate(); }});
    profiles.push_back(
        {"two_number_product", [](Plan& p) { return p.scalar(12).times().scalar(9).calculate(); }});

    // 3 numbers: a small mixed-operator expression, like a line-item total
    // (price * quantity) + shipping.
    profiles.push_back({"price_quantity_plus_shipping", [](Plan& p) {
                             return p.left().scalar(25).times().scalar(3).right().plus().scalar(8).calculate();
                         }});

    // 4 numbers: an unbracketed left-associative chain, like summing four
    // quarterly totals.
    profiles.push_back({"quarterly_totals", [](Plan& p) {
                             return p.scalar(120)
                                 .plus()
                                 .scalar(95)
                                 .plus()
                                 .scalar(130)
                                 .plus()
                                 .scalar(110)
                                 .calculate();
                         }});

    // 6 numbers: a small basket of (price * quantity) line items summed
    // together -- a realistic mix of both operators.
    profiles.push_back({"weighted_basket", [](Plan& p) {
                             return p.left()
                                 .scalar(12)
                                 .times()
                                 .scalar(2)
                                 .right()
                                 .plus()
                                 .left()
                                 .scalar(5)
                                 .times()
                                 .scalar(4)
                                 .right()
                                 .plus()
                                 .left()
                                 .scalar(9)
                                 .times()
                                 .scalar(3)
                                 .right()
                                 .calculate();
                         }});

    // 6 numbers: a longer unbracketed sum, e.g. six monthly totals.
    profiles.push_back({"half_year_totals", [](Plan& p) {
                             return p.scalar(40)
                                 .plus()
                                 .scalar(35)
                                 .plus()
                                 .scalar(50)
                                 .plus()
                                 .scalar(45)
                                 .plus()
                                 .scalar(60)
                                 .plus()
                                 .scalar(55)
                                 .calculate();
                         }});

    // 8 numbers: two levels of nested groups -- (a+b)*(c+d) + (e+f)*(g+h) --
    // like combining two independently-scored halves of a game.
    profiles.push_back({"nested_score_totals", [](Plan& p) {
                             return p.left()
                                 .left()
                                 .scalar(3)
                                 .plus()
                                 .scalar(5)
                                 .right()
                                 .times()
                                 .left()
                                 .scalar(2)
                                 .plus()
                                 .scalar(4)
                                 .right()
                                 .right()
                                 .plus()
                                 .left()
                                 .left()
                                 .scalar(6)
                                 .plus()
                                 .scalar(1)
                                 .right()
                                 .times()
                                 .left()
                                 .scalar(4)
                                 .plus()
                                 .scalar(3)
                                 .right()
                                 .right()
                                 .calculate();
                         }});

    // 10 numbers: the upper end of "typical" -- a flat sum, e.g. ten daily
    // readings totaled at the end of the week-plus.
    profiles.push_back({"ten_day_totals", [](Plan& p) {
                             return p.scalar(7)
                                 .plus()
                                 .scalar(3)
                                 .plus()
                                 .scalar(9)
                                 .plus()
                                 .scalar(2)
                                 .plus()
                                 .scalar(8)
                                 .plus()
                                 .scalar(5)
                                 .plus()
                                 .scalar(6)
                                 .plus()
                                 .scalar(4)
                                 .plus()
                                 .scalar(10)
                                 .plus()
                                 .scalar(1)
                                 .calculate();
                         }});

    // 3 numbers, via numberVia() instead of scalar(): three real
    // SmoothInteger objects, summed. Sharing the Plan's own Metrics with
    // each number means every number's own conversion counter lands in
    // the same Metrics row as the rest of that Plan's counters.
    profiles.push_back({"converted_number_sum", [](Plan& p) {
                             SmoothInteger a, b, c;
                             a.setMetricsPtr(p.metricsPtr());
                             b.setMetricsPtr(p.metricsPtr());
                             c.setMetricsPtr(p.metricsPtr());
                             a.setValue(30LL);
                             b.setValue(45LL);
                             c.setValue(20LL);
                             return p.numberVia(a).plus().numberVia(b).plus().numberVia(c).calculate();
                         }});

    // 3 numbers, mixing numberVia() and scalar(): (price * quantity) +
    // shipping, showing the two leaf kinds combine freely.
    profiles.push_back({"converted_price_quantity_plus_shipping", [](Plan& p) {
                             SmoothInteger price, quantity;
                             price.setMetricsPtr(p.metricsPtr());
                             quantity.setMetricsPtr(p.metricsPtr());
                             price.setValue(18LL);
                             quantity.setValue(4LL);
                             return p.left()
                                 .numberVia(price)
                                 .times()
                                 .numberVia(quantity)
                                 .right()
                                 .plus()
                                 .scalar(6)
                                 .calculate();
                         }});

    return profiles;
}

// Every counter a Plan run can currently produce. convert_to_<name()> is
// Plan's own counter, incremented once per leaf. The
// convert_<representation>_to_<representation> counters belong to a
// fed-in SmoothNumberBase itself, always starting from Dynamic -- so
// convert_dynamic_to_<X> is reachable for every X except Dynamic (which
// MatrixPlan targets, needing no conversion at all).
const std::vector<std::string>& counterColumns() {
    static const std::vector<std::string> columns = {
        "convert_to_scalar",
        "convert_to_sparse",
        "convert_to_matrix",
        "convert_to_row_values",
        "convert_dynamic_to_sparse",
        "convert_dynamic_to_row_values",
        "convert_dynamic_to_scalar",
        "add",
        "multiply",
        "carries",
        "bit_operations",
        "scalar_operations",
        "bit_iterations",
    };
    return columns;
}

struct PlanKind {
    std::string name;
    std::function<std::unique_ptr<Plan>(std::shared_ptr<Metrics>)> make;
};

const std::vector<PlanKind>& planKinds() {
    static const std::vector<PlanKind> kinds = {
        {"scalar", [](std::shared_ptr<Metrics> m) { return std::make_unique<DefaultPlan>(std::move(m)); }},
        {"sparse", [](std::shared_ptr<Metrics> m) { return std::make_unique<SparsePlan>(std::move(m)); }},
        {"matrix", [](std::shared_ptr<Metrics> m) { return std::make_unique<MatrixPlan>(std::move(m)); }},
        {"row_values",
         [](std::shared_ptr<Metrics> m) { return std::make_unique<RowValuesPlan>(std::move(m)); }},
    };
    return kinds;
}

void fail(sqlite3* db, const std::string& context) {
    std::cerr << "sqlite error (" << context << "): " << sqlite3_errmsg(db) << "\n";
    std::exit(1);
}

void exec(sqlite3* db, const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string message = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        std::cerr << "sqlite error running: " << sql << "\n  " << message << "\n";
        std::exit(1);
    }
}

// Quotes an identifier for use as a column/table name, so counter names
// that happen to collide with a SQL keyword (e.g. "add") are never
// ambiguous.
std::string quoteIdent(const std::string& name) { return "\"" + name + "\""; }

void createTableIfNeeded(sqlite3* db) {
    std::string sql = "CREATE TABLE IF NOT EXISTS results (\n"
                       "  plan_name TEXT NOT NULL,\n"
                       "  profile_name TEXT NOT NULL,\n"
                       "  result REAL NOT NULL,\n";
    for (const auto& column : counterColumns()) {
        sql += "  " + quoteIdent(column) + " INTEGER NOT NULL DEFAULT 0,\n";
    }
    sql += "  PRIMARY KEY (plan_name, profile_name)\n);";
    exec(db, sql);
}

// Refuses to run against a database whose results table doesn't have
// exactly the columns this build expects, rather than silently
// inserting into (or reading from) a schema that no longer matches.
void validateSchema(sqlite3* db) {
    std::set<std::string> expected = {"plan_name", "profile_name", "result"};
    for (const auto& column : counterColumns()) expected.insert(column);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "PRAGMA table_info(results);", -1, &stmt, nullptr) != SQLITE_OK) {
        fail(db, "PRAGMA table_info");
    }
    std::set<std::string> actual;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        actual.insert(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
    }
    sqlite3_finalize(stmt);

    if (actual != expected) {
        std::cerr << "The results table's columns don't match this build's counters.\n"
                      "This happens after a counter is added/removed/renamed in the library --\n"
                      "delete the database file and re-run to rebuild it from scratch.\n";
        std::exit(1);
    }
}

bool alreadyComputed(sqlite3* db, const std::string& planName, const std::string& profileName) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT 1 FROM results WHERE plan_name = ? AND profile_name = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) fail(db, "prepare SELECT");
    sqlite3_bind_text(stmt, 1, planName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, profileName.c_str(), -1, SQLITE_TRANSIENT);
    bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

void insertResult(sqlite3* db, const std::string& planName, const std::string& profileName, double result,
                   const Metrics& metrics) {
    std::string sql = "INSERT INTO results (plan_name, profile_name, result";
    for (const auto& column : counterColumns()) sql += ", " + quoteIdent(column);
    sql += ") VALUES (?, ?, ?";
    for (std::size_t i = 0; i < counterColumns().size(); ++i) sql += ", ?";
    sql += ");";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) fail(db, "prepare INSERT");
    sqlite3_bind_text(stmt, 1, planName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, profileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, result);
    int index = 4;
    for (const auto& column : counterColumns()) {
        sqlite3_bind_int64(stmt, index++, metrics.get(column));
    }
    if (sqlite3_step(stmt) != SQLITE_DONE) fail(db, "step INSERT");
    sqlite3_finalize(stmt);
}

}  // namespace

int main(int argc, char** argv) {
    std::string dbPath = argc > 1 ? argv[1] : "smooth_profiles.db";

    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Couldn't open database at " << dbPath << ": " << sqlite3_errmsg(db) << "\n";
        return 1;
    }

    createTableIfNeeded(db);
    validateSchema(db);

    int computed = 0;
    int skipped = 0;
    for (const auto& kind : planKinds()) {
        for (const auto& profile : allProfiles()) {
            if (alreadyComputed(db, kind.name, profile.name)) {
                ++skipped;
                continue;
            }
            auto metrics = std::make_shared<Metrics>();
            std::unique_ptr<Plan> plan = kind.make(metrics);
            double result = profile.run(*plan);
            insertResult(db, kind.name, profile.name, result, *metrics);
            std::cout << "computed " << kind.name << " / " << profile.name << " = " << result << "\n";
            ++computed;
        }
    }

    sqlite3_close(db);

    std::cout << "\n" << computed << " combo(s) computed, " << skipped << " already present in " << dbPath
              << ".\n";
    return 0;
}
