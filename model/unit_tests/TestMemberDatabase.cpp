//  TestMemberDatabase.cpp
//
//  Copyright (C) 2026  gLabels-qt contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//

#include "TestMemberDatabase.hpp"

#include "merge/Factory.hpp"
#include "merge/MemberDatabase.hpp"
#include "merge/MemberDb.hpp"

#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>

#include <memory>


QTEST_MAIN(TestMemberDatabase)

using namespace glabels::merge;


void TestMemberDatabase::initTestCase()
{
        QVERIFY( mTmp.isValid() );
        // Must be set before the singleton is first used
        qputenv( "GLABELS_UYELER_DB", mTmp.filePath( "uyeler.sqlite" ).toUtf8() );
        Factory::init();

        QString error;
        QVERIFY2( MemberDatabase::instance()->ensureOpen( &error ), qPrintable( error ) );
        QCOMPARE( MemberDatabase::instance()->filePath(), QFileInfo( mTmp.filePath( "uyeler.sqlite" ) ).absoluteFilePath() );
}


void TestMemberDatabase::suggestKey_data()
{
        QTest::addColumn<QString>( "label" );
        QTest::addColumn<QString>( "key" );

        QTest::newRow( "turkish" )  << QString( "Oda Kayıt Tarihi" ) << QString( "ODA_KAYIT_TARIHI" );
        QTest::newRow( "dotted-i" ) << QString( "İşyeri Şubesi" )   << QString( "ISYERI_SUBESI" );
        QTest::newRow( "symbols" )  << QString( "  Web / Sosyal  " ) << QString( "WEB_SOSYAL" );
        QTest::newRow( "digit" )    << QString( "2. Telefon" )      << QString( "ALAN_2_TELEFON" );
        QTest::newRow( "empty" )    << QString( "!!!" )             << QString( "" );
}


void TestMemberDatabase::suggestKey()
{
        QFETCH( QString, label );
        QFETCH( QString, key );
        QCOMPARE( MemberDatabase::suggestKey( label ), key );
}


void TestMemberDatabase::customKeyValidation()
{
        QVERIFY( MemberDatabase::isValidCustomKey( "WEB_SITESI" ) );
        QVERIFY( !MemberDatabase::isValidCustomKey( "" ) );
        QVERIFY( !MemberDatabase::isValidCustomKey( "web" ) );
        QVERIFY( !MemberDatabase::isValidCustomKey( "1ABC" ) );
        QVERIFY( !MemberDatabase::isValidCustomKey( "AD" ) );       // built-in
        QVERIFY( !MemberDatabase::isValidCustomKey( "UYE_ID" ) );   // reserved
        QVERIFY( !MemberDatabase::isValidCustomKey( "AKTIF" ) );    // reserved
        QVERIFY( !MemberDatabase::isValidCustomKey( "A B" ) );
}


void TestMemberDatabase::crud()
{
        auto db = MemberDatabase::instance();
        QSignalSpy spy( db, &MemberDatabase::changed );
        QString error;

        Member a;
        a.values["FIRMA"] = "Alaçayır Ticaret";
        a.values["AD"] = "Yener";
        a.values["SOYAD"] = "Alaçayır";
        a.values["ADRES"] = "Yenişehir\nMersin";
        a.values["NOT"] = "  baştaki boşluk silinmeli  ";
        QVERIFY2( db->saveMember( a, &error ), qPrintable( error ) );
        QVERIFY( a.id > 0 );

        Member b;
        b.values["FIRMA"] = "Beta Gıda";
        QVERIFY2( db->saveMember( b, &error ), qPrintable( error ) );
        QVERIFY( b.id > a.id );
        QCOMPARE( spy.count(), 2 );

        QList<Member> list;
        QVERIFY2( db->members( list, false, &error ), qPrintable( error ) );
        QCOMPARE( list.size(), 2 );
        QCOMPARE( list[0].values["FIRMA"], QString( "Alaçayır Ticaret" ) );   // ordered by firma
        QCOMPARE( list[0].values["ADRES"], QString( "Yenişehir\nMersin" ) );
        QCOMPARE( list[0].values["NOT"], QString( "baştaki boşluk silinmeli" ) );
        QCOMPARE( list[0].values["TELEFON"], QString() );

        // Update
        a.values["TELEFON"] = "0532 000 00 00";
        QVERIFY2( db->saveMember( a, &error ), qPrintable( error ) );
        QVERIFY( db->members( list, false, &error ) );
        QCOMPARE( list[0].values["TELEFON"], QString( "0532 000 00 00" ) );

        // Update of a missing id fails and reports an error
        Member ghost;
        ghost.id = 999999;
        ghost.values["FIRMA"] = "Yok";
        QVERIFY( !db->saveMember( ghost, &error ) );
        QVERIFY( !error.isEmpty() );

        // Passive members are hidden unless requested
        QVERIFY2( db->setActive( { b.id }, false, &error ), qPrintable( error ) );
        QVERIFY( db->members( list, false, &error ) );
        QCOMPARE( list.size(), 1 );
        QVERIFY( db->members( list, true, &error ) );
        QCOMPARE( list.size(), 2 );

        // Delete
        QVERIFY2( db->deleteMembers( { b.id }, &error ), qPrintable( error ) );
        QVERIFY( db->members( list, true, &error ) );
        QCOMPARE( list.size(), 1 );
        QCOMPARE( list[0].id, a.id );
}


void TestMemberDatabase::customFields()
{
        auto db = MemberDatabase::instance();
        QString error;

        QVERIFY2( db->addCustomField( { "WEB", "Web Sitesi", false }, &error ), qPrintable( error ) );
        QVERIFY( !db->addCustomField( { "WEB", "Tekrar", false }, &error ) );     // duplicate
        QVERIFY( !db->addCustomField( { "AD", "Ad", false }, &error ) );          // built-in clash
        QVERIFY( !db->addCustomField( { "OK", "   ", false }, &error ) );         // empty label

        QList<MemberField> custom;
        QVERIFY( db->customFields( custom, &error ) );
        QCOMPARE( custom.size(), 1 );
        QCOMPARE( custom[0].key, QString( "WEB" ) );
        QCOMPARE( custom[0].label, QString( "Web Sitesi" ) );

        QList<Member> list;
        QVERIFY( db->members( list, true, &error ) );
        QCOMPARE( list.size(), 1 );
        Member m = list[0];
        QVERIFY( m.values.contains( "WEB" ) );
        m.values["WEB"] = "https://ornek.com.tr";
        QVERIFY2( db->saveMember( m, &error ), qPrintable( error ) );
        QVERIFY( db->members( list, true, &error ) );
        QCOMPARE( list[0].values["WEB"], QString( "https://ornek.com.tr" ) );

        // Removing the field removes its values (cascade)
        QVERIFY2( db->removeCustomField( "WEB", &error ), qPrintable( error ) );
        QVERIFY( !db->removeCustomField( "WEB", &error ) );
        QVERIFY( db->members( list, true, &error ) );
        QVERIFY( !list[0].values.contains( "WEB" ) );

        QSqlQuery q( QSqlDatabase::database( "glabels-uyeler" ) );
        QVERIFY( q.exec( "SELECT COUNT(*) FROM member_values" ) && q.next() );
        QCOMPARE( q.value( 0 ).toInt(), 0 );
}


void TestMemberDatabase::mergeBackend()
{
        auto db = MemberDatabase::instance();
        QString error;

        Member c;
        c.values["FIRMA"] = "Zeta İnşaat";
        c.values["AD"] = "Ayşe";
        QVERIFY2( db->saveMember( c, &error ), qPrintable( error ) );

        Member passive;
        passive.values["FIRMA"] = "Pasif Ltd";
        passive.active = false;
        QVERIFY2( db->saveMember( passive, &error ), qPrintable( error ) );

        QCOMPARE( Factory::idToType( MemberDb::id() ), Factory::FIXED );

        std::unique_ptr<Merge> merge( Factory::createMerge( MemberDb::id() ) );
        QCOMPARE( merge->id(), MemberDb::id() );
        QVERIFY( merge->keys().contains( "FIRMA" ) );
        QVERIFY( merge->keys().contains( "UYE_ID" ) );
        QCOMPARE( merge->primaryKey(), QString( "UYE_ID" ) );

        merge->setSource( "" );
        QCOMPARE( merge->recordList().size(), 2 );          // passive excluded
        QCOMPARE( merge->nSelectedRecords(), 0 );           // nothing selected by default (safety)
        QCOMPARE( merge->recordList()[1]["FIRMA"], QString( "Zeta İnşaat" ) );

        QSignalSpy spy( merge.get(), &Merge::selectionChanged );
        int n = merge->selectOnlyKeys( { QString::number( c.id ), "123456" } );
        QCOMPARE( n, 1 );
        QCOMPARE( spy.count(), 1 );
        QCOMPARE( merge->selectedRecords().size(), 1 );
        QCOMPARE( merge->selectedRecords()[0]["AD"], QString( "Ayşe" ) );

        // Reload keeps selection by primary key and picks up edits
        c.values["AD"] = "Ayşe Nur";
        QVERIFY( db->saveMember( c, &error ) );
        merge->reloadSource();
        QCOMPARE( merge->nSelectedRecords(), 1 );
        QCOMPARE( merge->selectedRecords()[0]["AD"], QString( "Ayşe Nur" ) );

        std::unique_ptr<Merge> copy( merge->clone() );
        QCOMPARE( copy->id(), MemberDb::id() );
        QCOMPARE( copy->nSelectedRecords(), 1 );
}


void TestMemberDatabase::backup()
{
        auto db = MemberDatabase::instance();
        QString error;
        QString dest = mTmp.filePath( "yedek.sqlite" );

        QVERIFY2( db->backupTo( dest, &error ), qPrintable( error ) );
        QVERIFY( QFileInfo( dest ).size() > 0 );
        // Overwrite works too
        QVERIFY2( db->backupTo( dest, &error ), qPrintable( error ) );
        // Refuses to overwrite itself
        QVERIFY( !db->backupTo( db->filePath(), &error ) );

        {
                QSqlDatabase b = QSqlDatabase::addDatabase( "QSQLITE", "backup-check" );
                b.setDatabaseName( dest );
                QVERIFY( b.open() );
                QSqlQuery q( b );
                QVERIFY( q.exec( "SELECT COUNT(*) FROM members" ) && q.next() );
                QCOMPARE( q.value( 0 ).toInt(), 3 );
                b.close();
        }
        QSqlDatabase::removeDatabase( "backup-check" );
}
