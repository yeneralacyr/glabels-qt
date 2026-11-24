//  MemberPrintDialog.cpp
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

#include "MemberPrintDialog.hpp"

#include "merge/Merge.hpp"
#include "model/Frame.hpp"
#include "model/Model.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>


namespace glabels
{

        MemberPrintDialog::MemberPrintDialog( model::Model* model, int nMembers, int nSkippedPassive, QWidget* parent )
                : QDialog( parent )
        {
                setWindowTitle( "Etiket Bas" );

                bool empty = !model || model->objectList().isEmpty();
                int perSheet = ( model && model->frame() ) ? model->frame()->nLabels() : 0;
                int sheets = perSheet > 0 ? ( nMembers + perSheet - 1 ) / perSheet : 0;

                auto* heading = new QLabel( QString( "<span style='font-size:15pt;'><b>%1 üye için etiket</b></span>" )
                                            .arg( nMembers ) );

                QString paper = model ? model->tmplate().name() : QString();
                auto* info = new QLabel( QString( "Etiket kağıdı: <b>%1</b><br>Sayfa başına %2 etiket · yaklaşık %3 sayfa" )
                                         .arg( paper.toHtmlEscaped() ).arg( perSheet ).arg( sheets ) );
                info->setWordWrap( true );

                QLabel* skippedLabel = nullptr;
                if ( nSkippedPassive > 0 )
                {
                        skippedLabel = new QLabel( QString( "<span style='color:#b35c00;'><b>Not:</b> seçtiğiniz %1 pasif üye "
                                                            "etikete dahil edilmedi. Basılması gerekiyorsa Üyeler sayfasında "
                                                            "önce <b>Aktif Yap</b> kullanın.</span>" ).arg( nSkippedPassive ) );
                        skippedLabel->setWordWrap( true );
                }

                auto* preview = new QLabel();
                preview->setAlignment( Qt::AlignCenter );
                preview->setMinimumSize( 380, 160 );
                preview->setStyleSheet( "QLabel { background-color: #ececec; border-radius: 6px; padding: 12px; }" );
                QPixmap pixmap = renderPreview( model, QSize( 360, 220 ) );
                if ( pixmap.isNull() )
                {
                        preview->setText( "Önizleme oluşturulamadı." );
                }
                else
                {
                        preview->setPixmap( pixmap );
                }

                auto* caption = new QLabel();
                caption->setWordWrap( true );
                if ( empty )
                {
                        caption->setText( "<b style='color:#b35c00;'>Etiket tasarımı boş.</b> Basmadan önce tasarımda "
                                          "soldaki <b>Üye Alanları</b> listesinden alan ekleyin." );
                }
                else
                {
                        caption->setText( "İlk üyenin etiketi yukarıda görünüyor. Tasarımı değiştirmek için "
                                          "<b>Tasarımı Düzenle</b>, olduğu gibi basmak için <b>Yazdırmaya Geç</b>." );
                }

                auto* buttons = new QDialogButtonBox();
                auto* cancel = buttons->addButton( "Vazgeç", QDialogButtonBox::RejectRole );
                auto* design = buttons->addButton( "Tasarımı Düzenle", QDialogButtonBox::ActionRole );
                auto* print = buttons->addButton( "Yazdırmaya Geç", QDialogButtonBox::AcceptRole );
                print->setEnabled( !empty );
                ( empty ? design : print )->setDefault( true );

                connect( cancel, &QPushButton::clicked, this, &QDialog::reject );
                connect( design, &QPushButton::clicked, this, [this] { mChoice = Choice::Design; accept(); } );
                connect( print, &QPushButton::clicked, this, [this] { mChoice = Choice::Print; accept(); } );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( heading );
                layout->addWidget( info );
                if ( skippedLabel )
                {
                        layout->addWidget( skippedLabel );
                }
                layout->addSpacing( 6 );
                layout->addWidget( preview, 1 );
                layout->addWidget( caption );
                layout->addSpacing( 6 );
                layout->addWidget( buttons );
        }


        MemberPrintDialog::Choice MemberPrintDialog::choice() const
        {
                return mChoice;
        }


        QPixmap MemberPrintDialog::renderPreview( model::Model* model, const QSize& maxSize )
        {
                if ( !model || !model->frame() )
                {
                        return QPixmap();
                }
                double w = model->w().pt();
                double h = model->h().pt();
                if ( w <= 0 || h <= 0 )
                {
                        return QPixmap();
                }

                // First selected record, otherwise first record, otherwise none
                merge::Record record;
                const auto& records = model->merge()->recordList();
                auto selected = std::find_if( records.begin(), records.end(),
                                              []( const merge::Record& r ) { return r.isSelected(); } );
                if ( selected != records.end() )
                {
                        record = *selected;
                }
                else if ( !records.isEmpty() )
                {
                        record = records.first();
                }

                double scale = std::min( maxSize.width() / w, maxSize.height() / h );
                int pw = std::max( 1, int( std::lround( w * scale ) ) );
                int ph = std::max( 1, int( std::lround( h * scale ) ) );

                qreal dpr = 2.0;   // crisp on high-DPI screens
                QPixmap pixmap( int( pw * dpr ), int( ph * dpr ) );
                pixmap.setDevicePixelRatio( dpr );
                pixmap.fill( Qt::transparent );

                QPainter painter( &pixmap );
                painter.setRenderHint( QPainter::Antialiasing );
                painter.setRenderHint( QPainter::TextAntialiasing );
                painter.setPen( QPen( QColor( 0xb0, 0xb0, 0xb0 ), 1 ) );
                painter.setBrush( Qt::white );
                painter.drawRoundedRect( QRectF( 0.5, 0.5, pw - 1, ph - 1 ), 6, 6 );
                painter.setClipRect( QRectF( 0, 0, pw, ph ) );
                painter.scale( scale, scale );
                model->draw( &painter, false, record, model->variables() );
                painter.end();

                return pixmap;
        }

}
