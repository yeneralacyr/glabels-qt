//  MembersView.cpp
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

#include "MembersView.hpp"

#include "CustomFieldsDialog.hpp"
#include "ListReportDialog.hpp"
#include "MemberDialog.hpp"
#include "CheckMarkDelegate.hpp"
#include "MemberMaintenance.hpp"

#include "merge/MemberBackup.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QCollator>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPolygonF>
#include <QPushButton>
#include <QResizeEvent>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>


namespace
{
        const int ID_ROLE     = Qt::UserRole + 1;   // member id (check column)
        const int SEARCH_ROLE = Qt::UserRole + 2;   // folded text of all fields (check column)
        const int GROUP_ROLE  = Qt::UserRole + 3;   // MESLEK_GRUBU (check column)
        const int ILCE_ROLE   = Qt::UserRole + 4;   // ILCE (check column)

        const int CHECK_COLUMN = 0;

        // Data columns shown in the table (all fields remain searchable)
        const QStringList LIST_KEYS = { "SICIL_NO", "FIRMA", "AD", "SOYAD", "MESLEK_GRUBU",
                                        "TELEFON", "EPOSTA", "ILCE", "IL" };

        const QString ALL_TEXT = QStringLiteral( "Tümü" );
}


namespace glabels
{

        ///
        /// Proxy filtering rows by search terms and the group / district filters
        ///
        ///
        /// Label that shortens its text with "…" instead of painting past its width
        ///
        class ElidedLabel : public QLabel
        {
        public:
                using QLabel::QLabel;

                void setFullText( const QString& text )
                {
                        mFull = text;
                        updateText();
                }

        protected:
                void resizeEvent( QResizeEvent* event ) override
                {
                        QLabel::resizeEvent( event );
                        updateText();
                }

        private:
                void updateText()
                {
                        QLabel::setText( fontMetrics().elidedText( mFull, Qt::ElideMiddle, std::max( 0, width() - 2 ) ) );
                }

                QString mFull;
        };


        class MemberFilterProxy : public QSortFilterProxyModel
        {
        public:
                explicit MemberFilterProxy( QObject* parent ) : QSortFilterProxyModel( parent ) {}

                void setFilter( const QStringList& terms, const QString& group, const QString& ilce )
                {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 10, 0 )
                        beginFilterChange();
                        mTerms = terms;
                        mGroup = group;
                        mIlce = ilce;
                        endFilterChange( QSortFilterProxyModel::Direction::Rows );
#else
                        // Windows CI builds with Qt 6.2, which predates begin/endFilterChange()
                        mTerms = terms;
                        mGroup = group;
                        mIlce = ilce;
                        invalidateFilter();
#endif
                }

                /// Checked rows are tinted across the whole row, so it is obvious what will be printed
                QVariant data( const QModelIndex& index, int role ) const override
                {
                        if ( role == Qt::BackgroundRole || role == Qt::ForegroundRole )
                        {
                                QModelIndex checkIndex = index.sibling( index.row(), CHECK_COLUMN );
                                if ( checkIndex.data( Qt::CheckStateRole ).toInt() == Qt::Checked )
                                {
                                        if ( role == Qt::BackgroundRole )
                                        {
                                                return QColor( 0xd6, 0xe8, 0xff );
                                        }
                                        if ( !QSortFilterProxyModel::data( index, role ).isValid() )
                                        {
                                                return QColor( Qt::black );
                                        }
                                }
                        }
                        return QSortFilterProxyModel::data( index, role );
                }

        protected:
                bool filterAcceptsRow( int sourceRow, const QModelIndex& sourceParent ) const override
                {
                        QModelIndex idx = sourceModel()->index( sourceRow, CHECK_COLUMN, sourceParent );
                        if ( !mGroup.isNull() && idx.data( GROUP_ROLE ).toString() != mGroup )
                        {
                                return false;
                        }
                        if ( !mIlce.isNull() && idx.data( ILCE_ROLE ).toString() != mIlce )
                        {
                                return false;
                        }
                        QString haystack = idx.data( SEARCH_ROLE ).toString();
                        for ( const auto& term : mTerms )
                        {
                                if ( !haystack.contains( term ) )
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
                        // Turkish alphabetical order (Ç after C, İ after I, ...) and natural numbers
                        static QCollator collator = []{
                                QCollator c{ QLocale( QLocale::Turkish ) };
                                c.setCaseSensitivity( Qt::CaseInsensitive );
                                c.setNumericMode( true );
                                return c;
                        }();
                        int c = collator.compare( left.data().toString(), right.data().toString() );
                        if ( c != 0 )
                        {
                                return c < 0;
                        }
                        // Equal values (e.g. same İlçe): keep a stable order — the order members were added
                        return left.sibling( left.row(), CHECK_COLUMN ).data( ID_ROLE ).toLongLong()
                             < right.sibling( right.row(), CHECK_COLUMN ).data( ID_ROLE ).toLongLong();
                }

        private:
                QStringList mTerms;
                QString     mGroup;   // null = all
                QString     mIlce;    // null = all
        };


        ///
        /// Fold text so that "alacayir" finds "ALAÇAYIR" and "istanbul" finds "İSTANBUL"
        ///
        QString MembersView::foldForSearch( const QString& text )
        {
                QString s = QLocale( QLocale::Turkish ).toLower( text );
                static const QMap<QChar,QChar> map = {
                        { QChar( 0x00E7 ), 'c' },  // ç
                        { QChar( 0x011F ), 'g' },  // ğ
                        { QChar( 0x0131 ), 'i' },  // ı
                        { QChar( 0x00F6 ), 'o' },  // ö
                        { QChar( 0x015F ), 's' },  // ş
                        { QChar( 0x00FC ), 'u' },  // ü
                        { QChar( 0x00E2 ), 'a' },  // â
                        { QChar( 0x00EE ), 'i' },  // î
                        { QChar( 0x00FB ), 'u' },  // û
                        { QChar( 0x0307 ), QChar() },  // combining dot (from İ lowered by non-Turkish rules)
                };
                QString out;
                out.reserve( s.size() );
                for ( QChar c : s )
                {
                        auto it = map.find( c );
                        if ( it == map.end() )
                        {
                                out.append( c );
                        }
                        else if ( !it.value().isNull() )
                        {
                                out.append( it.value() );
                        }
                }
                return out;
        }


        ///
        /// Constructor
        ///
        MembersView::MembersView( QWidget* parent )
                : QWidget( parent )
        {
                auto* title = new QLabel( QString( "<span style='font-size:18pt;'>%1</span>" ).arg( "Üyeler" ) );

                // Filter row
                mSearchEdit = new QLineEdit();
                mSearchEdit->setPlaceholderText( "Ara: firma, yetkili, sicil no, telefon, adres…" );
                mSearchEdit->setClearButtonEnabled( true );
                mGroupCombo = new QComboBox();
                mGroupCombo->setSizeAdjustPolicy( QComboBox::AdjustToContents );
                mIlceCombo = new QComboBox();
                mIlceCombo->setSizeAdjustPolicy( QComboBox::AdjustToContents );
                mShowPassiveCheck = new QCheckBox( "Pasif üyeleri göster" );

                auto* filterRow = new QHBoxLayout();
                filterRow->addWidget( mSearchEdit, 1 );
                filterRow->addSpacing( 8 );
                filterRow->addWidget( new QLabel( "Meslek Grubu:" ) );
                filterRow->addWidget( mGroupCombo );
                filterRow->addSpacing( 8 );
                filterRow->addWidget( new QLabel( "İlçe:" ) );
                filterRow->addWidget( mIlceCombo );
                filterRow->addSpacing( 8 );
                filterRow->addWidget( mShowPassiveCheck );

                // Action row
                mNewButton          = new QPushButton( "Yeni Üye" );
                mEditButton         = new QPushButton( "Düzenle" );
                mDeleteButton       = new QPushButton( "Sil" );
                mToggleActiveButton = new QPushButton( "Pasif Yap" );
                mCheckVisibleButton = new QPushButton( "Tümünü Seç" );
                mClearChecksButton  = new QPushButton( "Seçimi Kaldır" );

                mToolsButton = new QToolButton();
                mToolsButton->setText( "Araçlar" );
                mToolsButton->setPopupMode( QToolButton::InstantPopup );
                mToolsButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
                auto* toolsMenu = new QMenu( mToolsButton );
                QAction* backupAction = toolsMenu->addAction( "Yedek Al…" );
                QAction* restoreAction = toolsMenu->addAction( "Yedekten Geri Yükle…" );
                toolsMenu->addSeparator();
                QAction* backupFolderAction = toolsMenu->addAction( "Otomatik Yedek Klasörünü Değiştir…" );
                QAction* openBackupFolderAction = toolsMenu->addAction( "Otomatik Yedek Klasörünü Aç" );
                toolsMenu->addSeparator();
                QAction* customFieldsAction = toolsMenu->addAction( "Ek Alanlar…" );
                mToolsButton->setMenu( toolsMenu );

                mPrintListButton   = new QPushButton( "Liste Yazdır…" );
                mPrintListButton->setToolTip( "Seçili üyeleri istediğiniz sütunlarla A4 liste olarak yazdırır, PDF veya Excel'e aktarır." );
                mPrintLabelsButton = new QPushButton( "Etiket Bas…" );
                mPrintLabelsButton->setDefault( true );
                mPrintLabelsButton->setToolTip( "Seçili üyelerin etiketini hazırlar: önizleme gösterir, tasarıma veya yazdırmaya yönlendirir." );

                auto* actionRow = new QHBoxLayout();
                actionRow->addWidget( mNewButton );
                actionRow->addWidget( mEditButton );
                actionRow->addWidget( mDeleteButton );
                actionRow->addWidget( mToggleActiveButton );
                actionRow->addSpacing( 16 );
                actionRow->addWidget( mCheckVisibleButton );
                actionRow->addWidget( mClearChecksButton );
                actionRow->addSpacing( 16 );
                actionRow->addWidget( mToolsButton );
                actionRow->addStretch( 1 );
                actionRow->addWidget( mPrintListButton );
                actionRow->addWidget( mPrintLabelsButton );

                // Table
                mItems = new QStandardItemModel( this );
                mProxy = new MemberFilterProxy( this );
                mProxy->setSourceModel( mItems );

                mTable = new QTableView();
                mTable->setModel( mProxy );
                mTable->setSelectionBehavior( QAbstractItemView::SelectRows );
                mTable->setSelectionMode( QAbstractItemView::SingleSelection );
                mTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
                mTable->setSortingEnabled( true );
                mTable->setAlternatingRowColors( true );
                mTable->setWordWrap( false );
                mTable->verticalHeader()->setVisible( false );
                mTable->horizontalHeader()->setStretchLastSection( true );
                mTable->setItemDelegateForColumn( CHECK_COLUMN, new CheckMarkDelegate( mTable ) );

                // The current-row highlight must not hide the check boxes: use a soft outline-like
                // colour with dark text instead of the strong system highlight
                {
                        QPalette pal = mTable->palette();
                        pal.setColor( QPalette::Highlight, QColor( 0xa9, 0xc8, 0xf0 ) );
                        pal.setColor( QPalette::HighlightedText, QColor( Qt::black ) );
                        mTable->setPalette( pal );
                }

                // Message page (empty database / errors)
                mMessageLabel = new QLabel();
                mMessageLabel->setAlignment( Qt::AlignCenter );
                mMessageLabel->setWordWrap( true );
                mMessageLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
                mRetryButton = new QPushButton( "Tekrar Dene" );
                auto* messagePage = new QWidget();
                auto* messageLayout = new QVBoxLayout( messagePage );
                messageLayout->addStretch( 1 );
                messageLayout->addWidget( mMessageLabel );
                messageLayout->addWidget( mRetryButton, 0, Qt::AlignHCenter );
                messageLayout->addStretch( 1 );

                mStack = new QStackedWidget();
                mStack->addWidget( mTable );       // 0
                mStack->addWidget( messagePage );  // 1

                // Status
                mStatusLabel = new QLabel();
                mPathLabel = new ElidedLabel();
                mPathLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
                mPathLabel->setForegroundRole( QPalette::PlaceholderText );
                // Long paths must not force the window wider; full path is in the tooltip
                mPathLabel->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
                mPathLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
                mBackupLabel = new QLabel();
                mBackupLabel->setTextFormat( Qt::RichText );
                mBackupLabel->setTextInteractionFlags( Qt::LinksAccessibleByMouse );
                connect( mBackupLabel, &QLabel::linkActivated, this, [this]( const QString& link ) {
                        if ( link == QStringLiteral( "backup" ) )
                        {
                                onBackupClicked();
                        }
                } );

                auto* statusRow = new QHBoxLayout();
                statusRow->addWidget( mStatusLabel );
                statusRow->addSpacing( 16 );
                mBackupLabel->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Preferred );
                statusRow->addWidget( mBackupLabel );
                statusRow->addSpacing( 16 );
                statusRow->addWidget( mPathLabel, 1 );

                auto* layout = new QVBoxLayout( this );
                layout->setContentsMargins( 12, 12, 12, 12 );
                layout->addWidget( title );
                layout->addLayout( filterRow );
                layout->addLayout( actionRow );
                layout->addWidget( mStack, 1 );
                layout->addLayout( statusRow );

                // Connections
                connect( mSearchEdit, &QLineEdit::textChanged, this, &MembersView::onFilterChanged );
                connect( mGroupCombo, &QComboBox::currentIndexChanged, this, &MembersView::onFilterChanged );
                connect( mIlceCombo, &QComboBox::currentIndexChanged, this, &MembersView::onFilterChanged );
                connect( mShowPassiveCheck, &QCheckBox::toggled, this, &MembersView::refresh );
                connect( mNewButton, &QPushButton::clicked, this, &MembersView::onNewClicked );
                connect( mEditButton, &QPushButton::clicked, this, &MembersView::onEditClicked );
                connect( mDeleteButton, &QPushButton::clicked, this, &MembersView::onDeleteClicked );
                connect( mToggleActiveButton, &QPushButton::clicked, this, &MembersView::onToggleActiveClicked );
                connect( mCheckVisibleButton, &QPushButton::clicked, this, &MembersView::onCheckVisibleClicked );
                connect( mClearChecksButton, &QPushButton::clicked, this, &MembersView::onClearChecksClicked );
                connect( customFieldsAction, &QAction::triggered, this, &MembersView::onCustomFieldsClicked );
                connect( backupAction, &QAction::triggered, this, &MembersView::onBackupClicked );
                connect( restoreAction, &QAction::triggered, this, &MembersView::onRestoreClicked );
                connect( backupFolderAction, &QAction::triggered, this, &MembersView::onChangeBackupFolderClicked );
                connect( openBackupFolderAction, &QAction::triggered, this, &MembersView::onOpenBackupFolderClicked );
                connect( mPrintListButton, &QPushButton::clicked, this, &MembersView::onPrintListClicked );
                connect( mPrintLabelsButton, &QPushButton::clicked, this, &MembersView::onPrintLabelsClicked );
                connect( mRetryButton, &QPushButton::clicked, this, &MembersView::refresh );
                connect( mTable, &QTableView::doubleClicked, this, &MembersView::onDoubleClicked );
                connect( mTable, &QTableView::clicked, this, &MembersView::onRowClicked );
                connect( mTable->selectionModel(), &QItemSelectionModel::selectionChanged,
                         this, &MembersView::updateActions );
                connect( mItems, &QStandardItemModel::itemChanged, this, &MembersView::onItemChanged );
                connect( merge::MemberDatabase::instance(), &merge::MemberDatabase::changed,
                         this, &MembersView::refresh );

                QString dbPath = QDir::toNativeSeparators( merge::MemberDatabase::instance()->filePath() );
                static_cast<ElidedLabel*>( mPathLabel )->setFullText( QString( "Veritabanı: %1" ).arg( dbPath ) );
                mPathLabel->setToolTip( dbPath );

                refresh();
        }


        ///
        /// Column index (in the item model) of a field key, -1 if not shown
        ///
        int MembersView::column( const QString& key ) const
        {
                for ( int i = 0; i < mColumns.size(); i++ )
                {
                        if ( mColumns[i].key == key )
                        {
                                return i + 1;
                        }
                }
                return -1;
        }


        ///
        /// Reload everything from the database, keeping checks, filters and current row
        ///
        void MembersView::refresh()
        {
                qint64 current = currentId();

                auto db = merge::MemberDatabase::instance();
                QString error;
                QList<merge::Member> list;

                mLoadOk = db->allFields( mFields, &error )
                          && db->members( list, mShowPassiveCheck->isChecked(), &error );

                mRefreshing = true;
                mItems->clear();
                mMembers.clear();

                if ( !mLoadOk )
                {
                        mRefreshing = false;
                        mMessageLabel->setText( QString( "<b>Üye veritabanı açılamadı.</b><br><br>%1" ).arg( error.toHtmlEscaped() ) );
                        mRetryButton->setVisible( true );
                        mStack->setCurrentIndex( 1 );
                        updateActions();
                        return;
                }

                // Columns: check box, list keys, custom fields, status
                mColumns.clear();
                for ( const auto& key : LIST_KEYS )
                {
                        for ( const auto& f : mFields )
                        {
                                if ( f.key == key )
                                {
                                        mColumns << f;
                                }
                        }
                }
                for ( const auto& f : mFields )
                {
                        if ( merge::MemberDatabase::isValidCustomKey( f.key ) )
                        {
                                mColumns << f;
                        }
                }

                QStringList headers;
                headers << "✓";
                for ( const auto& c : mColumns )
                {
                        headers << c.label;
                }
                headers << "Durum";
                mItems->setHorizontalHeaderLabels( headers );

                QSet<qint64> stillThere;
                for ( const auto& m : list )
                {
                        mMembers.insert( m.id, m );
                        stillThere.insert( m.id );

                        QStringList all;
                        for ( auto it = m.values.constBegin(); it != m.values.constEnd(); ++it )
                        {
                                all << it.value();
                        }

                        QList<QStandardItem*> row;

                        auto* check = new QStandardItem();
                        check->setCheckable( true );
                        check->setEditable( false );
                        check->setCheckState( mChecked.contains( m.id ) ? Qt::Checked : Qt::Unchecked );
                        check->setData( m.id, ID_ROLE );
                        check->setData( foldForSearch( all.join( ' ' ) ), SEARCH_ROLE );
                        check->setData( m.values.value( "MESLEK_GRUBU" ).trimmed(), GROUP_ROLE );
                        check->setData( m.values.value( "ILCE" ).trimmed(), ILCE_ROLE );
                        check->setToolTip( "Etiket ve liste için seç (satırın herhangi bir yerine tıklayın)" );
                        row << check;

                        for ( const auto& c : mColumns )
                        {
                                QString v = m.values.value( c.key );
                                auto* item = new QStandardItem( QString( v ).replace( '\n', ' ' ) );
                                item->setToolTip( v );
                                row << item;
                        }
                        row << new QStandardItem( m.active ? "Aktif" : "Pasif" );

                        if ( !m.active )
                        {
                                for ( auto* item : row )
                                {
                                        item->setForeground( palette().brush( QPalette::Disabled, QPalette::Text ) );
                                }
                        }
                        mItems->appendRow( row );
                }

                // Forget checks of members that no longer exist (deleted) or are hidden as passive
                mChecked.intersect( stillThere );
                mRefreshing = false;

                updateBackupStatus();
                fillFilterCombo( mGroupCombo, "MESLEK_GRUBU" );
                fillFilterCombo( mIlceCombo, "ILCE" );
                onFilterChanged();

                if ( list.isEmpty() )
                {
                        mMessageLabel->setText( mShowPassiveCheck->isChecked()
                                                ? "Henüz kayıtlı üye yok.<br>Başlamak için <b>Yeni Üye</b> düğmesine tıklayın."
                                                : "Listede aktif üye yok.<br>Eklemek için <b>Yeni Üye</b> düğmesine tıklayın "
                                                  "veya pasif üyeleri görmek için <b>Pasif üyeleri göster</b>i işaretleyin." );
                        mRetryButton->setVisible( false );
                        mStack->setCurrentIndex( 1 );
                }
                else
                {
                        mStack->setCurrentIndex( 0 );
                        if ( !mSortInitialized && column( "FIRMA" ) > 0 )
                        {
                                // Default: by company name, A→Z
                                mTable->sortByColumn( column( "FIRMA" ), Qt::AscendingOrder );
                                mSortInitialized = true;
                        }
                        mTable->resizeColumnsToContents();
                        for ( int c = 1; c < mItems->columnCount(); c++ )
                        {
                                if ( mTable->columnWidth( c ) > 260 )
                                {
                                        mTable->setColumnWidth( c, 260 );
                                }
                        }

                        // Restore current row
                        for ( int r = 0; r < mProxy->rowCount(); r++ )
                        {
                                QModelIndex idx = mProxy->index( r, CHECK_COLUMN );
                                if ( idx.data( ID_ROLE ).toLongLong() == current )
                                {
                                        mTable->selectRow( r );
                                        break;
                                }
                        }
                }

                updateActions();
        }


        ///
        /// Fill a filter combo with the distinct values of a field, keeping the current choice
        ///
        void MembersView::fillFilterCombo( QComboBox* combo, const QString& key )
        {
                QString previous = combo->currentIndex() > 0 ? combo->currentData().toString() : QString();

                QStringList values;
                for ( const auto& m : mMembers )
                {
                        QString v = m.values.value( key ).trimmed();
                        if ( !v.isEmpty() && !values.contains( v ) )
                        {
                                values << v;
                        }
                }
                QCollator collator{ QLocale( QLocale::Turkish ) };
                collator.setCaseSensitivity( Qt::CaseInsensitive );
                std::sort( values.begin(), values.end(),
                           [&]( const QString& a, const QString& b ) { return collator.compare( a, b ) < 0; } );

                QSignalBlocker block( combo );
                combo->clear();
                combo->addItem( ALL_TEXT );
                for ( const auto& v : values )
                {
                        combo->addItem( v, v );
                }

                int idx = previous.isNull() ? 0 : combo->findData( previous );
                combo->setCurrentIndex( idx < 0 ? 0 : idx );
                combo->setEnabled( !values.isEmpty() );
        }


        void MembersView::onFilterChanged()
        {
                QStringList terms = foldForSearch( mSearchEdit->text() ).split( ' ', Qt::SkipEmptyParts );
                QString group = mGroupCombo->currentIndex() > 0 ? mGroupCombo->currentData().toString() : QString();
                QString ilce  = mIlceCombo->currentIndex() > 0 ? mIlceCombo->currentData().toString() : QString();
                mProxy->setFilter( terms, group, ilce );
                updateActions();
        }


        QList<qint64> MembersView::selectedMemberIds() const
        {
                return checkedIds();
        }


        void MembersView::syncFromPrintSelection( const QSet<qint64>& selected, const QSet<qint64>& printable )
        {
                QSet<qint64> wanted = mChecked - printable;
                wanted.unite( selected );
                if ( wanted == mChecked )
                {
                        return;
                }
                mChecked = wanted;
                mRefreshing = true;
                for ( int r = 0; r < mItems->rowCount(); r++ )
                {
                        auto* item = mItems->item( r, CHECK_COLUMN );
                        item->setCheckState( mChecked.contains( item->data( ID_ROLE ).toLongLong() ) ? Qt::Checked : Qt::Unchecked );
                }
                mRefreshing = false;
                mTable->viewport()->update();
                updateActions();
        }


        void MembersView::onItemChanged( QStandardItem* item )
        {
                if ( mRefreshing || item->column() != CHECK_COLUMN )
                {
                        return;
                }
                qint64 id = item->data( ID_ROLE ).toLongLong();
                if ( item->checkState() == Qt::Checked )
                {
                        mChecked.insert( id );
                }
                else
                {
                        mChecked.remove( id );
                }
                mTable->viewport()->update();   // re-tint the whole row
                updateActions();
                emit selectionEdited();
        }


        ///
        /// Checked member ids in the current display order (hidden checked rows follow at the end)
        ///
        QList<qint64> MembersView::checkedIds() const
        {
                QList<qint64> ids;
                QSet<qint64> seen;
                for ( int r = 0; r < mProxy->rowCount(); r++ )
                {
                        qint64 id = mProxy->index( r, CHECK_COLUMN ).data( ID_ROLE ).toLongLong();
                        if ( mChecked.contains( id ) )
                        {
                                ids << id;
                                seen.insert( id );
                        }
                }
                for ( int r = 0; r < mItems->rowCount(); r++ )
                {
                        qint64 id = mItems->item( r, CHECK_COLUMN )->data( ID_ROLE ).toLongLong();
                        if ( mChecked.contains( id ) && !seen.contains( id ) )
                        {
                                ids << id;
                        }
                }
                return ids;
        }


        QList<merge::Member> MembersView::checkedMembers() const
        {
                QList<merge::Member> list;
                for ( qint64 id : checkedIds() )
                {
                        list << mMembers.value( id );
                }
                return list;
        }


        qint64 MembersView::currentId() const
        {
                if ( !mTable->selectionModel() )
                {
                        return 0;
                }
                QModelIndexList rows = mTable->selectionModel()->selectedRows( CHECK_COLUMN );
                return rows.isEmpty() ? 0 : rows.first().data( ID_ROLE ).toLongLong();
        }


        void MembersView::updateActions()
        {
                bool ok = mLoadOk;
                int nChecked = mChecked.size();
                bool hasCurrent = currentId() != 0;

                mNewButton->setEnabled( ok );
                mToolsButton->setEnabled( ok );
                mSearchEdit->setEnabled( ok );
                mShowPassiveCheck->setEnabled( ok );
                mEditButton->setEnabled( ok && hasCurrent );
                mDeleteButton->setEnabled( ok && nChecked > 0 );
                mToggleActiveButton->setEnabled( ok && nChecked > 0 );
                mCheckVisibleButton->setEnabled( ok && mProxy->rowCount() > 0 );
                bool filtered = mProxy->rowCount() != mItems->rowCount();
                mCheckVisibleButton->setText( filtered ? QString( "Görünen %1 Üyeyi Seç" ).arg( mProxy->rowCount() )
                                                       : QString( "Tümünü Seç" ) );
                mCheckVisibleButton->setToolTip( filtered ? "Arama/filtreyle görünen üyelerin hepsini seçer"
                                                          : "Listedeki tüm üyeleri seçer" );
                mClearChecksButton->setEnabled( ok && nChecked > 0 );
                mPrintLabelsButton->setEnabled( ok && nChecked > 0 );
                mPrintListButton->setEnabled( ok && nChecked > 0 );

                QString hint = nChecked > 0 ? QString() : QString( "Önce listeden üyeleri seçin (satıra tıklayın)." );
                for ( QPushButton* b : { mDeleteButton, mToggleActiveButton, mPrintLabelsButton, mPrintListButton } )
                {
                        b->setStatusTip( hint );
                }

                // "Pasif Yap" unless every checked member is already passive
                bool allPassive = nChecked > 0;
                for ( qint64 id : mChecked )
                {
                        if ( mMembers.value( id ).active )
                        {
                                allPassive = false;
                                break;
                        }
                }
                mToggleActiveButton->setText( allPassive ? "Aktif Yap" : "Pasif Yap" );

                // Status line
                if ( !ok )
                {
                        mStatusLabel->setText( "Veritabanı hatası" );
                        return;
                }
                int total = mItems->rowCount();
                int visible = mProxy->rowCount();
                QString text = ( visible == total )
                        ? QString( "%1 üye" ).arg( total )
                        : QString( "%1 üyeden %2 tanesi gösteriliyor" ).arg( total ).arg( visible );
                if ( total > 0 && visible == 0 )
                {
                        text += " — filtreyle eşleşen üye yok";
                }
                if ( nChecked > 0 )
                {
                        int hiddenChecked = 0;
                        QSet<qint64> visibleIds;
                        for ( int r = 0; r < visible; r++ )
                        {
                                visibleIds.insert( mProxy->index( r, CHECK_COLUMN ).data( ID_ROLE ).toLongLong() );
                        }
                        for ( qint64 id : mChecked )
                        {
                                if ( !visibleIds.contains( id ) )
                                {
                                        hiddenChecked++;
                                }
                        }
                        text += QString( " · <b>%1 seçili</b>" ).arg( nChecked );
                        if ( hiddenChecked > 0 )
                        {
                                text += QString( " (%1 tanesi şu an filtre dışında)" ).arg( hiddenChecked );
                        }
                }
                mStatusLabel->setText( text );
        }


        void MembersView::onCheckVisibleClicked()
        {
                for ( int r = 0; r < mProxy->rowCount(); r++ )
                {
                        mChecked.insert( mProxy->index( r, CHECK_COLUMN ).data( ID_ROLE ).toLongLong() );
                }
                mRefreshing = true;
                for ( int r = 0; r < mItems->rowCount(); r++ )
                {
                        auto* item = mItems->item( r, CHECK_COLUMN );
                        item->setCheckState( mChecked.contains( item->data( ID_ROLE ).toLongLong() ) ? Qt::Checked : Qt::Unchecked );
                }
                mRefreshing = false;
                updateActions();
                emit selectionEdited();
        }


        void MembersView::onClearChecksClicked()
        {
                mChecked.clear();
                mRefreshing = true;
                for ( int r = 0; r < mItems->rowCount(); r++ )
                {
                        mItems->item( r, CHECK_COLUMN )->setCheckState( Qt::Unchecked );
                }
                mRefreshing = false;
                updateActions();
                emit selectionEdited();
        }


        ///
        /// A click anywhere on a row toggles its check mark (the box itself is small)
        ///
        void MembersView::onRowClicked( const QModelIndex& index )
        {
                if ( !index.isValid() )
                {
                        return;
                }
                QModelIndex checkIndex = mProxy->mapToSource( index.sibling( index.row(), CHECK_COLUMN ) );
                if ( QStandardItem* item = mItems->itemFromIndex( checkIndex ) )
                {
                        item->setCheckState( item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked );
                }
        }


        void MembersView::onDoubleClicked( const QModelIndex& index )
        {
                if ( index.isValid() && index.column() != CHECK_COLUMN )
                {
                        // The first click of the double click toggled the mark; undo that
                        onRowClicked( index );

                        editMember( mProxy->index( index.row(), CHECK_COLUMN ).data( ID_ROLE ).toLongLong() );
                }
        }


        void MembersView::onNewClicked()
        {
                editMember( 0 );
        }


        void MembersView::onEditClicked()
        {
                qint64 id = currentId();
                if ( id != 0 )
                {
                        editMember( id );
                }
        }


        void MembersView::editMember( qint64 id )
        {
                merge::Member member;
                if ( id != 0 )
                {
                        if ( !mMembers.contains( id ) )
                        {
                                showError( "Üye bulunamadı", "Bu üye artık listede yok. Liste yenileniyor." );
                                refresh();
                                return;
                        }
                        member = mMembers.value( id );
                }

                MemberDialog dialog( mFields, member, this );
                if ( dialog.exec() == QDialog::Accepted )
                {
                        // Database emitted changed() -> refresh(); move to the saved member
                        qint64 savedId = dialog.member().id;
                        for ( int r = 0; r < mProxy->rowCount(); r++ )
                        {
                                QModelIndex idx = mProxy->index( r, CHECK_COLUMN );
                                if ( idx.data( ID_ROLE ).toLongLong() == savedId )
                                {
                                        mTable->selectRow( r );
                                        mTable->scrollTo( idx );
                                        break;
                                }
                        }
                }
        }


        void MembersView::onDeleteClicked()
        {
                QList<qint64> ids = checkedIds();
                if ( ids.isEmpty() )
                {
                        return;
                }

                QString what;
                if ( ids.size() == 1 )
                {
                        const auto& m = mMembers.value( ids.first() );
                        QString name = m.values.value( "FIRMA" );
                        if ( name.isEmpty() )
                        {
                                name = ( m.values.value( "AD" ) + " " + m.values.value( "SOYAD" ) ).trimmed();
                        }
                        what = QString( "\"%1\" kaydı" ).arg( name );
                }
                else
                {
                        what = QString( "Seçili %1 üye" ).arg( ids.size() );
                }

                auto answer = QMessageBox::question( this, "Üyeyi sil",
                                                     QString( "%1 kalıcı olarak silinecek. Bu işlem geri alınamaz.\n\n"
                                                              "Üyeyi saklamak ama etiketlerden çıkarmak için \"Pasif Yap\" kullanabilirsiniz.\n\n"
                                                              "Silinsin mi?" ).arg( what ),
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
                if ( answer != QMessageBox::Yes )
                {
                        return;
                }

                QString error;
                if ( !merge::MemberDatabase::instance()->deleteMembers( ids, &error ) )
                {
                        showError( "Silinemedi", error );
                }
        }


        void MembersView::onToggleActiveClicked()
        {
                QList<qint64> ids = checkedIds();
                bool makeActive = ( mToggleActiveButton->text() == "Aktif Yap" );

                QString error;
                if ( !merge::MemberDatabase::instance()->setActive( ids, makeActive, &error ) )
                {
                        showError( "Durum değiştirilemedi", error );
                        return;
                }
                if ( !makeActive && !mShowPassiveCheck->isChecked() )
                {
                        mStatusLabel->setText( QString( "%1 üye pasif yapıldı (görmek için \"Pasif üyeleri göster\")." ).arg( ids.size() ) );
                }
        }


        void MembersView::onCustomFieldsClicked()
        {
                CustomFieldsDialog dialog( this );
                dialog.exec();
                // Database changes already triggered refresh()
        }


        ///
        /// Selected members hidden by search/filter: ask whether to include them.
        /// Returns false when cancelled. "Only visible" also clears the hidden checks,
        /// so the selection everywhere matches what is printed.
        ///
        bool MembersView::confirmHiddenSelection( const QString& what )
        {
                QSet<qint64> visible;
                for ( int r = 0; r < mProxy->rowCount(); r++ )
                {
                        visible.insert( mProxy->index( r, CHECK_COLUMN ).data( ID_ROLE ).toLongLong() );
                }
                QSet<qint64> visibleChecked = mChecked & visible;
                int hidden = int( mChecked.size() - visibleChecked.size() );
                if ( hidden == 0 )
                {
                        return true;
                }

                QMessageBox box( this );
                box.setIcon( QMessageBox::Question );
                box.setWindowTitle( what );
                box.setText( QString( "Seçili %1 üyenin %2 tanesi şu an arama/filtre nedeniyle listede görünmüyor." )
                             .arg( mChecked.size() ).arg( hidden ) );
                box.setInformativeText( "Görünmeyen seçili üyeler de dahil edilsin mi?" );
                QPushButton* all = box.addButton( QString( "Hepsini Dahil Et (%1)" ).arg( mChecked.size() ), QMessageBox::AcceptRole );
                QPushButton* onlyVisible = box.addButton( QString( "Sadece Görünenler (%1)" ).arg( visibleChecked.size() ),
                                                          QMessageBox::AcceptRole );
                QPushButton* cancel = box.addButton( "Vazgeç", QMessageBox::RejectRole );
                onlyVisible->setEnabled( !visibleChecked.isEmpty() );
                box.setDefaultButton( visibleChecked.isEmpty() ? all : onlyVisible );
                box.setEscapeButton( cancel );
                box.exec();

                if ( box.clickedButton() == all )
                {
                        return true;
                }
                if ( box.clickedButton() == onlyVisible )
                {
                        mChecked = visibleChecked;
                        mRefreshing = true;
                        for ( int r = 0; r < mItems->rowCount(); r++ )
                        {
                                auto* item = mItems->item( r, CHECK_COLUMN );
                                item->setCheckState( mChecked.contains( item->data( ID_ROLE ).toLongLong() ) ? Qt::Checked : Qt::Unchecked );
                        }
                        mRefreshing = false;
                        mTable->viewport()->update();
                        updateActions();
                        emit selectionEdited();
                        return true;
                }
                return false;
        }


        void MembersView::onPrintLabelsClicked()
        {
                if ( checkedIds().isEmpty() || !confirmHiddenSelection( "Etiket Bas" ) )
                {
                        return;
                }
                emit printLabelsRequested( checkedIds() );
        }


        void MembersView::onPrintListClicked()
        {
                if ( !checkedIds().isEmpty() && !confirmHiddenSelection( "Liste Yazdır" ) )
                {
                        return;
                }
                QList<merge::Member> members = checkedMembers();
                if ( members.isEmpty() )
                {
                        return;
                }
                ListReportDialog dialog( members, mFields, this );
                dialog.exec();
        }


        void MembersView::onBackupClicked()
        {
                QString suggested = QDir( merge::MemberBackup::folder() )
                        .filePath( QString( "uyeler-yedek-%1.sqlite" ).arg( QDate::currentDate().toString( Qt::ISODate ) ) );
                QDir().mkpath( merge::MemberBackup::folder() );

                QString fileName = QFileDialog::getSaveFileName( this, "Üye veritabanını yedekle", suggested,
                                                                 "Üye yedeği (*.sqlite)" );
                if ( fileName.isEmpty() )
                {
                        return;
                }
                if ( !fileName.endsWith( ".sqlite", Qt::CaseInsensitive ) )
                {
                        fileName += ".sqlite";
                }

                QString error;
                if ( !merge::MemberDatabase::instance()->backupTo( fileName, &error ) )
                {
                        showError( "Yedek alınamadı", error );
                        return;
                }
                merge::MemberBackup::recordBackup( QDateTime::currentDateTime() );
                MemberMaintenance::clearNotice();
                updateBackupStatus();
                QMessageBox::information( this, "Yedek alındı",
                                          QString( "Yedek kaydedildi:\n%1\n\n"
                                                   "Bilgisayar arızasına karşı yedeği ara sıra bir USB belleğe veya buluta da kopyalayın." )
                                          .arg( QDir::toNativeSeparators( fileName ) ) );
        }


        void MembersView::onRestoreClicked()
        {
                QString start = merge::MemberBackup::folder();
                if ( !QDir( start ).exists() )
                {
                        start = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
                }
                QString fileName = QFileDialog::getOpenFileName( this, "Geri yüklenecek yedeği seçin", start,
                                                                 "Üye yedeği (*.sqlite);;Tüm dosyalar (*)" );
                if ( fileName.isEmpty() )
                {
                        return;
                }

                merge::BackupInfo info;
                QString error;
                if ( !merge::MemberDatabase::inspectBackup( fileName, &info, &error ) )
                {
                        showError( "Bu dosya geri yüklenemez", error );
                        return;
                }

                auto db = merge::MemberDatabase::instance();
                QString countError;
                int currentCount = db->memberCount( &countError );
                QString current = currentCount >= 0 ? QString( "%1 üye" ).arg( currentCount )
                                                    : QString( "okunamadı" );
                QString when = QLocale( QLocale::Turkish ).toString( info.fileTime, "d MMMM yyyy HH:mm" );

                QMessageBox box( this );
                box.setIcon( QMessageBox::Warning );
                box.setWindowTitle( "Yedekten geri yükle" );
                box.setText( "Mevcut üye verileri seçilen yedekle <b>değiştirilecek</b>." );
                box.setInformativeText( QString( "Yedek: %1 — %2 üye (%3 aktif)\nŞu anki veritabanı: %4\n\n"
                                                 "Yedekten sonra yapılan değişiklikler kaybolur. "
                                                 "Güvenlik için mevcut veriler geri yüklemeden önce ayrıca saklanır." )
                                        .arg( when ).arg( info.totalMembers ).arg( info.activeMembers ).arg( current ) );
                QPushButton* restoreButton = box.addButton( "Geri Yükle", QMessageBox::DestructiveRole );
                QPushButton* cancelButton = box.addButton( "Vazgeç", QMessageBox::RejectRole );
                box.setDefaultButton( cancelButton );
                box.exec();
                if ( box.clickedButton() != restoreButton )
                {
                        return;
                }

                QString preserve = merge::MemberBackup::preRestorePath();
                if ( !db->restoreFrom( fileName, preserve, &error ) )
                {
                        showError( "Geri yükleme başarısız", error );
                        refresh();
                        return;
                }
                mChecked.clear();
                refresh();
                QMessageBox::information( this, "Geri yüklendi",
                                          QString( "Yedek geri yüklendi (%1 üye).\n\n"
                                                   "Geri yüklemeden önceki veriler şuraya kaydedildi:\n%2" )
                                          .arg( info.totalMembers ).arg( QDir::toNativeSeparators( preserve ) ) );
        }


        void MembersView::onChangeBackupFolderClicked()
        {
                QString dir = QFileDialog::getExistingDirectory( this, "Otomatik yedeklerin kaydedileceği klasör",
                                                                 merge::MemberBackup::folder() );
                if ( dir.isEmpty() )
                {
                        return;
                }

                // Make sure the folder is usable right away, not at the next automatic backup
                merge::MemberBackup::setFolder( dir );
                QString used, warning, error;
                if ( !merge::MemberBackup::runAutomatic( &used, &warning, &error ) )
                {
                        showError( "Yedek alınamadı", error );
                }
                else if ( !warning.isEmpty() )
                {
                        showError( "Klasöre yazılamadı", warning );
                }
                else
                {
                        MemberMaintenance::clearNotice();
                        QMessageBox::information( this, "Yedek klasörü değişti",
                                                  QString( "Otomatik yedekler artık şuraya alınacak:\n%1\n\nİlk yedek alındı." )
                                                  .arg( QDir::toNativeSeparators( dir ) ) );
                }
                updateBackupStatus();
        }


        void MembersView::onOpenBackupFolderClicked()
        {
                QString dir = merge::MemberBackup::folder();
                if ( !QDir( dir ).exists() && !QDir().mkpath( dir ) )
                {
                        showError( "Klasör açılamadı", QString( "Klasör bulunamadı: %1" ).arg( QDir::toNativeSeparators( dir ) ) );
                        return;
                }
                if ( !QDesktopServices::openUrl( QUrl::fromLocalFile( dir ) ) )
                {
                        showError( "Klasör açılamadı", QDir::toNativeSeparators( dir ) );
                }
        }


        void MembersView::updateBackupStatus()
        {
                QDateTime last = merge::MemberBackup::lastBackupTime();
                QString notice = MemberMaintenance::lastNotice();
                bool hasMembers = !mMembers.isEmpty();

                QString text;
                bool warn = false;
                if ( !last.isValid() )
                {
                        text = "Henüz yedek alınmadı";
                        warn = hasMembers;
                }
                else
                {
                        QDate d = last.date();
                        QDate today = QDate::currentDate();
                        QString day = ( d == today ) ? QString( "bugün" )
                                    : ( d == today.addDays( -1 ) ) ? QString( "dün" )
                                    : QLocale( QLocale::Turkish ).toString( d, "d MMMM" );
                        text = QString( "Son yedek: %1 %2" ).arg( day, last.time().toString( "HH:mm" ) );
                        warn = last.daysTo( QDateTime::currentDateTime() ) >= merge::MemberBackup::REMIND_AFTER_DAYS;
                }
                if ( !notice.isEmpty() )
                {
                        warn = true;
                }

                if ( warn )
                {
                        mBackupLabel->setText( QString( "<span style='color:#e65100;'>⚠ %1</span> · <a href='backup'>Şimdi yedekle</a>" )
                                               .arg( text.toHtmlEscaped() ) );
                }
                else
                {
                        mBackupLabel->setText( QString( "<span style='color:#6b6b6b;'>%1</span>" ).arg( text.toHtmlEscaped() ) );
                }
                mBackupLabel->setToolTip( notice.isEmpty()
                                          ? QString( "Otomatik yedek klasörü: %1" ).arg( QDir::toNativeSeparators( merge::MemberBackup::folder() ) )
                                          : notice );
        }


        void MembersView::showError( const QString& title, const QString& message )
        {
                QMessageBox::warning( this, title, message );
        }

}
