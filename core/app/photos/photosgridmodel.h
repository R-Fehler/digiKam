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

#pragma once

// Qt includes

#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace Digikam
{

class PhotosLibraryModel;

class PhotosGridModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int columns   READ columns  WRITE setColumns NOTIFY columnsChanged)
    Q_PROPERTY(bool byMonth  READ byMonth                   NOTIFY columnsChanged)

public:

    enum RowType
    {
        HeaderRow = 0,
        PhotoRow  = 1
    };
    Q_ENUM(RowType)

    enum Roles
    {
        RowTypeRole = Qt::UserRole + 1,
        FirstRole,
        CountRole,
        TitleRole
    };

    /// Above this number of columns, sections are months instead of days.
    static const int MonthSectionsFromColumns = 8;

public:

    explicit PhotosGridModel(PhotosLibraryModel* const library, QObject* const parent = nullptr);
    ~PhotosGridModel() override = default;

    int                    rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant               data(const QModelIndex& index, int role = Qt::DisplayRole)   const override;
    QHash<int, QByteArray> roleNames()                                                   const override;

    int  columns() const;
    void setColumns(int columns);
    bool byMonth() const;

    /// Grid row containing the given library row (photo index), or -1.
    Q_INVOKABLE int rowForPhoto(int photoIndex) const;

    /// First photo index shown in the given grid row (header rows map to their first photo).
    Q_INVOKABLE int photoForRow(int row)        const;

Q_SIGNALS:

    void columnsChanged();

private Q_SLOTS:

    void rebuild();
    void slotLibraryDataChanged();

private:

    struct Row
    {
        int     type  = PhotoRow;
        int     first = 0;
        int     count = 0;
        QString title;
    };

    PhotosLibraryModel* m_library = nullptr;
    QList<Row>          m_rows;
    int                 m_columns = 5;
};

} // namespace Digikam
