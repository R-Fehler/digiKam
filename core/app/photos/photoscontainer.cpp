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
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QKeyEvent>
#include <QMenuBar>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

// KDE includes

#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>
#include <ktoolbar.h>

// Local includes

#include "digikam_debug.h"
#include "digikamapp.h"
#include "itemiconview.h"
#include "dfileoperations.h"
#include "photoslibrarymodel.h"
#include "photosgridmodel.h"
#include "photosimageproviders.h"
#include "photosgestures.h"
#include "photosmetadata.h"
#include "photosimporter.h"
#include "photoslibraries.h"
#include "photosmode.h"
#include "metaenginesettings.h"
#include "metaenginesettingscontainer.h"
#include "thumbnailinfo.h"

namespace Digikam
{

namespace
{

class PhotosFileOpenFilter : public QObject
{
public:

    explicit PhotosFileOpenFilter(PhotosContainer* const container)
        : QObject    (container),
          m_container(container)
    {
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if ((watched == qApp) && (event->type() == QEvent::FileOpen))
        {
            const QFileOpenEvent* const open = static_cast<QFileOpenEvent*>(event);
            m_container->openPaths(QStringList() << (open->file().isEmpty() ? open->url().toLocalFile()
                                                                              : open->file()));

            return true;
        }

        return QObject::eventFilter(watched, event);
    }

private:

    PhotosContainer* m_container = nullptr;
};

} // namespace

PhotosContainer::PhotosContainer(DigikamApp* const app, ItemIconView* const classicView)
    : QStackedWidget(app),
      m_app        (app),
      m_classicView(classicView)
{
    setObjectName(QLatin1String("PhotosContainer"));

    m_broker   = new PhotosThumbnailBroker(this);
    m_previews = new PhotosPreviewLoader(this);
    m_library = new PhotosLibraryModel(this);
    m_grid    = new PhotosGridModel(m_library, this);
    m_importer  = new PhotosImporter(m_library, this);
    m_libraries = new PhotosLibraries(this);

    // Read by the Qt Quick scene as soon as it loads.

    m_sidecarVisibility = new PhotosSidecarVisibility(this);

    m_filmstrip = KSharedConfig::openConfig()->group(QLatin1String("Photos Mode"))
                                             .readEntry("Filmstrip", true);

    m_quick   = new QQuickWidget(this);
    m_quick->setObjectName(QLatin1String("PhotosQuickWidget"));
    m_quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quick->setClearColor(palette().color(QPalette::Window));
    m_quick->setFocusPolicy(Qt::StrongFocus);
    m_quick->installEventFilter(this);

    QQmlEngine* const engine = m_quick->engine();

    // The engine takes ownership of the providers.

    engine->addImageProvider(QLatin1String("dkthumb"),   new PhotosThumbnailProvider(m_broker));
    engine->addImageProvider(QLatin1String("dkpreview"), new PhotosPreviewProvider(m_previews));

    QQmlContext* const context = m_quick->rootContext();
    context->setContextProperty(QLatin1String("library"),   m_library);
    context->setContextProperty(QLatin1String("grid"),      m_grid);
    context->setContextProperty(QLatin1String("photosApp"), this);
    context->setContextProperty(QLatin1String("importer"),  m_importer);
    context->setContextProperty(QLatin1String("libraries"), m_libraries);

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

    m_library->setDefaultThumbnailSize(m_broker->sizes().constLast());

    connect(m_broker, &PhotosThumbnailBroker::signalPregenerationProgress,
            this, [this] (int done, int total)
        {
            const int percent = ((total > 0) && (done < total)) ? (100 * done / total) : -1;

            if (percent != m_preparingPercent)
            {
                m_preparingPercent = percent;

                Q_EMIT signalPreparingChanged();
            }
        }
    );

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

    // Information kept only in the database so far goes to sidecars, once,
    // after the start (see PhotosSidecarSync).

    m_sidecarSync = new PhotosSidecarSync(this);

    connect(m_sidecarSync, &PhotosSidecarSync::signalProgressChanged,
            this, &PhotosContainer::signalSidecarSyncChanged);

    connect(m_sidecarVisibility, &PhotosSidecarVisibility::signalHiddenChanged,
            this, &PhotosContainer::signalHideSidecarsChanged);

    connect(MetaEngineSettings::instance(), &MetaEngineSettings::signalSettingsChanged,
            this, &PhotosContainer::signalSidecarsChanged);

    QTimer::singleShot(10000, m_sidecarSync, &PhotosSidecarSync::startIfPending);

    // Like "code <folder>": later "digikam --photos <path>" calls hand their
    // paths over to this window (PhotosMode::forwardToRunningInstance()).

    QLocalServer* const server = new QLocalServer(this);
    server->setSocketOptions(QLocalServer::UserAccessOption);

    if (!server->listen(PhotosMode::instanceServerName()))
    {
        // A stale socket of a crashed instance.

        QLocalServer::removeServer(PhotosMode::instanceServerName());
        server->listen(PhotosMode::instanceServerName());
    }

    connect(server, &QLocalServer::newConnection,
            this, &PhotosContainer::slotNewInstanceConnection);

    // macOS: folders and photos dropped on the Dock icon or opened from Finder.
    // A separate filter: an application filter sees the events of all objects.

    qApp->installEventFilter(new PhotosFileOpenFilter(this));


    setCurrentWidget(m_quick);
    m_library->reload();
}

void PhotosContainer::slotNewInstanceConnection()
{
    QLocalServer* const server = qobject_cast<QLocalServer*>(sender());

    while (server && server->hasPendingConnections())
    {
        QLocalSocket* const socket = server->nextPendingConnection();

        connect(socket, &QLocalSocket::disconnected,
                socket, &QObject::deleteLater);

        connect(socket, &QLocalSocket::readyRead,
                this, [this, socket] ()
            {
                while (socket->canReadLine())
                {
                    const QJsonObject message = QJsonDocument::fromJson(socket->readLine()).object();
                    QStringList paths;

                    for (const QJsonValue& path : message.value(QLatin1String("open")).toArray())
                    {
                        paths << path.toString();
                    }

                    openPaths(paths);
                }
            }
        );
    }
}

void PhotosContainer::openStartupPaths()
{
    // Paths of this process' command line: the window is just being shown.

    m_windowReady            = true;
    const QStringList paths  = PhotosMode::takeStartupPaths() + m_pendingPaths;
    m_pendingPaths.clear();

    if (!paths.isEmpty())
    {
        Q_EMIT openRequested(m_libraries->checkPath(paths.constFirst()));
    }
}

void PhotosContainer::openPaths(const QStringList& paths)
{
    // Another instance started while this one is still starting: later.

    if (!m_windowReady)
    {
        m_pendingPaths << paths;

        return;
    }

    // To the front, as a second "code" call does.

    if (m_app)
    {
        if (m_app->isMinimized())
        {
            m_app->showNormal();
        }

        m_app->show();
        m_app->raise();
        m_app->activateWindow();
    }

    if (!photosActive())
    {
        setPhotosActive(true);
    }

    if (paths.isEmpty())
    {
        return;
    }

    // One window, one view: the first path is shown.

    Q_EMIT openRequested(m_libraries->checkPath(paths.constFirst()));
}

bool PhotosContainer::sidecars() const
{
    return PhotosMetadata::sidecarMode(MetaEngineSettings::instance()->settings());
}

void PhotosContainer::setSidecars(bool sidecars)
{
    if (sidecars == this->sidecars())
    {
        return;
    }

    MetaEngineSettingsContainer settings = MetaEngineSettings::instance()->settings();
    PhotosMetadata::setSidecarMode(settings, sidecars);
    MetaEngineSettings::instance()->setSettings(settings);
    KSharedConfig::openConfig()->sync();

    if (sidecars)
    {
        m_sidecarSync->restart();
    }
}

bool PhotosContainer::hideSidecars() const
{
    return m_sidecarVisibility->hidden();
}

void PhotosContainer::setHideSidecars(bool hide)
{
    m_sidecarVisibility->setHidden(hide);
}

int PhotosContainer::sidecarSyncPercent() const
{
    return m_sidecarSync ? m_sidecarSync->progress() : -1;
}

PhotosContainer::~PhotosContainer()
{
    // Destroy the Qt Quick scene before the models it references.

    delete m_quick;
    m_quick = nullptr;
}

QVariantList PhotosContainer::thumbnailSizes() const
{
    QVariantList sizes;

    for (const int size : m_broker->sizes())
    {
        sizes << size;
    }

    return sizes;
}

int PhotosContainer::preparingPercent() const
{
    return m_preparingPercent;
}

bool PhotosContainer::filmstrip() const
{
    return m_filmstrip;
}

void PhotosContainer::setFilmstrip(bool show)
{
    if (show == m_filmstrip)
    {
        return;
    }

    m_filmstrip = show;

    KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String("Photos Mode"));
    group.writeEntry("Filmstrip", show);
    group.sync();

    Q_EMIT signalFilmstripChanged();
}

bool PhotosContainer::multiTouch() const
{
    return m_multiTouch;
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
    // Touchpad pinch: see photosForwardNativeGesture().

    if ((watched == m_quick) && photosForwardNativeGesture(m_quick, event))
    {
        return true;
    }

    // Touch screen pinch: see photosTouchPointCount(), PhotosTouchPinch and
    // photosDeliverTouch(). The events still reach the Qt Quick scene (taps,
    // scrolling...).

    const int touchPoints = ((watched == m_quick) && !m_deliveringTouch) ? photosTouchPointCount(event) : -1;

    if (touchPoints >= 0)
    {
        if ((touchPoints >= 2) != m_multiTouch)
        {
            m_multiTouch = (touchPoints >= 2);

            Q_EMIT signalMultiTouchChanged();
        }

        const PhotosTouchPinch::Step step = m_touchPinch.handle(event);

        switch (step.phase)
        {
            case PhotosTouchPinch::Started:
            {
                Q_EMIT touchPinchStarted(step.center.x(), step.center.y());
                break;
            }

            case PhotosTouchPinch::Updated:
            {
                Q_EMIT touchPinchUpdated(step.scale, step.center.x(), step.center.y());
                break;
            }

            case PhotosTouchPinch::Finished:
            {
                Q_EMIT touchPinchFinished();
                break;
            }

            default:
            {
                break;
            }
        }
    }

    if ((watched == m_quick) && photosDeliverTouch(m_quick, event, &m_deliveringTouch))
    {
        return true;
    }

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

void PhotosContainer::prefetchPreviews(int row, int size)
{
    QStringList filePaths;

    if ((row >= 0) && (size > 0))
    {
        // More ahead than behind: people mostly go forward.

        const int deltas[] = { +1, -1, +2, +3, -2 };

        for (const int delta : deltas)
        {
            const int neighbour = row + delta;

            if ((neighbour >= 0) && (neighbour < m_library->count()) && !m_library->isVideoAt(neighbour))
            {
                filePaths << m_library->filePathAt(neighbour);
            }
        }
    }

    m_previews->prefetch(filePaths, size);
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

void PhotosContainer::quitApplication()
{
    if (m_app)
    {
        m_app->close();
    }
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
