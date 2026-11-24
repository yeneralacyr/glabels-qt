//  MemberMaintenance.hpp
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

#ifndef MemberMaintenance_hpp
#define MemberMaintenance_hpp


#include <QString>

class QWidget;


namespace glabels
{

        ///
        /// Program start / exit housekeeping for the member database
        ///
        namespace MemberMaintenance
        {
                /// Open + integrity check (offers restoring the newest automatic backup if the
                /// database is damaged), then the daily automatic backup.
                void startup( QWidget* parent );

                /// Automatic backup at exit if anything changed
                void shutdown();

                /// Last notice from automatic backups (e.g. fallback folder used); empty if none
                QString lastNotice();
                void clearNotice();
        }

}


#endif // MemberMaintenance_hpp
