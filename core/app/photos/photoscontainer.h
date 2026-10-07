/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - central widget hosting the Qt Quick
 *               Photos UI next to the classic digiKam view.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QStackedWidget>
#include <QString>

class QAction;
class QQuickWidget;

namespace Digikam
{

class DigikamApp;
class ItemIconView;
class PhotosLibraryModel;
class PhotosGridModel;
class PhotosThumbnailBroker;

class PhotosContainer : public QStackedWidget
{
    Q_OBJECT

    Q_PROPERTY(bool photosActive READ photosActive NOTIFY signalPhotosActiveChanged)

public:

    explicit PhotosContainer(DigikamApp* const app, ItemIconView* const classicView);
    ~PhotosContainer() override;

    bool     photosActive() const;
    void     setPhotosActive(bool active);
    QAction* toggleAction() const;

    // --- Helpers callable from QML ("photosApp") ---

    Q_INVOKABLE void switchToClassic();
    Q_INVOKABLE void openContainingFolder(const QString& filePath);
    Q_INVOKABLE void openExternally(const QString& filePath);

Q_SIGNALS:

    void signalPhotosActiveChanged();

private Q_SLOTS:

    void slotToggle();

private:

    void applyChrome(bool photos);

private:

    DigikamApp*            m_app         = nullptr;
    ItemIconView*          m_classicView = nullptr;
    QQuickWidget*          m_quick       = nullptr;
    PhotosLibraryModel*    m_library     = nullptr;
    PhotosGridModel*       m_grid        = nullptr;
    PhotosThumbnailBroker* m_broker      = nullptr;
    QAction*               m_toggle      = nullptr;
};

} // namespace Digikam
