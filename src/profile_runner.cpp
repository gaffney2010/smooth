// Runs every (plan kind, profile) combination not already present in a
// SQLite database, and records the resulting Metrics counters. Re-running
// this is always safe and cheap: anything already in the database is
// skipped, so only newly added plan kinds or profiles actually do work.
//
// The set of counter columns is fixed (see kCounterColumns below) to
// whatever counters a Plan-driven run can currently produce. If a new
// counter is added to the library later, this program's schema check will
// refuse to run against an old database -- per the project's stated policy,
// the fix is to delete the database file and let it be recreated fresh,
// not to migrate it in place.
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
#include "smooth/profiles.hpp"

using namespace smooth;

namespace {

// Every counter a Plan (or plan_zoo Plan) run can currently produce.
// convert_to_<name()> is Plan::compileNode()'s own counter, incremented
// once per leaf regardless of its source. The
// convert_<representation>_to_<representation> counters instead belong to
// a fed-in SmoothNumberBase itself (SmoothNumberBase::ensure()): every Plan
// now forces a numberVia() leaf through its own target representation (see
// Plan::convertNumberLeaf()/targetRepresentation() in plan.hpp), and every
// number's canonical representation always starts out (and, for now,
// stays) Dynamic -- so convert_dynamic_to_<X> is reachable for every X
// except Dynamic itself (MatrixPlan's target *is* Dynamic, so ensure()
// short-circuits with nothing to convert, and convert_dynamic_to_dynamic
// can never fire).
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
// exactly the columns this build expects -- rather than silently inserting
// into (or reading from) a schema that no longer matches what the counters
// mean, which is exactly the "we added a counter" situation the project's
// stated policy (delete the database, start fresh) is meant to cover.
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
