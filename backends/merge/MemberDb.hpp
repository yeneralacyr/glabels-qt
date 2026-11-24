//  MemberDb.hpp
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

#ifndef merge_MemberDb_hpp
#define merge_MemberDb_hpp


#include "Merge.hpp"
#include "Record.hpp"

#include <QList>


namespace glabels::merge
{

        ///
        /// Merge backend reading active members from the local member database
        ///
        struct MemberDb : public Merge
        {

                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                MemberDb();
                MemberDb( const MemberDb* merge );
                ~MemberDb() override = default;


                /////////////////////////////////
                // Object duplication
                /////////////////////////////////
                MemberDb* clone() const override;


                /////////////////////////////////
                // Static methods
                /////////////////////////////////
        public:
                static QString id();
                static Merge* create();



                /////////////////////////////////
                // Implementation of virtual methods
                /////////////////////////////////
        public:
                QStringList keys() const override;
                QString primaryKey() const override;
        protected:
                void open() override;
                void close() override;
                Record readNextRecord() override;


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                QList<Record> mPending;
                int           mNext{ 0 };

        };

}


#endif // merge_MemberDb_hpp
