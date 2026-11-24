//  MemberBackup.cpp
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

#include "MemberBackup.hpp"

#include "MemberDatabase.hpp"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>


namespace
{
        const char* KEY_FOLDER     = "uyeler/yedekKlasoru";
        const char* KEY_LAST       = "uyeler/sonYedek";
        const char* KEY_LAST_DAILY = "uyeler/sonGunlukYedek";

        const QString AUTO_PREFIX = QStringLiteral( "uyeler-otomatik-yedek-" );


        QString timestamp()
        {
                return QDateTime::currentDateTime().toString( QStringLiteral( "yyyy-MM-dd_HH-mm-ss" ) );
        }


        bool ensureDir( const QString& path )
        {
                QDir dir( path );
                if ( !dir.exists() && !dir.mkpath( QStringLiteral( "." ) ) )
                {
                        return false;
                }
                // Make sure we can actually write there (read-only share, full USB stick, ...)
                QString probe = dir.filePath( QStringLiteral( ".yazma-denemesi" ) );
                QFile f( probe );
                if ( !f.open( QIODevice::WriteOnly ) )
                {
                        return false;
                }
                f.close();
                f.remove();
                return true;
        }
}


namespace glabels::merge::MemberBackup
{

        QString defaultFolder()
        {
                QString docs = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
                return QDir( docs ).filePath( QStringLiteral( "Üye Yedekleri" ) );
        }


        QString folder()
        {
                QSettings settings;
                QString f = settings.value( KEY_FOLDER ).toString();
                return f.isEmpty() ? defaultFolder() : f;
        }


        void setFolder( const QString& path )
        {
                QSettings settings;
                settings.setValue( KEY_FOLDER, QDir::cleanPath( path ) );
        }


        QString fallbackFolder()
        {
                QString dbDir = QFileInfo( MemberDatabase::instance()->filePath() ).absolutePath();
                return QDir( dbDir ).filePath( QStringLiteral( "yedekler" ) );
        }


        QDateTime lastBackupTime()
        {
                QSettings settings;
                return settings.value( KEY_LAST ).toDateTime();
        }


        void recordBackup( const QDateTime& when )
        {
                QSettings settings;
                settings.setValue( KEY_LAST, when );
        }


        bool isAutomaticBackupName( const QString& fileName )
        {
                static const QRegularExpression re(
                        QStringLiteral( "^uyeler-otomatik-yedek-\\d{4}-\\d{2}-\\d{2}_\\d{2}-\\d{2}-\\d{2}\\.sqlite$" ) );
                return re.match( fileName ).hasMatch();
        }


        int prune( const QString& folderPath, int keep )
        {
                QDir dir( folderPath );
                QStringList names;
                for ( const QString& name : dir.entryList( QDir::Files ) )
                {
                        if ( isAutomaticBackupName( name ) )
                        {
                                names << name;
                        }
                }
                // Timestamped names sort chronologically
                std::sort( names.begin(), names.end(), std::greater<QString>() );

                int removed = 0;
                for ( int i = keep; i < names.size(); i++ )
                {
                        if ( dir.remove( names[i] ) )
                        {
                                removed++;
                        }
                        else
                        {
                                qWarning( "MemberBackup: could not remove old backup %s", qUtf8Printable( names[i] ) );
                        }
                }
                return removed;
        }


        bool runAutomatic( QString* usedPath, QString* warning, QString* error )
        {
                QString target = folder();
                if ( !ensureDir( target ) )
                {
                        QString fallback = fallbackFolder();
                        if ( !ensureDir( fallback ) )
                        {
                                if ( error )
                                {
                                        *error = QStringLiteral( "Yedek klasörüne yazılamıyor: %1" ).arg( QDir::toNativeSeparators( target ) );
                                }
                                return false;
                        }
                        if ( warning )
                        {
                                *warning = QStringLiteral( "Yedek klasörüne ulaşılamadı (%1). Yedek bu bilgisayardaki yedek klasörüne alındı: %2" )
                                           .arg( QDir::toNativeSeparators( target ), QDir::toNativeSeparators( fallback ) );
                        }
                        target = fallback;
                }

                QString path = QDir( target ).filePath( AUTO_PREFIX + timestamp() + QStringLiteral( ".sqlite" ) );
                if ( !MemberDatabase::instance()->backupTo( path, error ) )
                {
                        return false;
                }

                recordBackup( QDateTime::currentDateTime() );
                prune( target, KEEP_AUTOMATIC );
                if ( usedPath )
                {
                        *usedPath = path;
                }
                return true;
        }


        bool runDailyIfDue( QString* usedPath, QString* warning, QString* error )
        {
                QSettings settings;
                QDate lastDaily = settings.value( KEY_LAST_DAILY ).toDate();
                if ( lastDaily == QDate::currentDate() )
                {
                        return true;   // already done today
                }

                int count = MemberDatabase::instance()->memberCount( error );
                if ( count < 0 )
                {
                        return false;
                }
                if ( count == 0 )
                {
                        return true;   // nothing worth backing up yet
                }

                if ( !runAutomatic( usedPath, warning, error ) )
                {
                        return false;
                }
                settings.setValue( KEY_LAST_DAILY, QDate::currentDate() );
                return true;
        }


        bool runOnExitIfNeeded( QString* usedPath, QString* warning, QString* error )
        {
                if ( !MemberDatabase::instance()->hasUnbackedChanges() )
                {
                        return true;
                }
                return runAutomatic( usedPath, warning, error );
        }


        QStringList automaticBackups()
        {
                QStringList paths;
                for ( const QString& f : { folder(), fallbackFolder() } )
                {
                        QDir dir( f );
                        for ( const QString& name : dir.entryList( QDir::Files ) )
                        {
                                if ( isAutomaticBackupName( name ) )
                                {
                                        paths << dir.filePath( name );
                                }
                        }
                }
                std::sort( paths.begin(), paths.end(), []( const QString& a, const QString& b ) {
                        return QFileInfo( a ).fileName() > QFileInfo( b ).fileName();
                } );
                paths.removeDuplicates();
                return paths;
        }


        QString preRestorePath()
        {
                QString dbDir = QFileInfo( MemberDatabase::instance()->filePath() ).absolutePath();
                return QDir( dbDir ).filePath( QStringLiteral( "yedekler/geri-yukleme-oncesi-%1.sqlite" ).arg( timestamp() ) );
        }

}
