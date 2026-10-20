// SPDX-License-Identifier: GPL-2.0-or-later
// Unit test of the Photos mode thumbnail cache budget policy (Qt Core only).
//   g++ -std=c++17 -fPIC -I core/app/photos $(pkg-config --cflags Qt6Core) \
//       streamline/tests/cachepolicy_test.cpp $(pkg-config --libs Qt6Core) -o cachepolicy_test

#include <cstdio>
#include "photoscachepolicy.h"

using namespace Digikam::PhotosCachePolicy;

static int failures = 0;

static void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    failures += ok ? 0 : 1;
}

int main()
{
    // Budgets: 20% of RAM within [512 MiB, 8 GiB].
    check(defaultBudgetKiB(0)        == 512 * MiB, "unknown memory: 512 MiB");
    check(defaultBudgetKiB(2 * GiB)  == 512 * MiB, "2 GiB RAM: lower bound 512 MiB");
    check(defaultBudgetKiB(8 * GiB)  == 8 * GiB / 5, "8 GiB RAM: 1.6 GiB");
    check(defaultBudgetKiB(16 * GiB) == 16 * GiB / 5, "16 GiB RAM: 3.2 GiB");
    check(defaultBudgetKiB(64 * GiB) == 8 * GiB, "64 GiB RAM: upper bound 8 GiB");

    const qint64 total  = 16 * GiB;          // low water mark: 1.6 GiB
    const qint64 budget = defaultBudgetKiB(total);

    // Plenty of memory: unchanged.
    check(nextMaxCostKiB(budget, 1 * GiB, budget, total, 10 * GiB) == budget, "plenty of memory: unchanged");

    // 1.0 GiB available, 0.6 GiB missing: the cache gives it back from what it uses.
    const qint64 shrunk = nextMaxCostKiB(budget, 2 * GiB, budget, total, 1 * GiB);
    check(shrunk == 2 * GiB - (total / 10 - 1 * GiB), "low memory: shrink by the missing amount");

    // Very low memory: never below 64 MiB.
    check(nextMaxCostKiB(budget, 100 * MiB, budget, total, 10 * MiB) == 64 * MiB, "very low memory: 64 MiB floor");

    // Between low and 2x low: keep (hysteresis).
    check(nextMaxCostKiB(shrunk, shrunk, budget, total, 2 * GiB) == shrunk, "recovering: hysteresis keeps value");

    // Above 2x low: grow by budget/8 per call, up to the budget.
    qint64 cost = shrunk;
    int steps   = 0;

    while ((cost < budget) && (steps < 100))
    {
        cost = nextMaxCostKiB(cost, cost, budget, total, 8 * GiB);
        ++steps;
    }

    check((cost == budget) && (steps <= 8), "plenty again: back to the budget in at most 8 steps");

    // Unknown memory figures: unchanged.
    check(nextMaxCostKiB(budget, 1 * GiB, budget, 0, -1) == budget, "unknown memory: unchanged");

    // Preview cache (full screen viewer).

    check(previewBudgetKiB(0)         == 256 * MiB,  "previews, unknown memory: 256 MiB");
    check(previewBudgetKiB(2 * GiB)   == 256 * MiB,  "previews, 2 GiB RAM: lower bound 256 MiB");
    check(previewBudgetKiB(16 * GiB)  == 1 * GiB,    "previews, 16 GiB RAM: 1 GiB");
    check(previewBudgetKiB(64 * GiB)  == 2 * GiB,    "previews, 64 GiB RAM: upper bound 2 GiB");

    std::printf("%s\n", failures ? "FAILED" : "all passed");

    return failures ? 1 : 0;
}
