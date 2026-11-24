//  MemberPrintDialog.hpp
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

#ifndef MemberPrintDialog_hpp
#define MemberPrintDialog_hpp


#include <QDialog>

class QPixmap;


namespace glabels
{

        namespace model { class Model; }


        ///
        /// Shown after "Etiket Bas": summary of what will be printed, a preview of the first
        /// member's label, and a choice between designing the label and going to the Print page.
        ///
        class MemberPrintDialog : public QDialog
        {
                Q_OBJECT

        public:
                enum class Choice { Cancel, Design, Print };

                MemberPrintDialog( model::Model* model, int nMembers, int nSkippedPassive, QWidget* parent = nullptr );

                Choice choice() const;

                /// Label drawn with the first selected record, scaled to fit maxSize (empty when no model)
                static QPixmap renderPreview( model::Model* model, const QSize& maxSize );

        private:
                Choice mChoice{ Choice::Cancel };
        };

}


#endif // MemberPrintDialog_hpp
