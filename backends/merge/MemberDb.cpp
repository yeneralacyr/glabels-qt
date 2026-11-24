//  MemberDb.cpp
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

#include "MemberDb.hpp"

#include "MemberDatabase.hpp"



namespace glabels::merge
{

        MemberDb::MemberDb() : Merge()
        {
                mId = id();
        }


        MemberDb::MemberDb( const MemberDb* merge ) : Merge( merge )
        {
        }


        MemberDb* MemberDb::clone() const
        {
                return new MemberDb( this );
        }


        QString MemberDb::id()
        {
                return QStringLiteral( "Builtin/Members" );
        }


        Merge* MemberDb::create()
        {
                return new MemberDb();
        }


        QStringList MemberDb::keys() const
        {
                QStringList list;
                QList<MemberField> fields;
                QString error;
                if ( !MemberDatabase::instance()->allFields( fields, &error ) )
                {
                        // Fall back to the built-in keys so templates still show field names
                        fields = MemberDatabase::standardFields();
                }
                for ( const auto& f : fields )
                {
                        list << f.key;
                }
                list << MemberDatabase::idKey();
                return list;
        }


        QString MemberDb::primaryKey() const
        {
                return MemberDatabase::idKey();
        }


        void MemberDb::open()
        {
                mPending.clear();
                mNext = 0;

                QList<Member> members;
                QString error;
                if ( !MemberDatabase::instance()->members( members, false, &error ) )
                {
                        // Error already logged by MemberDatabase; produce no records
                        return;
                }

                for ( const auto& m : members )
                {
                        Record record;
                        for ( auto it = m.values.constBegin(); it != m.values.constEnd(); ++it )
                        {
                                record[it.key()] = it.value();
                        }
                        record[MemberDatabase::idKey()] = QString::number( m.id );
                        // Safety: never print every member by accident. Members are chosen
                        // explicitly on the Members page (or with "select all" on the Merge page).
                        record.setSelected( false );
                        mPending.append( record );
                }
        }


        void MemberDb::close()
        {
                mPending.clear();
                mNext = 0;
        }


        Record MemberDb::readNextRecord()
        {
                if ( mNext < mPending.size() )
                {
                        return mPending[mNext++];
                }
                return NullRecord();
        }

}
