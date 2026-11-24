//  MemberDialog.hpp
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

#ifndef MemberDialog_hpp
#define MemberDialog_hpp


#include "merge/MemberDatabase.hpp"

#include <QDialog>
#include <QList>
#include <QMap>

class QCheckBox;
class QLabel;
class QWidget;


namespace glabels
{

        ///
        /// Add / edit one member; saves to the database on OK
        ///
        class MemberDialog : public QDialog
        {
                Q_OBJECT

        public:
                MemberDialog( const QList<merge::MemberField>& fields,
                              const merge::Member&             member,
                              QWidget*                         parent = nullptr );

                const merge::Member& member() const;

                /// Validation used by the dialog; returns an empty string when valid
                static QString validate( const merge::Member& member );

        public slots:
                void accept() override;

        private:
                QString fieldText( const QString& key ) const;

                QList<merge::MemberField> mFields;
                merge::Member             mMember;
                QMap<QString,QWidget*>    mEditors;
                QCheckBox*                mActiveCheck;
                QLabel*                   mErrorLabel;
        };

}


#endif // MemberDialog_hpp
