/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-02-26
 * Description : Upper widget in the search sidebar
 *
 * SPDX-FileCopyrightText: 2008-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchtabheader_p.h"

namespace Digikam
{

void SearchTabHeader::slotKeywordChanged()
{
    QString keywords = d->keywordEdit->getText();

    qCDebug(DIGIKAM_GENERAL_LOG) << "keywords changed to '" << keywords << "'";

    if ((d->oldKeywordContent == keywords) || (keywords.trimmed().isEmpty()))
    {
        qCDebug(DIGIKAM_GENERAL_LOG) << "same keywords as before, ignoring...";

        return;
    }
    else
    {
        d->oldKeywordContent = keywords;
    }

    setCurrentSearch(DatabaseSearch::KeywordSearch, queryFromKeywords(keywords));
    d->keywordEdit->setFocus();
}

void SearchTabHeader::slotKeywordChangedTimer()
{
    if (d->keywordEdit->autoSearchEnabled())
    {
        slotKeywordChanged();
    }
}

void SearchTabHeader::slotEditCurrentSearch()
{
    const SAlbum* const album  = AlbumManager::instance()->findSAlbum(SAlbum::getTemporaryTitle(DatabaseSearch::AdvancedSearch));
    SearchWindow* const window = searchWindow();

    if (album)
    {
        window->readSearch(album->id(), album->query());
    }
    else
    {
        window->reset();
    }

    window->show();
    window->raise();
}

void SearchTabHeader::slotSaveSearch()
{
    // Only applicable if:
    // 1. current album is Search View Current Album Save this album as a user names search album.
    // 2. user as processed a search before to save it.

    QString name = d->saveNameEdit->text();

    qCDebug(DIGIKAM_GENERAL_LOG) << "name = " << name;

    if (name.isEmpty() || !d->currentAlbum)
    {
        qCDebug(DIGIKAM_GENERAL_LOG) << "no current album, returning";

        // passive popup

        return;
    }

    const SAlbum* oldAlbum = AlbumManager::instance()->findSAlbum(name);

    while (oldAlbum)
    {
        QString label    = i18n("Search name already exists.\n"
                                "Please enter a new name:");
        bool ok;
        QString newTitle = QInputDialog::getText(this,
                                                 i18nc("@title:window", "Name Exists"),
                                                 label,
                                                 QLineEdit::Normal,
                                                 name,
                                                 &ok);

        if (!ok)
        {
            return;
        }

        name     = newTitle;
        oldAlbum = AlbumManager::instance()->findSAlbum(name);
    }

    // cppcheck-suppress constVariablePointer
    SAlbum* const newAlbum = AlbumManager::instance()->createSAlbum(name, d->currentAlbum->searchType(),
                                                                    d->currentAlbum->query());
    Q_EMIT searchShallBeSelected(QList<Album*>() << newAlbum);
}

void SearchTabHeader::slotStoredKeywordChanged()
{
    QString keywords = d->storedKeywordEdit->text();

    if (d->oldStoredKeywordContent == keywords)
    {
        return;
    }
    else
    {
        d->oldStoredKeywordContent = keywords;
    }

    if (d->currentAlbum)
    {
        AlbumManager::instance()->updateSAlbum(d->currentAlbum, queryFromKeywords(keywords));

        Q_EMIT searchShallBeSelected(QList<Album*>() << d->currentAlbum);
    }
}

void SearchTabHeader::slotEditStoredAdvancedSearch()
{
    if (d->currentAlbum)
    {
        SearchWindow* const window = searchWindow();
        window->reset();
        window->readSearch(d->currentAlbum->id(), d->currentAlbum->query());
        window->show();
        window->raise();
    }
}

void SearchTabHeader::slotAdvancedSearchEdited(int id, const QString& query)
{
    // if the user just pressed the button, but did not change anything in the window,
    // the search is effectively still a keyword search.
    // We go the hard way and check this case.

    KeywordSearchReader check(query);
    DatabaseSearch::Type type = (check.isSimpleKeywordSearch() ? DatabaseSearch::KeywordSearch
                                                               : DatabaseSearch::AdvancedSearch);

    if (id == -1)
    {
        setCurrentSearch(type, query);
    }
    else
    {
        SAlbum* const album = AlbumManager::instance()->findSAlbum(id);

        if (album)
        {
            AlbumManager::instance()->updateSAlbum(album, query, album->title(), type);

            Q_EMIT searchShallBeSelected(QList<Album*>() << album);
        }
    }
}

void SearchTabHeader::selectedSearchChanged(Album* a)
{
    SAlbum* album = dynamic_cast<SAlbum*>(a);

    // Signal from SearchFolderView that a search has been selected.
    // Don't check on d->currentAlbum == album, rather update status (which may have changed on same album)

    d->currentAlbum = album;

    qCDebug(DIGIKAM_GENERAL_LOG) << "changing to SAlbum " << album;

    if (!album)
    {
        d->lowerArea->setCurrentWidget(d->saveAsWidget);
        d->lowerArea->setEnabled(false);
    }
    else
    {
        d->lowerArea->setEnabled(true);

        if      (album->title() == SAlbum::getTemporaryTitle(DatabaseSearch::AdvancedSearch))
        {
            d->lowerArea->setCurrentWidget(d->saveAsWidget);

            if (album->isKeywordSearch())
            {
                d->keywordEdit->setText(keywordsFromQuery(album->query()));
                d->keywordEdit->showAdvancedSearch(false);
            }
            else
            {
                d->keywordEdit->showAdvancedSearch(true);
            }
        }
        else if (album->isKeywordSearch())
        {
            d->lowerArea->setCurrentWidget(d->editSimpleWidget);
            d->storedKeywordEditName->setAdjustedText(album->title());
            d->storedKeywordEdit->setText(keywordsFromQuery(album->query()));
            d->keywordEdit->showAdvancedSearch(false);
        }
        else
        {
            d->lowerArea->setCurrentWidget(d->editAdvancedWidget);
            d->storedAdvancedEditName->setAdjustedText(album->title());
            d->keywordEdit->showAdvancedSearch(false);
        }
    }
}

void SearchTabHeader::copySearch(SAlbum* album)         // cppcheck-suppress constParameterPointer
{
    if (!album)
    {
        return;
    }

    if (album->isAdvancedSearch())
    {
        SAlbum* const salbum = AlbumManager::instance()->findSAlbum(SAlbum::getTemporaryTitle(DatabaseSearch::AdvancedSearch));

        if (salbum)
        {
            AlbumManager::instance()->updateSAlbum(salbum, album->query());
            SearchWindow* const window = searchWindow();

            window->reset();
            window->readSearch(salbum->id(), salbum->query());
            window->show();
            window->raise();
        }
    }
}

void SearchTabHeader::editSearch(SAlbum* album)         // cppcheck-suppress constParameterPointer
{
    if (!album)
    {
        return;
    }

    if      (album->isAdvancedSearch())
    {
        SearchWindow* const window = searchWindow();
        window->reset();
        window->readSearch(album->id(), album->query());
        window->show();
        window->raise();
    }
    else if (album->isKeywordSearch())
    {
        d->storedKeywordEdit->selectAll();
    }
}

void SearchTabHeader::newKeywordSearch()
{
    d->keywordEdit->clear();
    d->keywordEdit->setFocus();
}

void SearchTabHeader::newAdvancedSearch()
{
    SearchWindow* const window = searchWindow();
    window->reset();
    window->show();
    window->raise();
}

} // namespace Digikam
