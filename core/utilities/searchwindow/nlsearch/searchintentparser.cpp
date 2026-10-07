/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Parses and validates raw model output into a
 *               SearchQueryIntent. Rejects anything that is not a
 *               well-formed JSON object matching the schema. The
 *               model output is NEVER trusted directly.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchintentparser.h"

// C++ includes

#include <cmath>

// Qt includes

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

namespace
{

const QSet<QString>& supportedFields()
{
    static const QSet<QString> s =
    {
        QLatin1String("tag"),
        QLatin1String("album"),
        QLatin1String("person"),
        QLatin1String("place"),
        QLatin1String("daterange"),
        QLatin1String("rating"),
        QLatin1String("picklabel"),
        QLatin1String("colorlabel"),
        QLatin1String("orientation"),
        QLatin1String("caption"),
        QLatin1String("videoduration"),
        QLatin1String("videoframerate"),
        QLatin1String("videoaudiobitrate"),
        QLatin1String("format"),
    };

    return s;
}

const QSet<QString>& supportedOps()
{
    static const QSet<QString> s =
    {
        QLatin1String("eq"),
        QLatin1String("contains"),
        QLatin1String("gte"),
        QLatin1String("lte"),
        QLatin1String("between"),
    };

    return s;
}

} // namespace

namespace Digikam
{

QByteArray SearchIntentParser::extractJsonObject(const QByteArray& raw) const
{
    const int start = raw.indexOf('{');

    if (start < 0)
    {
        return QByteArray();
    }

    int depth    = 0;
    bool inStr   = false;
    bool escaped = false;

    for (int i = start ; i < raw.size() ; ++i)
    {
        const char c = raw.at(i);

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (c == '\\')
        {
            escaped = true;
            continue;
        }

        if (c == '"')
        {
            inStr = !inStr;
            continue;
        }

        if (inStr)
        {
            continue;
        }

        if      (c == '{')
        {
            ++depth;
        }
        else if (c == '}')
        {
            --depth;

            if (depth == 0)
            {
                return raw.mid(start, i - start + 1);
            }
        }
    }

    return QByteArray();
}

SearchQueryIntent SearchIntentParser::parse(const QByteArray& rawJson,
                                            const QString& originalQuery,
                                            const QString& normalizedQuery) const
{
    SearchQueryIntent intent;
    intent.originalQuery            = originalQuery;
    intent.normalizedQuery          = normalizedQuery;
    intent.rawModelOutput           = QString::fromUtf8(rawJson);

    const QByteArray jsonBlock      = extractJsonObject(rawJson);

    if (jsonBlock.isEmpty())
    {
        return intent;
    }

    QJsonParseError err {};
    const QJsonDocument doc         = QJsonDocument::fromJson(jsonBlock, &err);

    if ((err.error != QJsonParseError::NoError) || !doc.isObject())
    {
        return intent;
    }

    const QJsonObject root          = doc.object();
    const QJsonValue constraintsVal = root.value(QLatin1String("constraints"));

    if (!constraintsVal.isArray())
    {
        return intent;
    }

    const auto arr                  = constraintsVal.toArray();

    for (const QJsonValue& v : arr)
    {
        if (!v.isObject())
        {
            return intent;   // malformed entry => reject whole output
        }

        const QJsonObject o       = v.toObject();

        SearchQueryConstraint c;
        c.field                   = o.value(QLatin1String("field")).toString().trimmed().toLower();
        c.op                      = o.value(QLatin1String("op")).toString().trimmed().toLower();
        const QJsonValue valueVal = o.value(QLatin1String("value"));

        if      (valueVal.isString())
        {
            c.value = valueVal.toString().trimmed();
        }
        else if (valueVal.isDouble())
        {
            const double d = valueVal.toDouble();

            if (d == std::floor(d))
            {
                c.value = QString::number(static_cast<qlonglong>(d));
            }
            else
            {
                c.value = QString::number(d);
            }
        }
        else if (valueVal.isBool())
        {
            c.value = valueVal.toBool() ? QLatin1String("true")
                                        : QLatin1String("false");
        }
        else
        {
            c.value = QString();
        }

        if (!validateField(c.field) || !validateOperator(c.field, c.op))
        {
            return intent;
        }

        intent.constraints << c;
    }

    const QJsonValue clarVal = root.value(QLatin1String("clarification"));

    if (clarVal.isObject())
    {
        const QJsonObject clar       = clarVal.toObject();
        intent.clarificationMessage  = clar.value(QLatin1String("message")).toString();

        const auto choices           = clar.value(QLatin1String("choices")).toArray();

        for (const QJsonValue& c : choices)
        {
            intent.clarificationChoices << c.toString();
        }

        intent.requiresClarification = !intent.clarificationMessage.isEmpty();
    }

    intent.parseSucceeded = true;

    return intent;
}

bool SearchIntentParser::validate(const SearchQueryIntent& intent) const
{
    return validationErrors(intent).isEmpty();
}

QStringList SearchIntentParser::validationErrors(const SearchQueryIntent& intent) const
{
    QStringList errors;

    if (!intent.parseSucceeded)
    {
        errors << QLatin1String("Model output could not be parsed as schema-conforming JSON.");
    }

    for (const SearchQueryConstraint& c : intent.constraints)
    {
        if (!validateField(c.field))
        {
            errors << QString::fromLatin1("Unsupported field: %1").arg(c.field);
        }

        if (!validateOperator(c.field, c.op))
        {
            errors << QString::fromLatin1("Unsupported operator '%1' for field '%2'").arg(c.op, c.field);
        }

        if (c.value.isEmpty())
        {
            errors << QString::fromLatin1("Empty value for field '%1'").arg(c.field);
        }
    }

    return errors;
}

bool SearchIntentParser::validateField(const QString& field) const
{
    return supportedFields().contains(field);
}

bool SearchIntentParser::validateOperator(const QString& field, const QString& op) const
{
    if (!supportedOps().contains(op))
    {
        return false;
    }

    // Field-specific restrictions: numeric/date comparisons only where they make sense.

    if ((op == QLatin1String("gte")) || (op == QLatin1String("lte")))
    {
        return (
                (field == QLatin1String("rating")) ||
                (field == QLatin1String("daterange"))
               );
    }

    if (op == QLatin1String("between"))
    {
        return (
                (field == QLatin1String("daterange"))      ||
                (field == QLatin1String("rating"))         ||
                (field == QLatin1String("videoduration"))  ||
                (field == QLatin1String("videoframerate")) ||
                (field == QLatin1String("videoaudiobitrate"))
               );
    }

    return true;
}

} // namespace Digikam
