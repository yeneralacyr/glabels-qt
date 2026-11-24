//  MemberDialog.cpp
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

#include "MemberDialog.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>


namespace glabels
{

        MemberDialog::MemberDialog( const QList<merge::MemberField>& fields,
                                    const merge::Member&             member,
                                    QWidget*                         parent )
                : QDialog( parent ), mFields( fields ), mMember( member )
        {
                setWindowTitle( member.id == 0 ? "Yeni Üye" : "Üyeyi Düzenle" );

                mErrorLabel = new QLabel();
                mErrorLabel->setWordWrap( true );
                mErrorLabel->setStyleSheet( "color: #c62828;" );
                mErrorLabel->setVisible( false );

                auto* form = new QFormLayout();
                form->setFieldGrowthPolicy( QFormLayout::ExpandingFieldsGrow );

                bool customHeaderAdded = false;
                for ( const auto& f : mFields )
                {
                        bool isCustom = merge::MemberDatabase::isValidCustomKey( f.key );
                        if ( isCustom && !customHeaderAdded )
                        {
                                auto* line = new QFrame();
                                line->setFrameShape( QFrame::HLine );
                                line->setFrameShadow( QFrame::Sunken );
                                form->addRow( line );
                                form->addRow( new QLabel( "<b>Ek alanlar</b>" ) );
                                customHeaderAdded = true;
                        }

                        QString value = member.values.value( f.key );
                        QWidget* editor = nullptr;
                        if ( f.multiLine )
                        {
                                auto* edit = new QPlainTextEdit( value );
                                edit->setTabChangesFocus( true );
                                int lineHeight = QFontMetrics( edit->font() ).lineSpacing();
                                edit->setFixedHeight( lineHeight * 3 + 12 );
                                editor = edit;
                        }
                        else
                        {
                                auto* edit = new QLineEdit( value );
                                edit->setMaxLength( 500 );
                                editor = edit;
                        }
                        editor->setToolTip( QString( "Etikette kullanmak için: ${%1}" ).arg( f.key ) );
                        // An old error message must not stay on screen after the user edits the form
                        if ( auto* line = qobject_cast<QLineEdit*>( editor ) )
                        {
                                connect( line, &QLineEdit::textEdited, this, [this] { mErrorLabel->setVisible( false ); } );
                        }
                        else if ( auto* text = qobject_cast<QPlainTextEdit*>( editor ) )
                        {
                                connect( text, &QPlainTextEdit::textChanged, this, [this] { mErrorLabel->setVisible( false ); } );
                        }
                        mEditors.insert( f.key, editor );
                        form->addRow( f.label + ":", editor );
                }

                mActiveCheck = new QCheckBox( "Aktif üye (etiketlere dahil edilir)" );
                mActiveCheck->setChecked( member.active );
                form->addRow( QString(), mActiveCheck );

                auto* formWidget = new QWidget();
                formWidget->setLayout( form );
                auto* scroll = new QScrollArea();
                scroll->setWidget( formWidget );
                scroll->setWidgetResizable( true );
                scroll->setFrameShape( QFrame::NoFrame );

                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Cancel );
                buttons->button( QDialogButtonBox::Save )->setText( "Kaydet" );
                buttons->button( QDialogButtonBox::Cancel )->setText( "Vazgeç" );
                connect( buttons, &QDialogButtonBox::accepted, this, &MemberDialog::accept );
                connect( buttons, &QDialogButtonBox::rejected, this, &MemberDialog::reject );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( scroll, 1 );
                layout->addWidget( mErrorLabel );
                layout->addWidget( buttons );

                resize( 520, 640 );

                if ( auto* first = mEditors.value( "FIRMA" ) )
                {
                        first->setFocus();
                }
        }


        const merge::Member& MemberDialog::member() const
        {
                return mMember;
        }


        QString MemberDialog::fieldText( const QString& key ) const
        {
                QWidget* w = mEditors.value( key );
                if ( auto* line = qobject_cast<QLineEdit*>( w ) )
                {
                        return line->text().trimmed();
                }
                if ( auto* text = qobject_cast<QPlainTextEdit*>( w ) )
                {
                        return text->toPlainText().trimmed();
                }
                return QString();
        }


        QString MemberDialog::validate( const merge::Member& m )
        {
                if ( m.values.value( "FIRMA" ).trimmed().isEmpty()
                     && m.values.value( "AD" ).trimmed().isEmpty()
                     && m.values.value( "SOYAD" ).trimmed().isEmpty() )
                {
                        return "Firma ünvanı veya yetkili adı/soyadı alanlarından en az biri doldurulmalıdır.";
                }

                QString email = m.values.value( "EPOSTA" ).trimmed();
                static const QRegularExpression emailRe( "^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$" );
                if ( !email.isEmpty() && !emailRe.match( email ).hasMatch() )
                {
                        return QString( "E-posta adresi geçersiz görünüyor: %1" ).arg( email );
                }

                return QString();
        }


        void MemberDialog::accept()
        {
                merge::Member candidate = mMember;
                for ( const auto& f : mFields )
                {
                        candidate.values[f.key] = fieldText( f.key );
                }
                candidate.active = mActiveCheck->isChecked();

                QString problem = validate( candidate );
                if ( problem.isEmpty() )
                {
                        QString error;
                        if ( merge::MemberDatabase::instance()->saveMember( candidate, &error ) )
                        {
                                mMember = candidate;
                                QDialog::accept();
                                return;
                        }
                        problem = QString( "Kaydedilemedi: %1" ).arg( error );
                }

                mErrorLabel->setText( problem );
                mErrorLabel->setVisible( true );
        }

}
