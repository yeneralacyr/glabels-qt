//  ListReportDialog.cpp
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

#include "ListReportDialog.hpp"

#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>


namespace
{
        const int KEY_ROLE = Qt::UserRole + 1;
        const QString NUMBER_KEY = QStringLiteral( "__SIRA__" );
        const char* SETTINGS_COLUMNS = "uyeler/listeSutunlari";

        const QStringList DEFAULT_COLUMNS = { NUMBER_KEY, "FIRMA", "AD", "SOYAD", "TELEFON" };


        QString defaultPath( const QString& baseName, const QString& ext )
        {
                QString dir = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
                return QDir( dir ).filePath( QString( "%1-%2.%3" )
                                             .arg( baseName, QDate::currentDate().toString( Qt::ISODate ), ext ) );
        }


        QString csvCell( const QString& value )
        {
                QString v = value;
                v.replace( '"', QStringLiteral( "\"\"" ) );
                return QStringLiteral( "\"%1\"" ).arg( v );
        }
}


namespace glabels
{

        ListReportDialog::ListReportDialog( const QList<merge::Member>&      members,
                                            const QList<merge::MemberField>& fields,
                                            QWidget*                         parent )
                : QDialog( parent ), mMembers( members ), mFields( fields )
        {
                setWindowTitle( "Liste Yazdır" );

                mInfoLabel = new QLabel( QString( "<b>%1 üye</b> listelenecek (Üyeler sayfasında işaretli olanlar)." )
                                         .arg( members.size() ) );

                mTitleEdit = new QLineEdit( "Üye Listesi" );

                // Column chooser: check to include, drag to reorder
                mColumnList = new QListWidget();
                mColumnList->setDragDropMode( QAbstractItemView::InternalMove );
                mColumnList->setDefaultDropAction( Qt::MoveAction );

                QSettings settings;
                QStringList saved = settings.value( SETTINGS_COLUMNS ).toStringList();
                QStringList wanted = saved.isEmpty() ? DEFAULT_COLUMNS : saved;

                QList<QPair<QString,QString>> all;   // key, label
                all << qMakePair( NUMBER_KEY, QStringLiteral( "Sıra No" ) );
                for ( const auto& f : mFields )
                {
                        all << qMakePair( f.key, f.label );
                }
                // Checked columns first, in remembered order; then the rest
                QList<QPair<QString,QString>> ordered;
                for ( const auto& key : wanted )
                {
                        for ( const auto& p : all )
                        {
                                if ( p.first == key )
                                {
                                        ordered << p;
                                }
                        }
                }
                for ( const auto& p : all )
                {
                        if ( !wanted.contains( p.first ) )
                        {
                                ordered << p;
                        }
                }
                for ( const auto& p : ordered )
                {
                        auto* item = new QListWidgetItem( p.second );
                        item->setData( KEY_ROLE, p.first );
                        item->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled );
                        item->setCheckState( wanted.contains( p.first ) ? Qt::Checked : Qt::Unchecked );
                        mColumnList->addItem( item );
                }

                mOrientationCombo = new QComboBox();
                mOrientationCombo->addItem( "Dikey (A4)" );
                mOrientationCombo->addItem( "Yatay (A4)" );

                auto* form = new QFormLayout();
                form->setFieldGrowthPolicy( QFormLayout::ExpandingFieldsGrow );
                form->addRow( "Başlık:", mTitleEdit );
                form->addRow( "Sayfa:", mOrientationCombo );

                auto* columnsHint = new QLabel( "Listede görünecek sütunları işaretleyin. Sırasını değiştirmek için sürükleyin." );
                columnsHint->setWordWrap( true );

                mPreviewButton = new QPushButton( "Önizle ve Yazdır…" );
                mPdfButton = new QPushButton( "PDF Olarak Kaydet…" );
                mExcelButton = new QPushButton( "Excel'e Aktar…" );
                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Close );
                buttons->button( QDialogButtonBox::Close )->setText( "Kapat" );
                buttons->addButton( mExcelButton, QDialogButtonBox::ActionRole );
                buttons->addButton( mPdfButton, QDialogButtonBox::ActionRole );
                buttons->addButton( mPreviewButton, QDialogButtonBox::ActionRole );
                // QDialogButtonBox makes "Close" the default; Enter should preview instead
                buttons->button( QDialogButtonBox::Close )->setAutoDefault( false );
                buttons->button( QDialogButtonBox::Close )->setDefault( false );
                mPreviewButton->setDefault( true );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( mInfoLabel );
                layout->addLayout( form );
                layout->addWidget( new QLabel( "<b>Sütunlar</b>" ) );
                layout->addWidget( columnsHint );
                layout->addWidget( mColumnList, 1 );
                layout->addWidget( buttons );

                connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
                connect( mPreviewButton, &QPushButton::clicked, this, &ListReportDialog::onPreviewClicked );
                connect( mPdfButton, &QPushButton::clicked, this, &ListReportDialog::onPdfClicked );
                connect( mExcelButton, &QPushButton::clicked, this, &ListReportDialog::onExcelClicked );
                connect( mColumnList, &QListWidget::itemChanged, this, &ListReportDialog::updateButtons );

                resize( 460, 560 );
                updateButtons();
        }


        QList<merge::MemberField> ListReportDialog::chosenColumns( bool* numbered ) const
        {
                QList<merge::MemberField> columns;
                QStringList keys;
                *numbered = false;
                for ( int i = 0; i < mColumnList->count(); i++ )
                {
                        auto* item = mColumnList->item( i );
                        if ( item->checkState() != Qt::Checked )
                        {
                                continue;
                        }
                        QString key = item->data( KEY_ROLE ).toString();
                        keys << key;
                        if ( key == NUMBER_KEY )
                        {
                                // Only meaningful as the first column
                                *numbered = true;
                                continue;
                        }
                        for ( const auto& f : mFields )
                        {
                                if ( f.key == key )
                                {
                                        columns << f;
                                }
                        }
                }

                QSettings settings;
                settings.setValue( SETTINGS_COLUMNS, keys );
                return columns;
        }


        void ListReportDialog::updateButtons()
        {
                bool any = false;
                for ( int i = 0; i < mColumnList->count(); i++ )
                {
                        auto* item = mColumnList->item( i );
                        if ( item->checkState() == Qt::Checked && item->data( KEY_ROLE ).toString() != NUMBER_KEY )
                        {
                                any = true;
                                break;
                        }
                }
                bool ok = any && !mMembers.isEmpty();
                mPreviewButton->setEnabled( ok );
                mPdfButton->setEnabled( ok );
                mExcelButton->setEnabled( ok );
                QString tip = any ? QString() : QString( "En az bir sütun işaretleyin." );
                mPreviewButton->setToolTip( tip );
                mPdfButton->setToolTip( tip );
                mExcelButton->setToolTip( tip );
        }


        QString ListReportDialog::buildHtml( const QString&                   title,
                                             const QList<merge::Member>&      members,
                                             const QList<merge::MemberField>& columns,
                                             bool                             numbered )
        {
                QString html;
                html += "<html><head><style>"
                        "body { font-family: Arial, Helvetica, sans-serif; font-size: 9pt; }"
                        "h1 { font-size: 14pt; margin: 0; }"
                        "p.meta { color: #555555; margin: 2px 0 8px 0; }"
                        "th { background-color: #e8e8e8; text-align: left; padding: 3px 4px; }"
                        "td { padding: 3px 4px; vertical-align: top; }"
                        "</style></head><body>";
                html += QString( "<h1>%1</h1>" ).arg( title.toHtmlEscaped() );
                html += QString( "<p class='meta'>%1 · %2 kayıt</p>" )
                        .arg( QLocale( QLocale::Turkish ).toString( QDate::currentDate(), QLocale::LongFormat ) )
                        .arg( members.size() );

                html += "<table width='100%' cellspacing='0' cellpadding='3' border='1' style='border-collapse: collapse; border-color: #999999;'>";
                html += "<thead><tr>";
                if ( numbered )
                {
                        html += "<th width='1%'>#</th>";
                }
                for ( const auto& c : columns )
                {
                        html += QString( "<th>%1</th>" ).arg( c.label.toHtmlEscaped() );
                }
                html += "</tr></thead><tbody>";

                int n = 1;
                for ( const auto& m : members )
                {
                        html += "<tr>";
                        if ( numbered )
                        {
                                html += QString( "<td align='right'>%1</td>" ).arg( n );
                        }
                        for ( const auto& c : columns )
                        {
                                QString v = m.values.value( c.key ).toHtmlEscaped();
                                v.replace( '\n', "<br>" );
                                html += QString( "<td>%1</td>" ).arg( v );
                        }
                        html += "</tr>";
                        n++;
                }
                html += "</tbody></table></body></html>";
                return html;
        }


        QByteArray ListReportDialog::buildCsv( const QList<merge::Member>&      members,
                                               const QList<merge::MemberField>& columns,
                                               bool                             numbered )
        {
                // UTF-8 BOM + ';' so Turkish Excel opens it with correct characters and columns
                QStringList lines;
                QStringList header;
                if ( numbered )
                {
                        header << csvCell( "Sıra No" );
                }
                for ( const auto& c : columns )
                {
                        header << csvCell( c.label );
                }
                lines << header.join( ';' );

                int n = 1;
                for ( const auto& m : members )
                {
                        QStringList row;
                        if ( numbered )
                        {
                                row << QString::number( n );
                        }
                        for ( const auto& c : columns )
                        {
                                row << csvCell( m.values.value( c.key ) );
                        }
                        lines << row.join( ';' );
                        n++;
                }

                QByteArray data( "\xEF\xBB\xBF" );
                data += lines.join( "\r\n" ).toUtf8();
                data += "\r\n";
                return data;
        }


        void ListReportDialog::renderTo( QPrinter* printer ) const
        {
                bool numbered = false;
                QList<merge::MemberField> columns = chosenColumns( &numbered );

                printer->setPageSize( QPageSize( QPageSize::A4 ) );
                printer->setPageOrientation( mOrientationCombo->currentIndex() == 1 ? QPageLayout::Landscape
                                                                                     : QPageLayout::Portrait );
                printer->setPageMargins( QMarginsF( 12, 12, 12, 12 ), QPageLayout::Millimeter );

                QTextDocument doc;
                doc.setHtml( buildHtml( mTitleEdit->text().trimmed(), mMembers, columns, numbered ) );
                // No page size on the document: QTextDocument paginates to the printer and adds page numbers
                doc.print( printer );
        }


        void ListReportDialog::onPreviewClicked()
        {
                QPrinter printer( QPrinter::HighResolution );
                QPrintPreviewDialog preview( &printer, this );
                preview.setWindowTitle( "Liste Önizleme" );
                connect( &preview, &QPrintPreviewDialog::paintRequested, this, &ListReportDialog::renderTo );
                preview.resize( 900, 700 );
                preview.exec();
        }


        void ListReportDialog::onPdfClicked()
        {
                QString fileName = QFileDialog::getSaveFileName( this, "PDF olarak kaydet",
                                                                 defaultPath( "uye-listesi", "pdf" ),
                                                                 "PDF dosyası (*.pdf)" );
                if ( fileName.isEmpty() )
                {
                        return;
                }
                if ( !fileName.endsWith( ".pdf", Qt::CaseInsensitive ) )
                {
                        fileName += ".pdf";
                }

                QPrinter printer( QPrinter::HighResolution );
                printer.setOutputFormat( QPrinter::PdfFormat );
                printer.setOutputFileName( fileName );
                renderTo( &printer );

                if ( !QFileInfo::exists( fileName ) || QFileInfo( fileName ).size() == 0 )
                {
                        showError( QString( "PDF oluşturulamadı:\n%1" ).arg( QDir::toNativeSeparators( fileName ) ) );
                        return;
                }

                auto answer = QMessageBox::question( this, "PDF kaydedildi",
                                                     QString( "Liste kaydedildi:\n%1\n\nŞimdi açılsın mı?" )
                                                     .arg( QDir::toNativeSeparators( fileName ) ) );
                if ( answer == QMessageBox::Yes && !QDesktopServices::openUrl( QUrl::fromLocalFile( fileName ) ) )
                {
                        showError( "PDF açılamadı. Dosyayı klasöründen açabilirsiniz." );
                }
        }


        void ListReportDialog::onExcelClicked()
        {
                QString fileName = QFileDialog::getSaveFileName( this, "Excel'e aktar",
                                                                 defaultPath( "uye-listesi", "csv" ),
                                                                 "Excel ile açılabilen CSV (*.csv)" );
                if ( fileName.isEmpty() )
                {
                        return;
                }
                if ( !fileName.endsWith( ".csv", Qt::CaseInsensitive ) )
                {
                        fileName += ".csv";
                }

                bool numbered = false;
                QList<merge::MemberField> columns = chosenColumns( &numbered );

                QSaveFile file( fileName );
                if ( !file.open( QIODevice::WriteOnly ) )
                {
                        showError( QString( "Dosya yazılamadı: %1" ).arg( file.errorString() ) );
                        return;
                }
                file.write( buildCsv( mMembers, columns, numbered ) );
                if ( !file.commit() )
                {
                        showError( QString( "Dosya kaydedilemedi: %1" ).arg( file.errorString() ) );
                        return;
                }

                auto answer = QMessageBox::question( this, "Excel dosyası kaydedildi",
                                                     QString( "Liste kaydedildi:\n%1\n\nŞimdi açılsın mı?" )
                                                     .arg( QDir::toNativeSeparators( fileName ) ) );
                if ( answer == QMessageBox::Yes && !QDesktopServices::openUrl( QUrl::fromLocalFile( fileName ) ) )
                {
                        showError( "Dosya açılamadı. Excel kurulu değilse dosyayı başka bir programla açabilirsiniz." );
                }
        }


        void ListReportDialog::showError( const QString& message )
        {
                QMessageBox::warning( this, "Liste Yazdır", message );
        }

}
