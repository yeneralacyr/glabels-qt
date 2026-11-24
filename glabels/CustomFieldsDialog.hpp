//  CustomFieldsDialog.hpp
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

#ifndef CustomFieldsDialog_hpp
#define CustomFieldsDialog_hpp


#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;


namespace glabels
{

        ///
        /// Manage user-defined member fields
        ///
        class CustomFieldsDialog : public QDialog
        {
                Q_OBJECT

        public:
                explicit CustomFieldsDialog( QWidget* parent = nullptr );

        private slots:
                void reload();
                void onLabelEdited( const QString& text );
                void onKeyEdited();
                void onAddClicked();
                void onRemoveClicked();
                void updateButtons();

        private:
                void showMessage( const QString& text, bool isError );

                QListWidget* mList;
                QLineEdit*   mLabelEdit;
                QLineEdit*   mKeyEdit;
                QPushButton* mAddButton;
                QPushButton* mRemoveButton;
                QLabel*      mMessageLabel;
                bool         mKeyTouched{ false };
        };

}


#endif // CustomFieldsDialog_hpp
