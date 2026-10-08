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

#include "photoscontainer.h"

// Qt includes

#include <QAction>
#include <QEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenuBar>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

// KDE includes

#include <klocalizedstring.h>
#include <ktoolbar.h>

// Local includes

#include "digikam_debug.h"
#include "digikamapp.h"
#include "itemiconview.h"
#include "dfileoperations.h"
#include "photoslibrarymodel.h"
#include "photosgridmodel.h"
#include "photosimageproviders.h"
#include "thumbnailinfo.h"

namespace Digikam
{

PhotosContainer::PhotosContainer(DigikamApp* const app, ItemIconView* const classicView)
    : QStackedWidget(app),
      m_app        (app),
      m_classicView(classicView)
{
    setObjectName(QLatin1String("PhotosContainer"));

    m_broker  = new PhotosThumbnailBroker(this);
    m_library = new PhotosLibraryModel(this);
    m_grid    = new PhotosGridModel(m_library, this);

    m_quick   = new QQuickWidget(this);
    m_quick->setObjectName(QLatin1String("PhotosQuickWidget"));
    m_quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quick->setClearColor(palette().color(QPalette::Window));
    m_quick->setFocusPolicy(Qt::StrongFocus);
    m_quick->installEventFilter(this);

    QQmlEngine* const engine = m_quick->engine();

    // The engine takes ownership of the providers.

    engine->addImageProvider(QLatin1String("dkthumb"),   new PhotosThumbnailProvider(m_broker));
    engine->addImageProvider(QLatin1String("dkpreview"), new PhotosPreviewProvider());

    QQmlContext* const context = m_quick->rootContext();
    context->setContextProperty(QLatin1String("library"),   m_library);
    context->setContextProperty(QLatin1String("grid"),      m_grid);
    context->setContextProperty(QLatin1String("photosApp"), this);

    m_quick->setSource(QUrl(QLatin1String("qrc:/photos/qml/Main.qml")));

    const auto errors = m_quick->errors();

    for (const QQmlError& error : errors)
    {
        qCWarning(DIGIKAM_GENERAL_LOG) << "Photos mode QML:" << error.toString();
    }

    addWidget(m_quick);
    addWidget(m_classicView);

    m_toggle = new QAction(this);
    m_toggle->setObjectName(QLatin1String("photos_toggle_interface"));
    m_toggle->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    m_toggle->setShortcutContext(Qt::ApplicationShortcut);

    connect(m_toggle, &QAction::triggered,
            this, &PhotosContainer::slotToggle);

    // Thumbnails of files modified on disk are requested again.

    connect(m_broker, &PhotosThumbnailBroker::signalThumbnailChanged,
            m_library, &PhotosLibraryModel::invalidateThumbnail);

    // Once the library is listed, generate missing thumbnails in the background,
    // so that scrolling through a library seen for the first time stays smooth.

    m_pregenTimer = new QTimer(this);
    m_pregenTimer->setSingleShot(true);
    m_pregenTimer->setInterval(3000);

    connect(m_pregenTimer, &QTimer::timeout,
            this, &PhotosContainer::slotPregenerate);

    connect(m_library, &PhotosLibraryModel::reloaded,
            this, [this] ()
        {
            if (
                (m_library->filter() == PhotosLibraryModel::Library) &&
                (m_library->count() != m_pregeneratedCount)
               )
            {
                m_pregenTimer->start();
            }
        }
    );

    setCurrentWidget(m_quick);
    m_library->reload();
}

PhotosContainer::~PhotosContainer()
{
    // Destroy the Qt Quick scene before the models it references.

    delete m_quick;
    m_quick = nullptr;
}

bool PhotosContainer::photosActive() const
{
    return (currentWidget() == m_quick);
}

QAction* PhotosContainer::toggleAction() const
{
    return m_toggle;
}

void PhotosContainer::setPhotosActive(bool active)
{
    setCurrentWidget(active ? static_cast<QWidget*>(m_quick)
                            : static_cast<QWidget*>(m_classicView));
    applyChrome(active);

    m_toggle->setText(active ? i18n("Switch to Classic Interface")
                             : i18n("Switch to Photos Interface"));

    if (active)
    {
        m_quick->setFocus();
    }

    Q_EMIT signalPhotosActiveChanged();
}

void PhotosContainer::applyChrome(bool photos)
{
    m_app->menuBar()->setVisible(!photos);
    m_app->statusBar()->setVisible(!photos);

    const auto bars = m_app->toolBars();

    for (KToolBar* const bar : bars)
    {
        bar->setVisible(!photos);
    }
}

bool PhotosContainer::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_quick) && (event->type() == QEvent::ShortcutOverride))
    {
        // The classic main window binds plain keys (Escape, arrows, +/-...) and
        // Ctrl +/- to its own actions. While the Photos UI has the focus, let
        // these keys reach the Qt Quick scene instead of triggering those actions.

        QKeyEvent* const keyEvent         = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers mods  = keyEvent->modifiers() & ~(Qt::KeypadModifier | Qt::ShiftModifier);
        const int key                     = keyEvent->key();
        const bool ctrlKey                = (key == Qt::Key_Plus) || (key == Qt::Key_Equal) ||
                                            (key == Qt::Key_Minus) || (key == Qt::Key_A);

        if ((mods == Qt::NoModifier) || ((mods == Qt::ControlModifier) && ctrlKey))
        {
            event->accept();

            return true;
        }
    }

    return QStackedWidget::eventFilter(watched, event);
}

void PhotosContainer::slotPregenerate()
{
    // Debug / benchmark switch.

    if (qEnvironmentVariableIsSet("DIGIKAM_PHOTOS_NO_PREGEN"))
    {
        return;
    }

    QList<ThumbnailIdentifier> identifiers;
    const auto& entries = m_library->entries();
    identifiers.reserve(entries.size());

    for (const PhotosEntry& entry : entries)
    {
        ThumbnailIdentifier identifier(entry.filePath);
        identifier.id = entry.id;
        identifiers << identifier;
    }

    m_pregeneratedCount = entries.size();
    m_broker->pregenerate(identifiers);
}

void PhotosContainer::slotToggle()
{
    setPhotosActive(!photosActive());
}

void PhotosContainer::switchToClassic()
{
    setPhotosActive(false);
}

void PhotosContainer::openContainingFolder(const QString& filePath)
{
    DFileOperations::openInFileManager(QList<QUrl>() << QUrl::fromLocalFile(filePath));
}

void PhotosContainer::openExternally(const QString& filePath)
{
    DFileOperations::openFilesWithDefaultApplication(QList<QUrl>() << QUrl::fromLocalFile(filePath));
}

} // namespace Digikam
