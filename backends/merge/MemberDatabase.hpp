//  MemberDatabase.hpp
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

#ifndef merge_MemberDatabase_hpp
#define merge_MemberDatabase_hpp


#include <QDateTime>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>


namespace glabels::merge
{

        ///
        /// Field definition (key used in label templates as ${KEY}, label shown in UI)
        ///
        struct MemberField
        {
                QString key;
                QString label;
                bool    multiLine{ false };
        };


        ///
        /// One member record
        ///
        struct BackupInfo
        {
                int       schemaVersion{ 0 };
                int       totalMembers{ 0 };
                int       activeMembers{ 0 };
                QDateTime fileTime;
        };


        struct Member
        {
                qint64                 id{ 0 };       // 0 = not yet stored
                bool                   active{ true };
                QMap<QString,QString>  values;        // standard + custom field values by key
        };


        ///
        /// Local SQLite member database (single shared file per user)
        ///
        class MemberDatabase : public QObject
        {
                Q_OBJECT

                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        private:
                MemberDatabase();

        public:
                ~MemberDatabase() override;
                MemberDatabase( const MemberDatabase& ) = delete;
                MemberDatabase& operator=( const MemberDatabase& ) = delete;

                static MemberDatabase* instance();


                /////////////////////////////////
                // Signals
                /////////////////////////////////
        signals:
                void changed();


                /////////////////////////////////
                // Static field info
                /////////////////////////////////
        public:
                static const QList<MemberField>& standardFields();
                static int supportedSchemaVersion();
                static QString idKey();          // "UYE_ID" (primary key exposed to merge)
                static QString activeKey();      // "AKTIF"
                static bool isValidCustomKey( const QString& key );
                static QString suggestKey( const QString& label );


                /////////////////////////////////
                // Connection
                /////////////////////////////////
        public:
                bool ensureOpen( QString* error );
                bool isOpen() const;
                QString filePath() const;
                QString lastError() const;
                bool backupTo( const QString& destPath, QString* error );

                /// Quick integrity check of the open database (PRAGMA quick_check)
                bool checkIntegrity( QString* error );

                /// True if anything was changed since the last successful backup
                bool hasUnbackedChanges() const;

                /// Validate a backup file without touching the live database
                static bool inspectBackup( const QString& path, BackupInfo* info, QString* error );

                /// Replace the live database with a backup. The current file (even if corrupt) is
                /// first copied to preservePath so the operation can be undone.
                bool restoreFrom( const QString& backupPath, const QString& preservePath, QString* error );

                /// Number of members (all, including passive); -1 on error
                int memberCount( QString* error );


                /////////////////////////////////
                // Custom fields
                /////////////////////////////////
        public:
                bool customFields( QList<MemberField>& fields, QString* error );
                bool allFields( QList<MemberField>& fields, QString* error );
                bool addCustomField( const MemberField& field, QString* error );
                bool removeCustomField( const QString& key, QString* error );


                /////////////////////////////////
                // Members
                /////////////////////////////////
        public:
                bool members( QList<Member>& list, bool includePassive, QString* error );
                bool saveMember( Member& member, QString* error );
                bool deleteMembers( const QList<qint64>& ids, QString* error );
                bool setActive( const QList<qint64>& ids, bool active, QString* error );


                /////////////////////////////////
                // Private
                /////////////////////////////////
        private:
                bool createSchema( QString* error );
                void closeConnection();
                void markChanged();
                bool fail( const QString& message, QString* error );

                QString mConnectionName;
                QString mFilePath;
                QString mLastError;
                bool    mOpen{ false };
                bool    mUnbackedChanges{ false };

        };

}


#endif // merge_MemberDatabase_hpp
