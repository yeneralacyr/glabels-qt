//  MemberFieldsPanel.cpp
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

#include "MemberFieldsPanel.hpp"

#include "merge/MemberDatabase.hpp"
#include "merge/MemberDb.hpp"
#include "model/Model.hpp"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>


namespace
{
        const int KEY_ROLE = Qt::UserRole + 1;
}


namespace glabels
{

        MemberFieldsPanel::MemberFieldsPanel( QWidget* parent )
                : QWidget( parent )
        {
                auto* title = new QLabel( "<b>Üye Alanları</b>" );
                auto* hint = new QLabel( "Bir alana tıklayın: seçili metin kutusuna eklenir; "
                                         "seçili nesne yoksa yeni metin kutusu oluşturulur." );
                hint->setWordWrap( true );
                hint->setForegroundRole( QPalette::PlaceholderText );

                mList = new QListWidget();
                mList->setToolTip( "Etikete eklemek için tıklayın" );

                mErrorLabel = new QLabel();
                mErrorLabel->setWordWrap( true );
                mErrorLabel->setStyleSheet( "color: #c62828;" );
                mErrorLabel->setVisible( false );

                auto* previewLabel = new QLabel( "<b>Önizleme üyesi</b>" );
                mPreviewCombo = new QComboBox();
                mPreviewCombo->setToolTip( "Tasarım ekranında kodlar yerine bu üyenin bilgilerini gösterir. "
                                           "Neyin basılacağını değiştirmez." );
                auto* previewHint = new QLabel( "Sadece ekrandaki görünümü değiştirir." );
                previewHint->setWordWrap( true );
                previewHint->setForegroundRole( QPalette::PlaceholderText );

                auto* fieldsPage = new QWidget();
                auto* fieldsLayout = new QVBoxLayout( fieldsPage );
                fieldsLayout->setContentsMargins( 0, 0, 0, 0 );
                fieldsLayout->addWidget( hint );
                fieldsLayout->addWidget( mList, 1 );
                fieldsLayout->addWidget( mErrorLabel );
                fieldsLayout->addSpacing( 8 );
                fieldsLayout->addWidget( previewLabel );
                fieldsLayout->addWidget( mPreviewCombo );
                fieldsLayout->addWidget( previewHint );

                // Shown when the label uses another data source (e.g. a CSV file)
                mOtherSourceLabel = new QLabel();
                mOtherSourceLabel->setWordWrap( true );
                mConnectButton = new QPushButton( "Üye veritabanına bağla" );
                auto* otherPage = new QWidget();
                auto* otherLayout = new QVBoxLayout( otherPage );
                otherLayout->setContentsMargins( 0, 0, 0, 0 );
                otherLayout->addWidget( mOtherSourceLabel );
                otherLayout->addWidget( mConnectButton );
                otherLayout->addStretch( 1 );

                mStack = new QStackedWidget();
                mStack->addWidget( fieldsPage );   // 0
                mStack->addWidget( otherPage );    // 1

                // How many members will be printed, and the way to the Print page
                mSelectionLabel = new QLabel();
                mSelectionLabel->setWordWrap( true );
                mPrintButton = new QPushButton( "Yazdırmaya Geç  →" );
                mPrintButton->setToolTip( "Tasarım bittiğinde Yazdır sayfasını açar" );

                auto* layout = new QVBoxLayout( this );
                layout->setContentsMargins( 8, 8, 8, 8 );
                layout->addWidget( title );
                layout->addWidget( mStack, 1 );
                layout->addSpacing( 8 );
                layout->addWidget( mSelectionLabel );
                layout->addWidget( mPrintButton );

                setFixedWidth( 210 );

                connect( mList, &QListWidget::itemClicked, this, &MemberFieldsPanel::onItemActivated );
                connect( mPreviewCombo, &QComboBox::currentIndexChanged, this, &MemberFieldsPanel::onPreviewIndexChanged );
                connect( mConnectButton, &QPushButton::clicked, this, &MemberFieldsPanel::onConnectClicked );
                connect( mPrintButton, &QPushButton::clicked, this, &MemberFieldsPanel::printRequested );
                connect( merge::MemberDatabase::instance(), &merge::MemberDatabase::changed,
                         this, &MemberFieldsPanel::reload );

                reload();
        }


        void MemberFieldsPanel::setModel( model::Model* model )
        {
                mModel = model;
                connect( mModel, &model::Model::mergeChanged, this, &MemberFieldsPanel::onModelMergeChanged );
                onModelMergeChanged();
        }


        void MemberFieldsPanel::setPreviewMember( qint64 memberId )
        {
                int idx = mPreviewCombo->findData( memberId );
                mPreviewCombo->setCurrentIndex( idx < 0 ? 0 : idx );
        }


        void MemberFieldsPanel::updateSelectionInfo()
        {
                bool isMembers = mModel && mModel->merge()->id() == merge::MemberDb::id();
                int n = isMembers ? mModel->merge()->nSelectedRecords() : 0;
                mSelectionLabel->setVisible( isMembers );
                mSelectionLabel->setText( n > 0 ? QString( "<b>%1 üye</b> basılacak." ).arg( n )
                                                : QString( "Basılacak üye seçilmedi." ) );
                mPrintButton->setEnabled( mModel != nullptr );
        }


        bool MemberFieldsPanel::hasPreview() const
        {
                return mHasPreview;
        }


        merge::Record MemberFieldsPanel::previewRecord() const
        {
                return mPreview;
        }


        void MemberFieldsPanel::onModelMergeChanged()
        {
                bool isMembers = mModel && mModel->merge()->id() == merge::MemberDb::id();
                if ( mModel )
                {
                        connect( mModel->merge(), &merge::Merge::selectionChanged,
                                 this, &MemberFieldsPanel::updateSelectionInfo, Qt::UniqueConnection );
                        connect( mModel->merge(), &merge::Merge::sourceChanged,
                                 this, &MemberFieldsPanel::updateSelectionInfo, Qt::UniqueConnection );
                }
                updateSelectionInfo();
                if ( isMembers )
                {
                        mStack->setCurrentIndex( 0 );
                }
                else
                {
                        bool none = !mModel || mModel->merge()->id() == QStringLiteral( "None" );
                        mOtherSourceLabel->setText( none
                                ? QString( "Bu etiket şu an bir veri kaynağına bağlı değil." )
                                : QString( "Bu etiket başka bir veri kaynağı (ör. CSV dosyası) kullanıyor. "
                                           "Alanlarını Düzenle panelindeki \"Alan ekle\" düğmesinden ekleyebilirsiniz." ) );
                        mStack->setCurrentIndex( 1 );
                        if ( mHasPreview )
                        {
                                mPreviewCombo->setCurrentIndex( 0 );
                        }
                }
        }


        void MemberFieldsPanel::onConnectClicked()
        {
                if ( !mModel )
                {
                        return;
                }
                merge::Merge* memberMerge = merge::MemberDb::create();
                memberMerge->setSource( "" );
                mModel->setMerge( memberMerge );
        }


        void MemberFieldsPanel::reload()
        {
                auto db = merge::MemberDatabase::instance();
                QString error;

                // Fields
                QList<merge::MemberField> fields;
                mList->clear();
                if ( !db->allFields( fields, &error ) )
                {
                        fields = merge::MemberDatabase::standardFields();
                        mErrorLabel->setText( QString( "Üye veritabanı okunamadı: %1" ).arg( error ) );
                        mErrorLabel->setVisible( true );
                }
                else
                {
                        mErrorLabel->setVisible( false );
                }
                for ( const auto& f : fields )
                {
                        auto* item = new QListWidgetItem( f.label );
                        item->setData( KEY_ROLE, f.key );
                        item->setToolTip( QString( "Etikete ${%1} olarak eklenir" ).arg( f.key ) );
                        mList->addItem( item );
                }

                // Preview members (keep the current choice if it still exists)
                qint64 previous = mPreviewCombo->currentData().toLongLong();
                QList<merge::Member> members;
                QSignalBlocker block( mPreviewCombo );
                mPreviewCombo->clear();
                mPreviewCombo->addItem( "Yok — alan kodlarını göster", 0 );
                if ( db->members( members, true, &error ) )
                {
                        for ( const auto& m : members )
                        {
                                QString name = m.values.value( "FIRMA" ).trimmed();
                                QString person = ( m.values.value( "AD" ) + " " + m.values.value( "SOYAD" ) ).trimmed();
                                QString text = name.isEmpty() ? person : ( person.isEmpty() ? name : name + " — " + person );
                                mPreviewCombo->addItem( text.isEmpty() ? QString( "(isimsiz üye %1)" ).arg( m.id ) : text, m.id );
                        }
                }
                mPreviewCombo->setEnabled( !members.isEmpty() );
                int idx = previous ? mPreviewCombo->findData( previous ) : 0;
                mPreviewCombo->setCurrentIndex( idx < 0 ? 0 : idx );

                // Re-read the preview member: its data may have changed (or it was deleted)
                onPreviewIndexChanged();
        }


        void MemberFieldsPanel::onItemActivated( QListWidgetItem* item )
        {
                if ( item )
                {
                        emit fieldActivated( item->data( KEY_ROLE ).toString() );
                }
        }


        void MemberFieldsPanel::onPreviewIndexChanged()
        {
                qint64 id = mPreviewCombo->currentData().toLongLong();
                bool had = mHasPreview;
                mHasPreview = false;
                mPreview = merge::Record();

                if ( id != 0 )
                {
                        QList<merge::Member> members;
                        QString error;
                        if ( merge::MemberDatabase::instance()->members( members, true, &error ) )
                        {
                                for ( const auto& m : members )
                                {
                                        if ( m.id == id )
                                        {
                                                for ( auto it = m.values.constBegin(); it != m.values.constEnd(); ++it )
                                                {
                                                        mPreview[it.key()] = it.value();
                                                }
                                                mPreview[merge::MemberDatabase::idKey()] = QString::number( m.id );
                                                mHasPreview = true;
                                                break;
                                        }
                                }
                        }
                }

                if ( had || mHasPreview )
                {
                        emit previewChanged();
                }
        }

}
