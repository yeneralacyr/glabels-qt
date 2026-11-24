//  MemberPickerDialog.hpp
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

#ifndef MemberPickerDialog_hpp
#define MemberPickerDialog_hpp


#include <QDialog>
#include <QModelIndex>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QStandardItemModel;
class QTableView;


namespace glabels
{

        namespace merge { class Merge; }

        class MemberPickerProxy;


        ///
        /// Choose which members (records of the member-database merge) get printed,
        /// without leaving the Print page: search, Meslek Grubu / İlçe filters, sortable
        /// columns. Applies the choice to the merge on OK.
        ///
        class MemberPickerDialog : public QDialog
        {
                Q_OBJECT

        public:
                MemberPickerDialog( merge::Merge* merge, QWidget* parent = nullptr );

        public slots:
                void accept() override;

        private slots:
                void applyFilter();
                void updateCount();
                void setAllVisible( bool checked );
                void onRowClicked( const QModelIndex& index );

        private:
                void fillCombo( QComboBox* combo, const QString& key );

                merge::Merge*       mMerge;
                QLineEdit*          mSearchEdit;
                QComboBox*          mGroupCombo;
                QComboBox*          mIlceCombo;
                QCheckBox*          mOnlySelectedCheck;
                QTableView*         mTable;
                QStandardItemModel* mItems;
                MemberPickerProxy*  mProxy;
                QLabel*             mCountLabel;
        };

}


#endif // MemberPickerDialog_hpp
