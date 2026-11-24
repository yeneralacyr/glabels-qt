//  MemberLabel.cpp
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

#include "MemberLabel.hpp"

#include "SelectProductDialog.hpp"

#include "merge/MemberDb.hpp"
#include "model/Model.hpp"
#include "model/ModelTextObject.hpp"
#include "model/XmlLabelCreator.hpp"

#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>


namespace
{
        const char* SETTINGS_KEY = "uyeler/etiketDosyasi";

        QString defaultFileName()
        {
                QString base = QStandardPaths::writableLocation( QStandardPaths::GenericDataLocation );
                return QDir( base ).filePath( QStringLiteral( "glabels-uyeler/uye-etiketi.glabels" ) );
        }
}


namespace glabels::MemberLabel
{

        QString fileName()
        {
                QSettings settings;
                // A remembered location is kept even when the file is missing: the label is then
                // created there again on first use (never silently somewhere else)
                QString stored = settings.value( SETTINGS_KEY ).toString();
                return stored.isEmpty() ? defaultFileName() : stored;
        }


        bool createDefault( const model::Template& tmplate, const QString& fileName, QString* error )
        {
                auto fail = [&]( const QString& msg ) {
                        if ( error )
                        {
                                *error = msg;
                        }
                        return false;
                };

                const model::Frame* frame = tmplate.frame();
                if ( tmplate.isNull() || !frame )
                {
                        return fail( QStringLiteral( "Geçersiz etiket kağıdı seçimi." ) );
                }

                model::Model label;
                label.setTmplate( tmplate );
                label.setRotate( frame->h() > frame->w() );   // same rule as File→New

                double w = label.w().pt();
                double h = label.h().pt();
                double margin = std::clamp( std::min( w, h ) * 0.08, 3.0, 9.0 );
                double boxW = w - 2 * margin;
                double boxH = h - 2 * margin;
                if ( boxW <= 0 || boxH <= 0 )
                {
                        return fail( QStringLiteral( "Seçilen etiket bir adres etiketi için çok küçük." ) );
                }

                // Up to 5 lines (address may take two); auto-shrink handles long values
                double fontSize = std::clamp( boxH / 5.0 / 1.25, 6.0, 11.0 );

                auto* text = new model::ModelTextObject( model::Distance::pt( margin ),
                                                         model::Distance::pt( margin ),
                                                         model::Distance::pt( boxW ),
                                                         model::Distance::pt( boxH ),
                                                         false,
                                                         QStringLiteral( "${FIRMA}\n${AD} ${SOYAD}\n${ADRES}\n${ILCE} ${IL}" ),
                                                         QStringLiteral( "Arial" ),
                                                         fontSize,
                                                         QFont::Normal,
                                                         false,
                                                         false,
                                                         model::ColorNode( QColor( Qt::black ) ),
                                                         Qt::AlignLeft,
                                                         Qt::AlignVCenter,
                                                         QTextOption::WordWrap,
                                                         1.0,
                                                         true );
                label.addObject( text );

                merge::Merge* memberMerge = merge::MemberDb::create();
                memberMerge->setSource( "" );
                label.setMerge( memberMerge );

                QDir dir = QFileInfo( fileName ).absoluteDir();
                if ( !dir.exists() && !dir.mkpath( QStringLiteral( "." ) ) )
                {
                        return fail( QStringLiteral( "Klasör oluşturulamadı: %1" ).arg( dir.absolutePath() ) );
                }

                model::XmlLabelCreator::writeFile( &label, fileName );
                if ( !QFileInfo::exists( fileName ) || QFileInfo( fileName ).size() == 0 )
                {
                        return fail( QStringLiteral( "Etiket tasarımı kaydedilemedi: %1" ).arg( fileName ) );
                }
                return true;
        }


        QString ensure( QWidget* parent, QString* error )
        {
                QString file = fileName();
                if ( QFileInfo::exists( file ) )
                {
                        return file;
                }

                QMessageBox::information( parent, QStringLiteral( "Etiket kağıdı seçin" ),
                                          QStringLiteral( "İlk etiket basımı için kullandığınız etiket kağıdını bir kez seçmeniz gerekiyor.\n\n"
                                                          "Kağıt kutusunun üzerindeki marka ve ürün kodunu (ör. Avery L7163) arayabilirsiniz. "
                                                          "Seçiminiz hatırlanacak; tasarımı daha sonra Düzenle sayfasından değiştirebilirsiniz." ) );

                SelectProductDialog dialog( parent );
                dialog.exec();
                model::Template tmplate = dialog.tmplate();
                if ( tmplate.isNull() )
                {
                        return QString();   // cancelled
                }

                if ( !createDefault( tmplate, file, error ) )
                {
                        return QString();
                }

                QSettings settings;
                settings.setValue( SETTINGS_KEY, file );
                return file;
        }

}
