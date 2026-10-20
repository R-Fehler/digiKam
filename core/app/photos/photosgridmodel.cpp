/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - splits the library into date sections
 *               and rows of square tiles for the Qt Quick grid.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photosgridmodel.h"

// C++ includes

#include <algorithm>

// Qt includes

#include <QDate>
#include <QLocale>

// KDE includes

#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>

// Local includes

#include "photoslibrarymodel.h"

namespace Digikam
{

namespace
{

QString dayTitle(const QDate& date)
{
    if (!date.isValid())
    {
        return i18n("Unknown date");
    }

    const QDate today = QDate::currentDate();

    if (date == today)
    {
        return i18n("Today");
    }

    if (date == today.addDays(-1))
    {
        return i18n("Yesterday");
    }

    const QLocale locale;

    if (date.year() == today.year())
    {
        return locale.toString(date, QLatin1String("ddd d MMM"));
    }

    return locale.toString(date, QLatin1String("ddd d MMM yyyy"));
}

QString monthTitle(const QDate& date)
{
    if (!date.isValid())
    {
        return i18n("Unknown date");
    }

    return QLocale().toString(date, QLatin1String("MMMM yyyy"));
}

} // namespace

PhotosGridModel::PhotosGridModel(PhotosLibraryModel* const library, QObject* const parent)
    : QAbstractListModel(parent),
      m_library         (library)
{
    // Photos mode runs with its own main configuration file (see PhotosMode).

    m_columns = qBound(1, KSharedConfig::openConfig()->group(QLatin1String("Photos Mode"))
                                                       .readEntry(QLatin1String("Columns"), 5), 40);

    connect(m_library, &QAbstractItemModel::modelReset,
            this, &PhotosGridModel::rebuild);

    connect(m_library, &QAbstractItemModel::dataChanged,
            this, &PhotosGridModel::slotLibraryDataChanged);

    rebuild();
}

int PhotosGridModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant PhotosGridModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (index.row() >= m_rows.size()))
    {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());

    switch (role)
    {
        case RowTypeRole:
            return row.type;

        case FirstRole:
            return row.first;

        case CountRole:
            return row.count;

        case Qt::DisplayRole:
        case TitleRole:
            return row.title;

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> PhotosGridModel::roleNames() const
{
    return {
             { RowTypeRole, "rowType" },
             { FirstRole,   "first"   },
             { CountRole,   "count"   },
             { TitleRole,   "title"   }
           };
}

int PhotosGridModel::columns() const
{
    return m_columns;
}

void PhotosGridModel::setColumns(int columns)
{
    columns = qBound(1, columns, 40);

    if (columns == m_columns)
    {
        return;
    }

    m_columns = columns;
    rebuild();

    KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String("Photos Mode"));
    group.writeEntry(QLatin1String("Columns"), m_columns);
    group.sync();

    Q_EMIT columnsChanged();
}

bool PhotosGridModel::byMonth() const
{
    return (m_columns >= MonthSectionsFromColumns);
}

void PhotosGridModel::rebuild()
{
    beginResetModel();

    m_rows.clear();

    const QList<PhotosEntry>& entries = m_library->entries();
    const bool monthly                = byMonth();
    int  sectionKey                   = -1;
    bool firstSection                 = true;
    int  i                            = 0;

    while (i < entries.size())
    {
        const QDate date = entries.at(i).dateTime.date();
        const int key    = !date.isValid() ? 0
                                           : (monthly ? (date.year() * 12 + date.month())
                                                      : int(date.toJulianDay()));

        if (firstSection || (key != sectionKey))
        {
            Row header;
            header.type  = HeaderRow;
            header.first = i;
            header.title = monthly ? monthTitle(date) : dayTitle(date);
            m_rows << header;

            sectionKey   = key;
            firstSection = false;
        }

        // Fill one row of tiles, stopping at the end of the section.

        Row row;
        row.type  = PhotoRow;
        row.first = i;

        while ((i < entries.size()) && (row.count < m_columns))
        {
            const QDate d = entries.at(i).dateTime.date();
            const int k   = !d.isValid() ? 0
                                         : (monthly ? (d.year() * 12 + d.month())
                                                    : int(d.toJulianDay()));

            if (k != sectionKey)
            {
                break;
            }

            ++row.count;
            ++i;
        }

        m_rows << row;
    }

    endResetModel();
}

void PhotosGridModel::slotLibraryDataChanged()
{
    // Only per-photo flags (e.g. favorite) changed: tiles bind directly
    // to the library model, so the row layout stays valid.
}

int PhotosGridModel::rowForPhoto(int photoIndex) const
{
    if ((photoIndex < 0) || m_rows.isEmpty())
    {
        return -1;
    }

    // Rows are sorted by first photo index. Find the last photo row
    // whose first index is <= photoIndex.

    auto it = std::upper_bound(m_rows.constBegin(), m_rows.constEnd(), photoIndex,
                               [] (int value, const Row& row)
                               {
                                   return (value < row.first);
                               });

    while (it != m_rows.constBegin())
    {
        --it;

        if (it->type == PhotoRow)
        {
            return int(it - m_rows.constBegin());
        }
    }

    return 0;
}

int PhotosGridModel::photoForRow(int row) const
{
    if ((row < 0) || (row >= m_rows.size()))
    {
        return -1;
    }

    return m_rows.at(row).first;
}

int PhotosGridModel::photoAt(int row, int column) const
{
    if ((row < 0) || (row >= m_rows.size()) || (column < 0))
    {
        return -1;
    }

    const Row& r = m_rows.at(row);

    if ((r.type != PhotoRow) || (column >= r.count))
    {
        return -1;
    }

    return (r.first + column);
}

QList<int> PhotosGridModel::photosInBlock(int rowA, int rowB, int columnA, int columnB) const
{
    QList<int> photos;

    if (m_rows.isEmpty())
    {
        return photos;
    }

    const int r0 = qBound(0, qMin(rowA, rowB), int(m_rows.size()) - 1);
    const int r1 = qBound(0, qMax(rowA, rowB), int(m_rows.size()) - 1);
    const int c0 = qMax(0, qMin(columnA, columnB));
    const int c1 = qMin(m_columns - 1, qMax(columnA, columnB));

    for (int r = r0 ; r <= r1 ; ++r)
    {
        const Row& row = m_rows.at(r);

        if (row.type != PhotoRow)
        {
            continue;
        }

        for (int c = c0 ; (c <= c1) && (c < row.count) ; ++c)
        {
            photos << (row.first + c);
        }
    }

    return photos;
}

} // namespace Digikam
