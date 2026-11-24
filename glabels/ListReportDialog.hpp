//  ListReportDialog.hpp
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

#ifndef ListReportDialog_hpp
#define ListReportDialog_hpp


#include "merge/MemberDatabase.hpp"

#include <QDialog>
#include <QList>
#include <QStringList>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPrinter;
class QPushButton;


namespace glabels
{

        ///
        /// Print / PDF / Excel (CSV) list of members with user-chosen columns
        ///
        class ListReportDialog : public QDialog
        {
                Q_OBJECT

        public:
                ListReportDialog( const QList<merge::Member>&      members,
                                  const QList<merge::MemberField>& fields,
                                  QWidget*                         parent = nullptr );

                /// HTML table used for printing (exposed for testing)
                static QString buildHtml( const QString&                   title,
                                          const QList<merge::Member>&      members,
                                          const QList<merge::MemberField>& columns,
                                          bool                             numbered );

                /// CSV for Excel (UTF-8 with BOM, ';' separated), exposed for testing
                static QByteArray buildCsv( const QList<merge::Member>&      members,
                                            const QList<merge::MemberField>& columns,
                                            bool                             numbered );

        private slots:
                void onPreviewClicked();
                void onPdfClicked();
                void onExcelClicked();
                void updateButtons();

        private:
                QList<merge::MemberField> chosenColumns( bool* numbered ) const;
                void renderTo( QPrinter* printer ) const;
                void showError( const QString& message );

                QList<merge::Member>      mMembers;
                QList<merge::MemberField> mFields;

                QLineEdit*   mTitleEdit;
                QListWidget* mColumnList;
                QComboBox*   mOrientationCombo;
                QLabel*      mInfoLabel;
                QPushButton* mPreviewButton;
                QPushButton* mPdfButton;
                QPushButton* mExcelButton;
        };

}


#endif // ListReportDialog_hpp
