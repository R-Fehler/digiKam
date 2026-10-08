/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - memory budget policy of the decoded
 *               thumbnail cache (pure functions, unit tested).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QtGlobal>

namespace Digikam
{

namespace PhotosCachePolicy
{

static constexpr qint64 MiB = 1024;              ///< Sizes below are in KiB.
static constexpr qint64 GiB = 1024 * MiB;

/**
 * Normal budget: 20% of the physical memory, between 512 MiB and 8 GiB.
 * Unknown memory size (0): 512 MiB.
 */
inline qint64 defaultBudgetKiB(qint64 totalKiB)
{
    if (totalKiB <= 0)
    {
        return 512 * MiB;
    }

    return qBound(512 * MiB, totalKiB / 5, 8 * GiB);
}

/**
 * Budget of the full screen preview cache: 1/16 of the physical memory,
 * between 256 MiB and 2 GiB (a 4K preview is about 32 MiB). Unknown memory
 * size (0): 256 MiB.
 */
inline qint64 previewBudgetKiB(qint64 totalKiB)
{
    if (totalKiB <= 0)
    {
        return 256 * MiB;
    }

    return qBound(256 * MiB, totalKiB / 16, 2 * GiB);
}

/// Memory the system should keep available: max(1 GiB, 10% of RAM).
inline qint64 lowWaterKiB(qint64 totalKiB)
{
    return qMax(1 * GiB, totalKiB / 10);
}

/**
 * Next maximum cost of the cache, called periodically.
 *  - Below the low water mark, give back what is missing (the cache then
 *    evicts its least recently used entries), but keep at least 64 MiB.
 *  - Above twice the low water mark, grow back towards the normal budget
 *    by 1/8 of it per call.
 *  - In between, keep the current value (hysteresis).
 */
inline qint64 nextMaxCostKiB(qint64 maxCost, qint64 usedKiB, qint64 budgetKiB,
                             qint64 totalKiB, qint64 availableKiB)
{
    if ((totalKiB <= 0) || (availableKiB < 0))
    {
        return maxCost;
    }

    const qint64 low     = lowWaterKiB(totalKiB);
    const qint64 minimum = 64 * MiB;

    if (availableKiB < low)
    {
        return qMax(minimum, qMin(maxCost, usedKiB) - (low - availableKiB));
    }

    if ((availableKiB > 2 * low) && (maxCost < budgetKiB))
    {
        return qMin(budgetKiB, maxCost + budgetKiB / 8);
    }

    return maxCost;
}

} // namespace PhotosCachePolicy

} // namespace Digikam
