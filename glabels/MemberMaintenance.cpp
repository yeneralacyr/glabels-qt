//  MemberMaintenance.cpp
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

#include "MemberMaintenance.hpp"

#include "merge/MemberBackup.hpp"
#include "merge/MemberDatabase.hpp"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>


namespace
{
        QString gNotice;
}


namespace glabels::MemberMaintenance
{

        QString lastNotice()
        {
                return gNotice;
        }


        void clearNotice()
        {
                gNotice.clear();
        }


        void startup( QWidget* parent )
        {
                auto db = merge::MemberDatabase::instance();
                QString error;

                bool healthy = db->ensureOpen( &error ) && db->checkIntegrity( &error );
                if ( !healthy )
                {
                        QString live = db->filePath();
                        merge::BackupInfo liveInfo;
                        QString liveError;
                        bool liveExists = QFileInfo::exists( live );
                        merge::MemberDatabase::inspectBackup( live, &liveInfo, &liveError );
                        bool newerProgram = liveInfo.schemaVersion > merge::MemberDatabase::supportedSchemaVersion();

                        // Find the newest automatic backup that is actually usable
                        QString candidate;
                        merge::BackupInfo candidateInfo;
                        if ( liveExists && !newerProgram )
                        {
                                for ( const QString& path : merge::MemberBackup::automaticBackups() )
                                {
                                        QString e;
                                        if ( merge::MemberDatabase::inspectBackup( path, &candidateInfo, &e ) )
                                        {
                                                candidate = path;
                                                break;
                                        }
                                }
                        }

                        if ( candidate.isEmpty() )
                        {
                                // Members page shows the error with a retry button; nothing to offer here
                                qWarning( "Member database problem: %s", qUtf8Printable( error ) );
                                return;
                        }

                        QString when = QLocale( QLocale::Turkish ).toString( candidateInfo.fileTime, QStringLiteral( "d MMMM yyyy HH:mm" ) );
                        auto answer = QMessageBox::warning(
                                parent, QStringLiteral( "Üye veritabanında sorun" ),
                                QStringLiteral( "Üye veritabanı açılırken bir sorun bulundu:\n%1\n\n"
                                                "En son otomatik yedek: %2 (%3 üye).\n\n"
                                                "Bu yedek geri yüklensin mi? Mevcut (sorunlu) dosya silinmez, ayrıca saklanır." )
                                        .arg( error, when ).arg( candidateInfo.totalMembers ),
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
                        if ( answer != QMessageBox::Yes )
                        {
                                return;
                        }

                        QString preserve = merge::MemberBackup::preRestorePath();
                        QString restoreError;
                        if ( !db->restoreFrom( candidate, preserve, &restoreError ) )
                        {
                                QMessageBox::critical( parent, QStringLiteral( "Geri yükleme başarısız" ), restoreError );
                                return;
                        }
                        QMessageBox::information( parent, QStringLiteral( "Geri yüklendi" ),
                                                  QStringLiteral( "Yedek geri yüklendi.\n\nSorunlu dosya şuraya kaydedildi:\n%1" )
                                                  .arg( QDir::toNativeSeparators( preserve ) ) );
                }

                QString used, warning, backupError;
                if ( !merge::MemberBackup::runDailyIfDue( &used, &warning, &backupError ) )
                {
                        gNotice = QStringLiteral( "Otomatik yedek alınamadı: %1" ).arg( backupError );
                }
                else if ( !warning.isEmpty() )
                {
                        gNotice = warning;
                }
        }


        void shutdown()
        {
                QString used, warning, error;
                if ( !merge::MemberBackup::runOnExitIfNeeded( &used, &warning, &error ) )
                {
                        // No UI at exit; the "last backup" reminder on the Members page will show it next time
                        qWarning( "Automatic backup at exit failed: %s", qUtf8Printable( error ) );
                }
        }

}
