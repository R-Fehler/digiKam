/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-01-20
 * Description : User interface for searches
 *
 * SPDX-FileCopyrightText: 2008-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchwindow_p.h"

namespace Digikam
{

void SearchWindow::setupNlSearch()
{
    d->nlResolver = new SearchIntentResolver(&d->nlDictionary);

#ifdef HAVE_LLAMACPP

        if (SearchNlModelManager::isModelAvailable())
        {
            SearchLlamaBackend* const llamaBackend = new SearchLlamaBackend(this);
            llamaBackend->loadModel(SearchNlModelManager::defaultModelPath());
            d->nlBackend                           = llamaBackend;

            qCInfo(DIGIKAM_NLSEARCH_LOG) << "NL search: using llama.cpp backend";
        }
        else

#endif

        {
            d->nlBackend = new SearchMockBackend(this);
            d->nlBackend->loadModel(QString());

            qCInfo(DIGIKAM_NLSEARCH_LOG) << "NL search: using mock backend"
                                         << "(model not available or built without llama.cpp)";
        }

    d->nlEngine   = new SearchQueryEngine(this);
    d->nlEngine->setBackend(d->nlBackend);
    d->nlEngine->setPromptBuilder(&d->nlPromptBuilder);
    d->nlEngine->setParser(&d->nlParser);
    d->nlEngine->setResolver(d->nlResolver);
    d->nlEngine->setCache(&d->nlCache);

    // TODO: collection-aware prompt hints

    connect(d->nlEngine, &SearchQueryEngine::signalIntentReady,
            this, &SearchWindow::slotNlIntentReady);

    connect(d->nlEngine, &SearchQueryEngine::signalClarificationRequired,
            this, &SearchWindow::slotNlClarificationRequired);

    connect(d->nlEngine, &SearchQueryEngine::signalErrorOccurred,
            this, &SearchWindow::slotNlError);

    connect(d->nlEngine, &SearchQueryEngine::signalCancelled,
            this, &SearchWindow::slotNlCancelled);

    connect(d->nlBackend, &SearchLanguageBackend::signalModelLoadProgress,
            this, [this](int percent)
        {
            d->nlStatusLabel->setText(i18n("Loading the language model... %1%", percent));
            d->nlStatusLabel->show();
        }
    );

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup group      = config->group(QLatin1String("NL Search Settings"));
    d->nlTranslate          = group.readEntry(QLatin1String("Translate Queries"), false);

    if (d->translateCheck)
    {
        d->translateCheck->setChecked(d->nlTranslate);
    }
}

SearchQueryEngine* SearchWindow::queryEngine() const
{
    return d->nlEngine;
}

void SearchWindow::startInterpretation(const QString& text)
{
    d->pendingNlText  = text;

    d->nlStatusLabel->setAutoFillBackground(false);
    d->nlStatusLabel->setPalette(QPalette());
    d->nlStatusLabel->setText(i18n("Interpreting the search..."));
    d->nlStatusLabel->show();
    d->nlSpinnerIndex = 0;
    d->nlSpinnerTimer->start(100);
    d->nlSpinnerLabel->show();
    d->nlCancelButton->setEnabled(true);

    if (!d->nlTranslate)
    {
        // Translation disabled: pass the original text straight through.
        // Empty normalizedText makes the engine reuse the original.

        d->nlEngine->slotInterpretQuery(text, QString());

        return;
    }

    // Translation enabled: normalize the query to English first, then
    // interpret. On failure we fall back to the original in the finished slot.

    if (!d->nlTranslator)
    {
        d->nlTranslator = new DOnlineTranslator(this);

        connect(d->nlTranslator, &DOnlineTranslator::signalFinished,
                this, &SearchWindow::slotTranslationFinished);
    }

    d->nlTranslator->translate(text,
                               DOnlineTranslator::Google,
                               DOnlineTranslator::English,   // translate INTO English
                               DOnlineTranslator::Auto);     // auto-detect source
}

void SearchWindow::applyResolvedIntent(const SearchQueryIntent& intent)
{
    const QString xml = intentToXml(intent);

    if (xml.contains(QLatin1String("<group/>")))
    {
        d->nlSpinnerTimer->stop();
        d->nlCancelButton->setEnabled(false);
        d->nlSpinnerLabel->setPixmap(QPixmap());

        d->nlStatusLabel->setText(i18nc("@info", "None of that could be turned into "
                                                 "a search. Try describing tags, dates, "
                                                 "ratings, labels, people, or places."));

        QPalette pal = d->nlStatusLabel->palette();
        pal.setColor(QPalette::Active, QPalette::Window, QColor(220, 140, 140));
        pal.setColor(QPalette::Active, QPalette::WindowText, Qt::black);
        d->nlStatusLabel->setPalette(pal);
        d->nlStatusLabel->setAutoFillBackground(true);

        d->nlStatusLabel->show();

        return;
    }

    d->nlStatusLabel->setAutoFillBackground(false);   // clear highlight on success
    d->nlStatusLabel->setPalette(QPalette());         // restore default text/background colours
    d->nlStatusLabel->hide();
    d->searchView->read(xml);
    d->nlCommittedXml = xml;
}

QString SearchWindow::intentToXml(const SearchQueryIntent& intent) const
{
    SearchXmlWriter writer;
    writer.writeGroup();

    for (const SearchQueryConstraint& c : intent.constraints)
    {
        writeConstraintToXml(writer, c);
    }

    writer.finishGroup();
    writer.finish();

    const QString xml = writer.xml();

    return xml;
}

void SearchWindow::writeConstraintToXml(SearchXmlWriter& writer,
                                        const SearchQueryConstraint& c) const
{
    if      (c.field == QLatin1String("tag"))
    {
        QList<int> ids = TagsCache::instance()->tagsForName(c.value);

        if (ids.isEmpty())
        {
            // The model lowercases values, but tag names are stored with
            // their original case and tagsForName() matches exactly.
            // tagsContaining() matches case-insensitively, so use it to
            // recover the tag when only the case differs.

            const QList<int> candidates = TagsCache::instance()->tagsContaining(c.value);

            for (int id : candidates)
            {
                const QString name = TagsCache::instance()->tagName(id);

                if (name.compare(c.value, Qt::CaseInsensitive) == 0)
                {
                    ids << id;
                }
            }
        }

        for (int id : std::as_const(ids))
        {
            writer.writeField(QLatin1String("tagid"), SearchXml::Equal);
            writer.writeValue(id);
            writer.finishField();
        }

        // If still empty: tag not in collection, emit nothing.
    }
    else if (c.field == QLatin1String("colorlabel"))
    {
        static const QMap<QString, int> nameToEnum =
        {
            { QLatin1String("none"),    NoColorLabel },
            { QLatin1String("red"),     RedLabel     },
            { QLatin1String("orange"),  OrangeLabel  },
            { QLatin1String("yellow"),  YellowLabel  },
            { QLatin1String("green"),   GreenLabel   },
            { QLatin1String("blue"),    BlueLabel    },
            { QLatin1String("magenta"), MagentaLabel },
            { QLatin1String("gray"),    GrayLabel    },
            { QLatin1String("black"),   BlackLabel   },
            { QLatin1String("white"),   WhiteLabel   },
        };

        const int labelEnum = nameToEnum.value(c.value.toLower(), -1);

        if (labelEnum >= 0)
        {
            const int labelTagId = TagsCache::instance()->tagForColorLabel(labelEnum);

            if (labelTagId)
            {
                writer.writeField(QLatin1String("labels"), SearchXml::InTree);
                writer.writeValue(labelTagId);
                writer.finishField();
            }
        }
    }
    else if ((c.field == QLatin1String("keyword")) || (c.field == QLatin1String("caption")))
    {
        writer.writeField(QLatin1String("keyword"), SearchXml::Like);
        writer.writeValue(c.value);
        writer.finishField();
    }
    else if (c.field == QLatin1String("daterange"))
    {
        const QStringList parts = c.value.split(QLatin1String(".."));

        if (parts.size() == 2)
        {
            const QDateTime start = QDateTime::fromString(parts.at(0), QLatin1String("yyyy-MM-dd"));
            const QDateTime end   = QDateTime::fromString(parts.at(1), QLatin1String("yyyy-MM-dd"));

            if (start.isValid() && end.isValid())
            {
                writer.writeField(QLatin1String("creationdate"), SearchXml::Interval);
                writer.writeValue(QList<QDateTime>() << start << end);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("rating"))
    {
        SearchXml::Relation rel = SearchXml::Equal;

        if      (c.op == QLatin1String("gte"))
        {
            rel = SearchXml::GreaterThanOrEqual;
        }
        else if (c.op == QLatin1String("lte"))
        {
            rel = SearchXml::LessThanOrEqual;
        }

        writer.writeField(QLatin1String("rating"), rel);
        writer.writeValue(c.value.toInt());
        writer.finishField();
    }
    else if (c.field == QLatin1String("picklabel"))
    {
        static const QMap<QString, int> nameToEnum =
        {
            { QLatin1String("none"),     NoPickLabel   },
            { QLatin1String("rejected"), RejectedLabel },
            { QLatin1String("pending"),  PendingLabel  },
            { QLatin1String("accepted"), AcceptedLabel },
        };

        const int labelEnum = nameToEnum.value(c.value.toLower(), -1);

        if (labelEnum >= 0)
        {
            const int labelTagId = TagsCache::instance()->tagForPickLabel(labelEnum);

            if (labelTagId)
            {
                writer.writeField(QLatin1String("labels"), SearchXml::InTree);
                writer.writeValue(labelTagId);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("person"))
    {
        QList<int> ids = TagsCache::instance()->tagsForName(c.value);

        if (ids.isEmpty())
        {
            // Same case-insensitive fallback as for tags: the model
            // lowercases values, but people tag names keep their original case.

            const QList<int> candidates = TagsCache::instance()->tagsContaining(c.value);

            for (int id : candidates)
            {
                const QString name = TagsCache::instance()->tagName(id);

                if (name.compare(c.value, Qt::CaseInsensitive) == 0)
                {
                    ids << id;
                }
            }
        }

        for (int id : std::as_const(ids))
        {
            writer.writeField(QLatin1String("tagid"), SearchXml::Equal);
            writer.writeValue(id);
            writer.finishField();
        }
    }
    else if (c.field == QLatin1String("album"))
    {
        writer.writeField(QLatin1String("albumname"), SearchXml::Like);
        writer.writeValue(c.value);
        writer.finishField();
    }
    else if (c.field == QLatin1String("orientation"))
    {
        // Codes come from SearchFieldPageOrientation: 1 = landscape, 2 = portrait.

        const QString v = c.value.toLower();
        int code        = -1;

        if      (v == QLatin1String("landscape"))
        {
            code = 1;
        }
        else if (v == QLatin1String("portrait"))
        {
            code = 2;
        }

        if (code > 0)
        {
            writer.writeField(QLatin1String("pageorientation"), SearchXml::Equal);
            writer.writeValue(code);
            writer.finishField();
        }
    }
    else if (c.field == QLatin1String("place"))
    {
        writer.writeGroup();
        writer.setDefaultFieldOperator(SearchXml::Or);

        const char* const locFields[] =
        {
            "city",
            "country",
            "location"
        };

        for (const char* const lf : locFields)
        {
            const QString field = QLatin1String(lf);

            // The model lowercases values, but location values are stored with
            // their original case. Match the value against the collection's
            // known values case-insensitively and emit the stored spelling.

            const QStringList known = CoreDbAccess().db()->getAllImagePropertiesByName(field);

            for (const QString& kv : known)
            {
                if (kv.compare(c.value, Qt::CaseInsensitive) == 0)
                {
                    writer.writeField(field, SearchXml::Equal);
                    writer.writeValue(kv);
                    writer.finishField();
                    break;
                }
            }
        }

        writer.finishGroup();
    }
    else if (c.field == QLatin1String("videoduration"))
    {
        auto toSeconds = [](const QString& token, bool* ok) -> int
        {
            QString t   = token.trimmed();
            int mult    = 1;

            if      (t.endsWith(QLatin1Char('m'))) { mult = 60; t.chop(1); }
            else if (t.endsWith(QLatin1Char('s'))) { mult = 1;  t.chop(1); }

            const int n = t.toInt(ok);

            return (n * mult);
        };

        const QStringList parts = c.value.split(QLatin1String(".."));

        if (parts.size() == 2)
        {
            bool okLo    = false;
            bool okHi    = false;
            const int lo = toSeconds(parts.at(0), &okLo);
            const int hi = toSeconds(parts.at(1), &okHi);

            if (okLo && okHi)
            {
                writer.writeField(QLatin1String("videoduration"), SearchXml::Interval);
                writer.writeValue(QList<int>() << lo << hi);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("videoframerate"))
    {
        const QStringList parts = c.value.split(QLatin1String(".."));

        if (parts.size() == 2)
        {
            bool okLo       = false;
            bool okHi       = false;
            const double lo = parts.at(0).toDouble(&okLo);
            const double hi = parts.at(1).toDouble(&okHi);

            if (okLo && okHi)
            {
                writer.writeField(QLatin1String("videoframerate"), SearchXml::Interval);
                writer.writeValue(QList<double>() << lo << hi);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("videoaudiobitrate"))
    {
        const QStringList parts = c.value.split(QLatin1String(".."));

        if (parts.size() == 2)
        {
            bool okLo    = false;
            bool okHi    = false;
            const int lo = parts.at(0).toInt(&okLo);
            const int hi = parts.at(1).toInt(&okHi);

            if (okLo && okHi)
            {
                writer.writeField(QLatin1String("videoaudiobitrate"), SearchXml::Interval);
                writer.writeValue(QList<int>() << lo << hi);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("format"))
    {
        const QString fmt = c.value.toUpper();

        if (fmt == QLatin1String("RAW"))
        {
            // RAW files are stored as "RAW-<EXT>" (RAW-CR2, RAW-NEF, ...).
            // Match by wildcard the way digiKam's own MIME filter does with
            // startsWith("RAW").

            writer.writeField(QLatin1String("format"), SearchXml::Like);
            writer.writeValue(QLatin1String("RAW*"));
            writer.finishField();
        }
        else
        {
            writer.writeField(QLatin1String("format"), SearchXml::Equal);
            writer.writeValue(fmt);
            writer.finishField();
        }
    }
    else
    {
        qCDebug(DIGIKAM_NLSEARCH_LOG) << "NL search: no XML mapping for field"
                                      << c.field << "value" << c.value;
    }
}

void SearchWindow::updateNlPanelState()
{

#ifdef HAVE_LLAMACPP

    const bool ready = SearchNlModelManager::isModelAvailable();

    if (d->nlReadyPanel)
    {
        d->nlReadyPanel->setVisible(ready);
    }

    if (d->nlDownloadPanel)
    {
        d->nlDownloadPanel->setVisible(!ready);
    }

#endif

}

} // namespace Digikam
