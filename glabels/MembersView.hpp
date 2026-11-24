//  MembersView.hpp
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

#ifndef MembersView_hpp
#define MembersView_hpp


#include "merge/MemberDatabase.hpp"

#include <QList>
#include <QMap>
#include <QModelIndex>
#include <QSet>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QStandardItem;
class QStandardItemModel;
class QTableView;
class QToolButton;


namespace glabels
{

        class MemberFilterProxy;


        ///
        /// Members page: local member database list, search, filters and actions
        ///
        class MembersView : public QWidget
        {
                Q_OBJECT

                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                explicit MembersView( QWidget* parent = nullptr );


                /////////////////////////////////
                // Signals
                /////////////////////////////////
        signals:
                /// Print labels for these members using the program's Print page
                void printLabelsRequested( const QList<qint64>& memberIds );

                /// The user changed which members are selected (check marks)
                void selectionEdited();


                /////////////////////////////////
                // Public
                /////////////////////////////////
        public:
                /// Turkish-aware, accent-insensitive folding used for search
                static QString foldForSearch( const QString& text );

                /// Selected member ids in display order
                QList<qint64> selectedMemberIds() const;

                /// Take over the print selection made elsewhere (Print page, Merge page).
                /// printable = members that can be printed (active); checks on other members are kept.
                void syncFromPrintSelection( const QSet<qint64>& selected, const QSet<qint64>& printable );


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        public slots:
                void refresh();

        private slots:
                void onFilterChanged();
                void onItemChanged( QStandardItem* item );
                void onRowClicked( const QModelIndex& index );
                void onDoubleClicked( const QModelIndex& index );
                void onNewClicked();
                void onEditClicked();
                void onDeleteClicked();
                void onToggleActiveClicked();
                void onCheckVisibleClicked();
                void onClearChecksClicked();
                void onCustomFieldsClicked();
                void onBackupClicked();
                void onRestoreClicked();
                void onChangeBackupFolderClicked();
                void onOpenBackupFolderClicked();
                void updateBackupStatus();
                void onPrintLabelsClicked();
                void onPrintListClicked();
                void updateActions();


                /////////////////////////////////
                // Private
                /////////////////////////////////
        private:
                QList<qint64> checkedIds() const;          // in current display order
                QList<merge::Member> checkedMembers() const;
                qint64 currentId() const;
                int column( const QString& key ) const;
                void fillFilterCombo( QComboBox* combo, const QString& key );
                void editMember( qint64 id );
                void showError( const QString& title, const QString& message );
                bool confirmHiddenSelection( const QString& what );

                QLineEdit*           mSearchEdit;
                QComboBox*           mGroupCombo;
                QComboBox*           mIlceCombo;
                QCheckBox*           mShowPassiveCheck;
                QPushButton*         mNewButton;
                QPushButton*         mEditButton;
                QPushButton*         mDeleteButton;
                QPushButton*         mToggleActiveButton;
                QPushButton*         mCheckVisibleButton;
                QPushButton*         mClearChecksButton;
                QToolButton*         mToolsButton;
                QPushButton*         mPrintListButton;
                QPushButton*         mPrintLabelsButton;
                QStackedWidget*      mStack;
                QTableView*          mTable;
                QLabel*              mMessageLabel;
                QPushButton*         mRetryButton;
                QLabel*              mStatusLabel;
                QLabel*              mPathLabel;
                QLabel*              mBackupLabel;

                QStandardItemModel*  mItems;
                MemberFilterProxy*   mProxy;

                QList<merge::MemberField>  mFields;
                QList<merge::MemberField>  mColumns;      // data columns after the check column
                QMap<qint64,merge::Member> mMembers;
                QSet<qint64>               mChecked;
                bool                       mLoadOk{ false };
                bool                       mSortInitialized{ false };
                bool                       mRefreshing{ false };
        };

}


#endif // MembersView_hpp
