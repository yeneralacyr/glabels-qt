//  MemberPickerDialog.cpp
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

#include "MemberPickerDialog.hpp"

#include "CheckMarkDelegate.hpp"
#include "MembersView.hpp"

#include "merge/Merge.hpp"
#include "merge/MemberDatabase.hpp"

#include <QCheckBox>
#include <QCollator>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>


namespace
{
        const int KEY_ROLE    = Qt::UserRole + 1;   // record primary key (check column)
        const int SEARCH_ROLE = Qt::UserRole + 2;   // folded text of all fields (check column)
        const int GROUP_ROLE  = Qt::UserRole + 3;
        const int ILCE_ROLE   = Qt::UserRole + 4;

        const int CHECK_COLUMN = 0;

        struct Column { const char* key; const char* title; };
        const Column COLUMNS[] = { { "FIRMA", "Firma Ünvanı" }, { "PERSON", "Yetkili" }, { "ILCE", "İlçe" },
                                   { "MESLEK_GRUBU", "Meslek Grubu" }, { "TELEFON", "Telefon" } };

        const QString ALL_TEXT = QStringLiteral( "Tümü" );
}


namespace glabels
{

        ///
        /// Filter (search terms, group, ilçe, only selected) + Turkish-aware sorting
        ///
        class MemberPickerProxy : public QSortFilterProxyModel
        {
        public:
                using QSortFilterProxyModel::QSortFilterProxyModel;

                void setFilter( const QStringList& terms, const QString& group, const QString& ilce, bool onlySelected )
                {
                        mTerms = terms;
                        mGroup = group;
                        mIlce = ilce;
                        mOnlySelected = onlySelected;
#if QT_VERSION >= QT_VERSION_CHECK( 6, 10, 0 )
                        beginFilterChange();
                        endFilterChange( QSortFilterProxyModel::Direction::Rows );
#else
                        invalidateFilter();   // Windows CI builds with Qt 6.2
#endif
                }

                /// Checked rows are tinted across the whole row
                QVariant data( const QModelIndex& index, int role ) const override
                {
                        if ( role == Qt::BackgroundRole || role == Qt::ForegroundRole )
                        {
                                if ( index.sibling( index.row(), CHECK_COLUMN ).data( Qt::CheckStateRole ).toInt() == Qt::Checked )
                                {
                                        return role == Qt::BackgroundRole ? QVariant( QColor( 0xd6, 0xe8, 0xff ) )
                                                                          : QVariant( QColor( Qt::black ) );
                                }
                        }
                        return QSortFilterProxyModel::data( index, role );
                }

        protected:
                bool filterAcceptsRow( int sourceRow, const QModelIndex& sourceParent ) const override
                {
                        QModelIndex idx = sourceModel()->index( sourceRow, CHECK_COLUMN, sourceParent );
                        if ( mOnlySelected && idx.data( Qt::CheckStateRole ).toInt() != Qt::Checked )
                        {
                                return false;
                        }
                        if ( !mGroup.isNull() && idx.data( GROUP_ROLE ).toString() != mGroup )
                        {
                                return false;
                        }
                        if ( !mIlce.isNull() && idx.data( ILCE_ROLE ).toString() != mIlce )
                        {
                                return false;
                        }
                        QString haystack = idx.data( SEARCH_ROLE ).toString();
                        for ( const auto& t : mTerms )
                        {
                                if ( !haystack.contains( t ) )
                                {
                                        return false;
                                }
                        }
                        return true;
                }

                bool lessThan( const QModelIndex& left, const QModelIndex& right ) const override
                {
                        if ( left.column() == CHECK_COLUMN )
                        {
                                return left.data( Qt::CheckStateRole ).toInt() < right.data( Qt::CheckStateRole ).toInt();
                        }
                        static QCollator collator = []{
                                QCollator c{ QLocale( QLocale::Turkish ) };
                                c.setCaseSensitivity( Qt::CaseInsensitive );
                                c.setNumericMode( true );
                                return c;
                        }();
                        int c = collator.compare( left.data().toString(), right.data().toString() );
                        return c != 0 ? c < 0 : left.row() < right.row();
                }

        private:
                QStringList mTerms;
                QString     mGroup;   // null = all
                QString     mIlce;    // null = all
                bool        mOnlySelected{ false };
        };


        MemberPickerDialog::MemberPickerDialog( merge::Merge* merge, QWidget* parent )
                : QDialog( parent ), mMerge( merge )
        {
                setWindowTitle( "Basılacak Üyeleri Seç" );

                auto* hint = new QLabel( "Etiketi basılacak üyeleri seçin: satıra tıklamak seçer / kaldırır. "
                                         "Pasif üyeler listede yer almaz." );
                hint->setWordWrap( true );

                // Filters
                mSearchEdit = new QLineEdit();
                mSearchEdit->setPlaceholderText( "Ara: firma, yetkili, telefon, adres…" );
                mSearchEdit->setClearButtonEnabled( true );
                mGroupCombo = new QComboBox();
                mIlceCombo = new QComboBox();
                mOnlySelectedCheck = new QCheckBox( "Sadece seçilenleri göster" );

                auto* filterRow = new QHBoxLayout();
                filterRow->addWidget( mSearchEdit, 1 );
                filterRow->addSpacing( 8 );
                filterRow->addWidget( new QLabel( "Meslek Grubu:" ) );
                filterRow->addWidget( mGroupCombo );
                filterRow->addWidget( new QLabel( "İlçe:" ) );
                filterRow->addWidget( mIlceCombo );

                // Table
                mItems = new QStandardItemModel( this );
                QStringList headers{ QStringLiteral( "✓" ) };
                for ( const auto& c : COLUMNS )
                {
                        headers << QString::fromUtf8( c.title );
                }
                mItems->setHorizontalHeaderLabels( headers );

                QString idKey = merge::MemberDatabase::idKey();
                for ( const auto& record : merge->recordList() )
                {
                        QList<QStandardItem*> row;
                        auto* check = new QStandardItem();
                        check->setCheckable( true );
                        check->setEditable( false );
                        check->setCheckState( record.isSelected() ? Qt::Checked : Qt::Unchecked );
                        check->setData( record.value( idKey ), KEY_ROLE );
                        QStringList all;
                        for ( auto it = record.constBegin(); it != record.constEnd(); ++it )
                        {
                                all << it.value();
                        }
                        check->setData( MembersView::foldForSearch( all.join( ' ' ) ), SEARCH_ROLE );
                        check->setData( record.value( "MESLEK_GRUBU" ).trimmed(), GROUP_ROLE );
                        check->setData( record.value( "ILCE" ).trimmed(), ILCE_ROLE );
                        row << check;
                        for ( const auto& c : COLUMNS )
                        {
                                QString key = QString::fromUtf8( c.key );
                                QString v = ( key == "PERSON" )
                                        ? ( record.value( "AD" ) + " " + record.value( "SOYAD" ) ).trimmed()
                                        : record.value( key );
                                auto* item = new QStandardItem( v.replace( '\n', ' ' ) );
                                item->setEditable( false );
                                row << item;
                        }
                        mItems->appendRow( row );
                }

                mProxy = new MemberPickerProxy( this );
                mProxy->setSourceModel( mItems );

                mTable = new QTableView();
                mTable->setModel( mProxy );
                mTable->setItemDelegateForColumn( CHECK_COLUMN, new CheckMarkDelegate( mTable ) );
                mTable->setSelectionBehavior( QAbstractItemView::SelectRows );
                mTable->setSelectionMode( QAbstractItemView::SingleSelection );
                mTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
                mTable->setSortingEnabled( true );
                mTable->sortByColumn( 1, Qt::AscendingOrder );
                mTable->verticalHeader()->setVisible( false );
                mTable->horizontalHeader()->setStretchLastSection( true );
                mTable->setWordWrap( false );
                mTable->resizeColumnsToContents();
                mTable->setColumnWidth( CHECK_COLUMN, 36 );
                for ( int c = 1; c < mItems->columnCount(); c++ )
                {
                        mTable->setColumnWidth( c, std::min( mTable->columnWidth( c ), 240 ) );
                }
                {
                        // Soft current-row highlight that does not hide the check marks
                        QPalette pal = mTable->palette();
                        pal.setColor( QPalette::Highlight, QColor( 0xa9, 0xc8, 0xf0 ) );
                        pal.setColor( QPalette::HighlightedText, QColor( Qt::black ) );
                        mTable->setPalette( pal );
                }

                fillCombo( mGroupCombo, "MESLEK_GRUBU" );
                fillCombo( mIlceCombo, "ILCE" );

                // Bulk actions + count
                auto* selectAll = new QPushButton( "Görünenlerin Hepsini Seç" );
                auto* clearAll = new QPushButton( "Görünenlerin Seçimini Kaldır" );
                mCountLabel = new QLabel();
                auto* row = new QHBoxLayout();
                row->addWidget( selectAll );
                row->addWidget( clearAll );
                row->addSpacing( 12 );
                row->addWidget( mOnlySelectedCheck );
                row->addStretch( 1 );
                row->addWidget( mCountLabel );

                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
                buttons->button( QDialogButtonBox::Ok )->setText( "Tamam" );
                buttons->button( QDialogButtonBox::Cancel )->setText( "Vazgeç" );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( hint );
                layout->addLayout( filterRow );
                layout->addWidget( mTable, 1 );
                layout->addLayout( row );
                layout->addWidget( buttons );
                resize( 860, 600 );

                connect( mTable, &QTableView::clicked, this, &MemberPickerDialog::onRowClicked );
                connect( mItems, &QStandardItemModel::itemChanged, this, [this] {
                        mTable->viewport()->update();
                        updateCount();
                } );
                connect( mSearchEdit, &QLineEdit::textChanged, this, &MemberPickerDialog::applyFilter );
                connect( mGroupCombo, &QComboBox::currentIndexChanged, this, &MemberPickerDialog::applyFilter );
                connect( mIlceCombo, &QComboBox::currentIndexChanged, this, &MemberPickerDialog::applyFilter );
                connect( mOnlySelectedCheck, &QCheckBox::toggled, this, &MemberPickerDialog::applyFilter );
                connect( selectAll, &QPushButton::clicked, this, [this] { setAllVisible( true ); } );
                connect( clearAll, &QPushButton::clicked, this, [this] { setAllVisible( false ); } );
                connect( buttons, &QDialogButtonBox::accepted, this, &MemberPickerDialog::accept );
                connect( buttons, &QDialogButtonBox::rejected, this, &MemberPickerDialog::reject );

                updateCount();
                mSearchEdit->setFocus();
        }


        void MemberPickerDialog::fillCombo( QComboBox* combo, const QString& key )
        {
                QSet<QString> values;
                for ( const auto& record : mMerge->recordList() )
                {
                        QString v = record.value( key ).trimmed();
                        if ( !v.isEmpty() )
                        {
                                values.insert( v );
                        }
                }
                QStringList sorted( values.begin(), values.end() );
                QCollator collator{ QLocale( QLocale::Turkish ) };
                collator.setCaseSensitivity( Qt::CaseInsensitive );
                std::sort( sorted.begin(), sorted.end(),
                           [&collator]( const QString& a, const QString& b ) { return collator.compare( a, b ) < 0; } );
                combo->addItem( ALL_TEXT );
                combo->addItems( sorted );
                combo->setEnabled( !sorted.isEmpty() );
        }


        void MemberPickerDialog::applyFilter()
        {
                QStringList terms = MembersView::foldForSearch( mSearchEdit->text() ).split( ' ', Qt::SkipEmptyParts );
                QString group = mGroupCombo->currentIndex() > 0 ? mGroupCombo->currentText() : QString();
                QString ilce = mIlceCombo->currentIndex() > 0 ? mIlceCombo->currentText() : QString();
                mProxy->setFilter( terms, group, ilce, mOnlySelectedCheck->isChecked() );
                updateCount();
        }


        void MemberPickerDialog::onRowClicked( const QModelIndex& index )
        {
                if ( !index.isValid() )
                {
                        return;
                }
                QStandardItem* item = mItems->item( mProxy->mapToSource( index ).row(), CHECK_COLUMN );
                item->setCheckState( item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked );
                if ( mOnlySelectedCheck->isChecked() )
                {
                        applyFilter();   // an unselected row leaves the "only selected" view
                }
        }


        void MemberPickerDialog::setAllVisible( bool checked )
        {
                QList<QStandardItem*> visible;
                for ( int r = 0; r < mProxy->rowCount(); r++ )
                {
                        visible << mItems->item( mProxy->mapToSource( mProxy->index( r, CHECK_COLUMN ) ).row(), CHECK_COLUMN );
                }
                for ( QStandardItem* item : visible )
                {
                        item->setCheckState( checked ? Qt::Checked : Qt::Unchecked );
                }
                if ( mOnlySelectedCheck->isChecked() )
                {
                        applyFilter();
                }
        }


        void MemberPickerDialog::updateCount()
        {
                int n = 0;
                for ( int r = 0; r < mItems->rowCount(); r++ )
                {
                        if ( mItems->item( r, CHECK_COLUMN )->checkState() == Qt::Checked )
                        {
                                n++;
                        }
                }
                QString text = QString( "<b>%1 üye seçili</b>" ).arg( n );
                if ( mProxy->rowCount() != mItems->rowCount() )
                {
                        text += QString( " · %1 / %2 gösteriliyor" ).arg( mProxy->rowCount() ).arg( mItems->rowCount() );
                }
                mCountLabel->setText( text );
        }


        void MemberPickerDialog::accept()
        {
                QStringList keys;
                for ( int r = 0; r < mItems->rowCount(); r++ )
                {
                        QStandardItem* item = mItems->item( r, CHECK_COLUMN );
                        if ( item->checkState() == Qt::Checked )
                        {
                                keys << item->data( KEY_ROLE ).toString();
                        }
                }
                mMerge->selectOnlyKeys( keys );
                QDialog::accept();
        }

}
