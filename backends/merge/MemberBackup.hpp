//  MemberBackup.hpp
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

#ifndef merge_MemberBackup_hpp
#define merge_MemberBackup_hpp


#include <QDateTime>
#include <QString>
#include <QStringList>


namespace glabels::merge
{

        ///
        /// Automatic / manual backups of the member database
        ///
        namespace MemberBackup
        {
                constexpr int KEEP_AUTOMATIC = 30;     // automatic backups kept per folder
                constexpr int REMIND_AFTER_DAYS = 7;   // warn when the last backup is older

                /// Configured backup folder (default: Documents/Üye Yedekleri)
                QString folder();
                QString defaultFolder();
                void setFolder( const QString& path );

                /// Local fallback used when the configured folder is not reachable (e.g. USB removed)
                QString fallbackFolder();

                /// Time of the last successful backup (automatic or manual); invalid if never
                QDateTime lastBackupTime();
                void recordBackup( const QDateTime& when );

                /// File name pattern of automatic backups (only these are ever pruned)
                bool isAutomaticBackupName( const QString& fileName );

                /// Take an automatic backup now. On success *usedPath is set; *warning is set if the
                /// fallback folder had to be used. Old automatic backups beyond KEEP_AUTOMATIC are removed.
                bool runAutomatic( QString* usedPath, QString* warning, QString* error );

                /// Startup: back up once per day if there is anything to back up
                bool runDailyIfDue( QString* usedPath, QString* warning, QString* error );

                /// Exit: back up if anything changed since the last backup
                bool runOnExitIfNeeded( QString* usedPath, QString* warning, QString* error );

                /// Newest automatic backups (configured folder and fallback), newest first
                QStringList automaticBackups();

                /// Remove automatic backups in folder beyond keep (newest kept); returns removed count
                int prune( const QString& folder, int keep );

                /// Where the current database is copied before a restore
                QString preRestorePath();
        }

}


#endif // merge_MemberBackup_hpp
