//  CustomFieldsDialog.cpp
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

#include "CustomFieldsDialog.hpp"

#include "merge/MemberDatabase.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>


namespace
{
        const int KEY_ROLE = Qt::UserRole + 1;
}


namespace glabels
{

        CustomFieldsDialog::CustomFieldsDialog( QWidget* parent )
                : QDialog( parent )
        {
                setWindowTitle( "Ek Alanlar" );

                auto* intro = new QLabel( "Hazır alanlara ek olarak üyeler için kendi alanlarınızı tanımlayabilirsiniz. "
                                          "Her alan etiket tasarımında <b>${ANAHTAR}</b> şeklinde kullanılır." );
                intro->setWordWrap( true );

                mList = new QListWidget();

                mRemoveButton = new QPushButton( "Seçili Alanı Sil" );

                mLabelEdit = new QLineEdit();
                mLabelEdit->setPlaceholderText( "ör. Oda Kayıt Tarihi" );
                mKeyEdit = new QLineEdit();
                mKeyEdit->setPlaceholderText( "ör. ODA_KAYIT_TARIHI" );
                mKeyEdit->setValidator( new QRegularExpressionValidator(
                                                QRegularExpression( "[A-Z][A-Z0-9_]{0,39}" ), mKeyEdit ) );
                mAddButton = new QPushButton( "Ekle" );

                auto* form = new QFormLayout();
                form->setFieldGrowthPolicy( QFormLayout::ExpandingFieldsGrow );
                form->addRow( "Alan adı:", mLabelEdit );
                form->addRow( "Şablon anahtarı:", mKeyEdit );

                auto* addRow = new QHBoxLayout();
                addRow->addLayout( form, 1 );
                addRow->addWidget( mAddButton, 0, Qt::AlignBottom );

                mMessageLabel = new QLabel();
                mMessageLabel->setWordWrap( true );
                mMessageLabel->setVisible( false );

                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Close );
                buttons->button( QDialogButtonBox::Close )->setText( "Kapat" );
                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( intro );
                layout->addWidget( mList, 1 );
                layout->addWidget( mRemoveButton, 0, Qt::AlignLeft );
                layout->addSpacing( 8 );
                layout->addWidget( new QLabel( "<b>Yeni alan</b>" ) );
                layout->addLayout( addRow );
                layout->addWidget( mMessageLabel );
                layout->addWidget( buttons );

                connect( mLabelEdit, &QLineEdit::textEdited, this, &CustomFieldsDialog::onLabelEdited );
                connect( mKeyEdit, &QLineEdit::textEdited, this, &CustomFieldsDialog::onKeyEdited );
                connect( mLabelEdit, &QLineEdit::returnPressed, this, &CustomFieldsDialog::onAddClicked );
                connect( mKeyEdit, &QLineEdit::returnPressed, this, &CustomFieldsDialog::onAddClicked );
                connect( mAddButton, &QPushButton::clicked, this, &CustomFieldsDialog::onAddClicked );
                connect( mRemoveButton, &QPushButton::clicked, this, &CustomFieldsDialog::onRemoveClicked );
                connect( mList, &QListWidget::itemSelectionChanged, this, &CustomFieldsDialog::updateButtons );

                resize( 480, 420 );
                reload();
        }


        void CustomFieldsDialog::reload()
        {
                mList->clear();

                QList<merge::MemberField> fields;
                QString error;
                if ( !merge::MemberDatabase::instance()->customFields( fields, &error ) )
                {
                        showMessage( error, true );
                }
                else if ( fields.isEmpty() )
                {
                        auto* item = new QListWidgetItem( "Henüz ek alan yok." );
                        item->setFlags( Qt::NoItemFlags );
                        mList->addItem( item );
                }
                for ( const auto& f : fields )
                {
                        auto* item = new QListWidgetItem( QString( "%1    ${%2}" ).arg( f.label, f.key ) );
                        item->setData( KEY_ROLE, f.key );
                        mList->addItem( item );
                }
                updateButtons();
        }


        void CustomFieldsDialog::onLabelEdited( const QString& text )
        {
                if ( !mKeyTouched )
                {
                        mKeyEdit->setText( merge::MemberDatabase::suggestKey( text ) );
                }
                updateButtons();
        }


        void CustomFieldsDialog::onKeyEdited()
        {
                mKeyTouched = !mKeyEdit->text().isEmpty();
                updateButtons();
        }


        void CustomFieldsDialog::updateButtons()
        {
                auto* item = mList->currentItem();
                mRemoveButton->setEnabled( item && item->isSelected() && !item->data( KEY_ROLE ).toString().isEmpty() );
                mAddButton->setEnabled( !mLabelEdit->text().trimmed().isEmpty() && !mKeyEdit->text().isEmpty() );
        }


        void CustomFieldsDialog::onAddClicked()
        {
                if ( !mAddButton->isEnabled() )
                {
                        return;
                }

                merge::MemberField field{ mKeyEdit->text(), mLabelEdit->text().trimmed(), false };
                QString error;
                if ( !merge::MemberDatabase::instance()->addCustomField( field, &error ) )
                {
                        showMessage( error, true );
                        return;
                }

                showMessage( QString( "\"%1\" eklendi. Etikette ${%2} olarak kullanabilirsiniz." ).arg( field.label, field.key ), false );
                mLabelEdit->clear();
                mKeyEdit->clear();
                mKeyTouched = false;
                reload();
                mLabelEdit->setFocus();
        }


        void CustomFieldsDialog::onRemoveClicked()
        {
                auto* item = mList->currentItem();
                if ( !item )
                {
                        return;
                }
                QString key = item->data( KEY_ROLE ).toString();
                if ( key.isEmpty() )
                {
                        return;
                }

                auto answer = QMessageBox::question( this, "Ek alanı sil",
                                                     QString( "${%1} alanı ve tüm üyelerdeki bu alana ait değerler kalıcı olarak silinecek.\n\n"
                                                              "Bu alanı kullanan etiket tasarımlarında alan boş görünecek.\n\nSilinsin mi?" ).arg( key ),
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
                if ( answer != QMessageBox::Yes )
                {
                        return;
                }

                QString error;
                if ( !merge::MemberDatabase::instance()->removeCustomField( key, &error ) )
                {
                        showMessage( error, true );
                        return;
                }
                showMessage( QString( "${%1} silindi." ).arg( key ), false );
                reload();
        }


        void CustomFieldsDialog::showMessage( const QString& text, bool isError )
        {
                mMessageLabel->setStyleSheet( isError ? "color: #c62828;" : "color: #2e7d32;" );
                mMessageLabel->setText( text );
                mMessageLabel->setVisible( true );
        }

}
