//  MemberFieldsPanel.hpp
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

#ifndef MemberFieldsPanel_hpp
#define MemberFieldsPanel_hpp


#include "merge/Record.hpp"

#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;


namespace glabels
{

        namespace model { class Model; }


        ///
        /// Editor side panel: member fields to insert and an optional preview member
        ///
        class MemberFieldsPanel : public QWidget
        {
                Q_OBJECT

        public:
                explicit MemberFieldsPanel( QWidget* parent = nullptr );

                void setModel( model::Model* model );

                /// Show this member's data on the design (0 = show field codes)
                void setPreviewMember( qint64 memberId );

                bool hasPreview() const;
                merge::Record previewRecord() const;

        signals:
                void fieldActivated( const QString& key );
                void previewChanged();
                void printRequested();

        private slots:
                void reload();
                void onModelMergeChanged();
                void onItemActivated( QListWidgetItem* item );
                void onPreviewIndexChanged();
                void onConnectClicked();
                void updateSelectionInfo();

        private:
                model::Model*   mModel{ nullptr };
                QStackedWidget* mStack;
                QListWidget*    mList;
                QComboBox*      mPreviewCombo;
                QLabel*         mErrorLabel;
                QLabel*         mOtherSourceLabel;
                QPushButton*    mConnectButton;
                QLabel*         mSelectionLabel;
                QPushButton*    mPrintButton;
                merge::Record   mPreview;
                bool            mHasPreview{ false };
        };

}


#endif // MemberFieldsPanel_hpp
