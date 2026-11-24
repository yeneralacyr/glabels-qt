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
#include "merge/MemberBackup.hpp"
#include "merge/MemberDatabase.hpp"
#include "merge/MemberDb.hpp"

#include <QSettings>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <memory>


QTEST_MAIN(TestMemberDatabase)

using namespace glabels::merge;


void TestMemberDatabase::initTestCase()
{
        QVERIFY( mTmp.isValid() );
        // Must be set before the singleton is first used
        qputenv( "GLABELS_UYELER_DB", mTmp.filePath( "uyeler.sqlite" ).toUtf8() );
        QCoreApplication::setOrganizationName( "glabels-unit-test" );
        QCoreApplication::setApplicationName( "TestMemberDatabase" );
        QSettings().clear();
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


void TestMemberDatabase::integrityAndInspect()
{
        auto db = MemberDatabase::instance();
        QString error;
        QVERIFY2( db->checkIntegrity( &error ), qPrintable( error ) );
        QCOMPARE( db->memberCount( &error ), 3 );

        // Valid backup
        QString good = mTmp.filePath( "inspect-good.sqlite" );
        QVERIFY( db->backupTo( good, &error ) );
        BackupInfo info;
        QVERIFY2( MemberDatabase::inspectBackup( good, &info, &error ), qPrintable( error ) );
        QCOMPARE( info.totalMembers, 3 );
        QCOMPARE( info.activeMembers, 2 );
        QCOMPARE( info.schemaVersion, MemberDatabase::supportedSchemaVersion() );

        // Missing, empty, not a database
        QVERIFY( !MemberDatabase::inspectBackup( mTmp.filePath( "yok.sqlite" ), &info, &error ) );
        QString empty = mTmp.filePath( "bos.sqlite" );
        { QFile f( empty ); QVERIFY( f.open( QIODevice::WriteOnly ) ); }
        QVERIFY( !MemberDatabase::inspectBackup( empty, &info, &error ) );
        QString junk = mTmp.filePath( "cop.sqlite" );
        { QFile f( junk ); QVERIFY( f.open( QIODevice::WriteOnly ) ); f.write( QByteArray( 4096, 'x' ) ); }
        QVERIFY( !MemberDatabase::inspectBackup( junk, &info, &error ) );
        QVERIFY( !error.isEmpty() );

        // SQLite file that is not a member database, and one from a newer program version
        auto makeDb = [&]( const QString& path, const QStringList& sql ) {
                {
                        QSqlDatabase d = QSqlDatabase::addDatabase( "QSQLITE", "make" );
                        d.setDatabaseName( path );
                        QVERIFY( d.open() );
                        QSqlQuery q( d );
                        for ( const auto& s : sql ) { QVERIFY2( q.exec( s ), qPrintable( q.lastError().text() ) ); }
                        d.close();
                }
                QSqlDatabase::removeDatabase( "make" );
        };
        QString other = mTmp.filePath( "baska.sqlite" );
        makeDb( other, { "CREATE TABLE x (a)" } );
        QVERIFY( !MemberDatabase::inspectBackup( other, &info, &error ) );
        QString newer = mTmp.filePath( "yeni-surum.sqlite" );
        QFile::copy( good, newer );
        makeDb( newer, { "PRAGMA user_version = 99" } );
        info = BackupInfo();
        QVERIFY( !MemberDatabase::inspectBackup( newer, &info, &error ) );
        QCOMPARE( info.schemaVersion, 99 );
}


void TestMemberDatabase::restore()
{
        auto db = MemberDatabase::instance();
        QString error;

        QString snapshot = mTmp.filePath( "restore-snapshot.sqlite" );
        QVERIFY( db->backupTo( snapshot, &error ) );
        int before = db->memberCount( &error );

        // Change data after the snapshot
        Member extra;
        extra.values["FIRMA"] = "Sonradan Eklenen";
        QVERIFY( db->saveMember( extra, &error ) );
        QCOMPARE( db->memberCount( &error ), before + 1 );
        QVERIFY( db->hasUnbackedChanges() );

        // Leftover journal of the old file must not survive the restore
        QString journal = db->filePath() + "-journal";
        { QFile f( journal ); QVERIFY( f.open( QIODevice::WriteOnly ) ); }

        QSignalSpy spy( db, &MemberDatabase::changed );
        QString preserve = mTmp.filePath( "sub/geri-yukleme-oncesi.sqlite" );
        QVERIFY2( db->restoreFrom( snapshot, preserve, &error ), qPrintable( error ) );
        QCOMPARE( spy.count(), 1 );
        QVERIFY( db->isOpen() );
        QCOMPARE( db->memberCount( &error ), before );
        QVERIFY( !db->hasUnbackedChanges() );
        QVERIFY( !QFile::exists( journal ) );
        QVERIFY( !QFile::exists( db->filePath() + ".eski" ) );
        QVERIFY( !QFile::exists( db->filePath() + ".geri-yukleme" ) );

        // The pre-restore copy holds the newer data and can itself be restored (undo)
        BackupInfo info;
        QVERIFY2( MemberDatabase::inspectBackup( preserve, &info, &error ), qPrintable( error ) );
        QCOMPARE( info.totalMembers, before + 1 );
        QVERIFY2( db->restoreFrom( preserve, mTmp.filePath( "undo-preserve.sqlite" ), &error ), qPrintable( error ) );
        QCOMPARE( db->memberCount( &error ), before + 1 );

        // Writes still work after reopening
        Member after;
        after.values["FIRMA"] = "Geri Yükleme Sonrası";
        QVERIFY2( db->saveMember( after, &error ), qPrintable( error ) );

        // Refusals leave the live database untouched
        int now = db->memberCount( &error );
        QVERIFY( !db->restoreFrom( db->filePath(), mTmp.filePath( "x.sqlite" ), &error ) );
        QVERIFY( !db->restoreFrom( mTmp.filePath( "cop.sqlite" ), mTmp.filePath( "y.sqlite" ), &error ) );
        QVERIFY( db->isOpen() );
        QCOMPARE( db->memberCount( &error ), now );
}


void TestMemberDatabase::automaticBackups()
{
        QString error, warning, used;
        QString folder = mTmp.filePath( "Üye Yedekleri" );
        MemberBackup::setFolder( folder );
        QCOMPARE( MemberBackup::folder(), QDir::cleanPath( folder ) );

        QVERIFY( MemberBackup::isAutomaticBackupName( "uyeler-otomatik-yedek-2026-11-24_10-00-00.sqlite" ) );
        QVERIFY( !MemberBackup::isAutomaticBackupName( "uyeler-yedek-2026-11-24.sqlite" ) );
        QVERIFY( !MemberBackup::isAutomaticBackupName( "geri-yukleme-oncesi-2026-11-24_10-00-00.sqlite" ) );

        QVERIFY2( MemberBackup::runAutomatic( &used, &warning, &error ), qPrintable( error ) );
        QVERIFY( warning.isEmpty() );
        QVERIFY( used.startsWith( QDir::cleanPath( folder ) ) );
        QVERIFY( MemberDatabase::inspectBackup( used, nullptr, &error ) );
        QVERIFY( MemberBackup::lastBackupTime().isValid() );

        // Pruning keeps the newest N automatic backups and never touches other files
        QDir dir( folder );
        for ( int i = 0; i < 35; i++ )
        {
                QFile f( dir.filePath( QString( "uyeler-otomatik-yedek-2020-01-01_00-00-%1.sqlite" ).arg( i, 2, 10, QChar( '0' ) ) ) );
                QVERIFY( f.open( QIODevice::WriteOnly ) );
        }
        { QFile f( dir.filePath( "uyeler-yedek-2020-01-01.sqlite" ) ); QVERIFY( f.open( QIODevice::WriteOnly ) ); }
        { QFile f( dir.filePath( "notlar.txt" ) ); QVERIFY( f.open( QIODevice::WriteOnly ) ); }
        int removed = MemberBackup::prune( folder, MemberBackup::KEEP_AUTOMATIC );
        QCOMPARE( removed, 36 - MemberBackup::KEEP_AUTOMATIC );
        QVERIFY( QFile::exists( used ) );                                       // newest kept
        QVERIFY( QFile::exists( dir.filePath( "uyeler-yedek-2020-01-01.sqlite" ) ) );
        QVERIFY( QFile::exists( dir.filePath( "notlar.txt" ) ) );
        QVERIFY( !QFile::exists( dir.filePath( "uyeler-otomatik-yedek-2020-01-01_00-00-00.sqlite" ) ) );
        QCOMPARE( MemberBackup::automaticBackups().first(), used );

        // Unreachable folder (a file blocks the path) -> local fallback with a warning
        QString blocker = mTmp.filePath( "engel" );
        { QFile f( blocker ); QVERIFY( f.open( QIODevice::WriteOnly ) ); }
        MemberBackup::setFolder( blocker + "/alt" );
        warning.clear();
        QVERIFY2( MemberBackup::runAutomatic( &used, &warning, &error ), qPrintable( error ) );
        QVERIFY( !warning.isEmpty() );
        QVERIFY( used.startsWith( MemberBackup::fallbackFolder() ) );
        MemberBackup::setFolder( folder );
}


void TestMemberDatabase::dailyAndExit()
{
        auto db = MemberDatabase::instance();
        QString error, warning, used;
        QSettings().remove( "uyeler/sonGunlukYedek" );

        QVERIFY2( MemberBackup::runDailyIfDue( &used, &warning, &error ), qPrintable( error ) );
        QVERIFY( !used.isEmpty() );
        used.clear();
        QVERIFY( MemberBackup::runDailyIfDue( &used, &warning, &error ) );
        QVERIFY( used.isEmpty() );                     // only once per day

        // Exit backup only when something changed since the last backup
        QVERIFY( !db->hasUnbackedChanges() );
        QVERIFY( MemberBackup::runOnExitIfNeeded( &used, &warning, &error ) );
        QVERIFY( used.isEmpty() );
        Member m;
        m.values["FIRMA"] = "Çıkış Testi";
        QVERIFY( db->saveMember( m, &error ) );
        QVERIFY( db->hasUnbackedChanges() );
        QVERIFY2( MemberBackup::runOnExitIfNeeded( &used, &warning, &error ), qPrintable( error ) );
        QVERIFY( !used.isEmpty() );
        QVERIFY( !db->hasUnbackedChanges() );
}
