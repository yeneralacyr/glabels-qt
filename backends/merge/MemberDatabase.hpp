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
                bool fail( const QString& message, QString* error );

                QString mConnectionName;
                QString mFilePath;
                QString mLastError;
                bool    mOpen{ false };

        };

}


#endif // merge_MemberDatabase_hpp
