/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2009-06-09
 * Description : a test for the AdvancedRename utility
 *
 * SPDX-FileCopyrightText: 2009-2011 by Andi Clemens <andi dot clemens at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "renamecustomizer_utest.h"

// Qt includes

#include <QFileInfo>
#include <QTest>
#include <QUrl>

// Local includes

#include "advancedrenamemanager.h"
#include "defaultrenameparser.h"
#include "parsesettings.h"
#include "renamecustomizer.h"
#include "digikam_debug.h"
#include "dtestdatadir.h"

using namespace Digikam;

QTEST_MAIN(RenameCustomizerTest)

void RenameCustomizerTest::newName_should_return_empty_string_with_empty_filename_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<QString>("result");

    QTest::newRow("empty")          << QString::fromUtf8("")      << QString::fromUtf8("");
    QTest::newRow("whitespaces")    << QString::fromUtf8("    ")  << QString::fromUtf8("");
}

void RenameCustomizerTest::newName_should_return_empty_string_with_empty_filename()
{
    QFETCH(QString, filename);
    QFETCH(QString, result);

    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    QCOMPARE(customizer.newName(filename), result);
}

void RenameCustomizerTest::setCaseType_set_to_none()
{
    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    customizer.setChangeCase(RenameCustomizer::NONE);
    QCOMPARE(customizer.changeCase(), RenameCustomizer::NONE);
}

void RenameCustomizerTest::setCaseType_set_to_upper()
{
    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    customizer.setChangeCase(RenameCustomizer::UPPER);
    QCOMPARE(customizer.changeCase(), RenameCustomizer::UPPER);
}

void RenameCustomizerTest::setCaseType_set_to_lower()
{
    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    customizer.setChangeCase(RenameCustomizer::LOWER);
    QCOMPARE(customizer.changeCase(), RenameCustomizer::LOWER);
}

void RenameCustomizerTest::setUseDefault_true()
{
    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    customizer.setUseDefault(true);
    QVERIFY(customizer.useDefault());
}

void RenameCustomizerTest::setUseDefault_false()
{
    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    customizer.setUseDefault(false);
    QVERIFY(customizer.useDefault() == false);
}

void RenameCustomizerTest::setUseDefault_case_none_should_deliver_original_filename()
{
    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    customizer.setUseDefault(true);
    customizer.setChangeCase(RenameCustomizer::NONE);
    QCOMPARE(customizer.newName(QLatin1String("TeSt.png")), QLatin1String("TeSt.png"));
}

void RenameCustomizerTest::setUseDefault_case_upper_should_deliver_uppercase_filename()
{
    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    customizer.setUseDefault(true);
    customizer.setChangeCase(RenameCustomizer::UPPER);
    QCOMPARE(customizer.newName(QLatin1String("TeSt.png")), QLatin1String("TEST.PNG"));
}

void RenameCustomizerTest::setUseDefault_case_lower_should_deliver_lowercase_filename()
{
    RenameCustomizer customizer(nullptr, QLatin1String("Unit Tests"));
    customizer.setUseDefault(true);
    customizer.setChangeCase(RenameCustomizer::LOWER);
    QCOMPARE(customizer.newName(QLatin1String("TeSt.pnG")), QLatin1String("test.png"));
}

#include "moc_renamecustomizer_utest.cpp"
