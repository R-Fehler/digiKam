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
#include <QVariantList>

// Local includes

#include "photosgestures.h"

class QAction;
class QTimer;
class QQuickWidget;

namespace Digikam
{

class DigikamApp;
class ItemIconView;
class PhotosLibraryModel;
class PhotosGridModel;
class PhotosThumbnailBroker;
class PhotosPreviewLoader;

class PhotosContainer : public QStackedWidget
{
    Q_OBJECT

    Q_PROPERTY(bool         photosActive     READ photosActive     NOTIFY signalPhotosActiveChanged)

    /// Thumbnail sizes served (ascending, device pixels): tiles pick the one matching their size.
    Q_PROPERTY(QVariantList thumbnailSizes   READ thumbnailSizes   CONSTANT)

    /// Background thumbnail generation progress in percent, or -1 when idle.
    Q_PROPERTY(int          preparingPercent READ preparingPercent NOTIFY signalPreparingChanged)

    /// Full screen viewer: show the strip of thumbnails at the bottom (saved setting).
    Q_PROPERTY(bool         filmstrip        READ filmstrip        WRITE setFilmstrip NOTIFY signalFilmstripChanged)

    /// Two or more fingers on the touch screen: lists stop scrolling, a pinch is going on.
    Q_PROPERTY(bool         multiTouch       READ multiTouch       NOTIFY signalMultiTouchChanged)

public:

    explicit PhotosContainer(DigikamApp* const app, ItemIconView* const classicView);
    ~PhotosContainer() override;

    bool     photosActive() const;
    void     setPhotosActive(bool active);
    QAction* toggleAction() const;

    QVariantList thumbnailSizes()   const;
    int          preparingPercent() const;
    bool         multiTouch()       const;
    bool         filmstrip()        const;
    void         setFilmstrip(bool show);

    // --- Helpers callable from QML ("photosApp") ---

    Q_INVOKABLE void switchToClassic();
    Q_INVOKABLE void openContainingFolder(const QString& filePath);
    Q_INVOKABLE void openExternally(const QString& filePath);

    /**
     * Full screen viewer on photo row: decode the previews (long side size) of
     * the neighbours ahead, next ones first. row < 0 drops the prefetches.
     */
    Q_INVOKABLE void prefetchPreviews(int row, int size);

Q_SIGNALS:

    void signalPhotosActiveChanged();
    void signalPreparingChanged();
    void signalMultiTouchChanged();
    void signalFilmstripChanged();

    /// Touch screen pinch (see PhotosTouchPinch): scale relative to the start,
    /// center in scene coordinates.
    void touchPinchStarted(qreal x, qreal y);
    void touchPinchUpdated(qreal scale, qreal x, qreal y);
    void touchPinchFinished();

protected:

    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:

    void slotToggle();
    void slotPregenerate();

private:

    void applyChrome(bool photos);

private:

    DigikamApp*            m_app         = nullptr;
    ItemIconView*          m_classicView = nullptr;
    QQuickWidget*          m_quick       = nullptr;
    PhotosLibraryModel*    m_library     = nullptr;
    PhotosGridModel*       m_grid        = nullptr;
    PhotosThumbnailBroker* m_broker      = nullptr;
    PhotosPreviewLoader*   m_previews    = nullptr;
    QAction*               m_toggle      = nullptr;
    QTimer*                m_pregenTimer = nullptr;
    int                    m_pregeneratedCount = -1;
    int                    m_preparingPercent  = -1;
    bool                   m_multiTouch        = false;
    bool                   m_filmstrip         = true;
    bool                   m_deliveringTouch   = false;
    PhotosTouchPinch       m_touchPinch;
};

} // namespace Digikam
