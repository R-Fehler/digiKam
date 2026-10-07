/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2010-03-06
 * Description : sub class of QTreeWidget for drag-and-drop support
 *
 * SPDX-FileCopyrightText: 2010 by Michael G. Hansen <mike at mghansen dot de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QTreeWidget>
#include <QPersistentModelIndex>

// Local includes

#include "myimageitem.h"

class QMouseEvent;

Q_DECLARE_METATYPE(QTreeWidgetItem*)

class MyTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:

    explicit MyTreeWidget(QWidget* const parent = nullptr);
    ~MyTreeWidget()                                                  override;

protected:

    void startDrag(Qt::DropActions supportedActions)                 override;

    QMimeData*  mimeData(const QList<QTreeWidgetItem*>& items) const override;

    // cppcheck-suppress passedByValue
    virtual QMimeData* mimeData(const QModelIndexList items)   const;

private:

    class Private;
    Private* const d = nullptr;
};
