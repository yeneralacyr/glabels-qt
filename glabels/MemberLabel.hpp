//  MemberLabel.hpp
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

#ifndef MemberLabel_hpp
#define MemberLabel_hpp


#include <QString>

class QWidget;


namespace glabels
{

        namespace model { class Template; }


        ///
        /// The remembered member address label design (a normal .glabels file)
        ///
        namespace MemberLabel
        {
                /// Remembered label file, or the default location if none has been created yet
                QString fileName();

                /// Ready-made address label (company, contact, address, district/city) for a product
                bool createDefault( const model::Template& tmplate, const QString& fileName, QString* error );

                /// Return the label file, creating it on first use after asking for the paper type.
                /// Returns an empty string if the user cancelled (error stays empty) or on failure.
                QString ensure( QWidget* parent, QString* error );
        }

}


#endif // MemberLabel_hpp
