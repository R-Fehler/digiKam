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
#include <QFileInfo>
#include <QMenuBar>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QStatusBar>
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
