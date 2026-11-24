//  PrintView.cpp
//
//  Copyright (C) 2013-2026  Jaye Evins <evins@snaught.com>
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


#include "PrintView.hpp"

#include "PrinterMonitor.hpp"

#include "MemberPickerDialog.hpp"
#include "merge/MemberDb.hpp"
#include "model/Settings.hpp"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPrintDialog>


namespace glabels
{

        ///
        /// Constructor
        ///
        PrintView::PrintView( QWidget *parent )
                : QWidget(parent), mModel(nullptr), mBlocked(false)
        {
                setupUi( this );

                titleLabel->setText( QString( "<span style='font-size:18pt;'>%1</span>" ).arg( tr("Print") ) );

                auto* printerMonitor = PrinterMonitor::instance();
                loadDestinations( printerMonitor->availablePrinters() );
                connect( printerMonitor, SIGNAL(availablePrintersChanged(QStringList)),
                         this, SLOT(onAvailablePrintersChanged(QStringList)) );

                setDestination( model::Settings::recentPrinter() );

                preview->setRenderer( &mRenderer );

                // Notice for member labels with nobody selected (members start unselected on purpose)
                mMemberNotice = new QFrame();
                mMemberNotice->setFrameShape( QFrame::StyledPanel );
                mMemberNoticeLabel = new QLabel();
                mMemberNoticeLabel->setWordWrap( true );
                mChooseMembersButton = new QPushButton( "Üyeleri Seç…" );
                mSelectAllMembersButton = new QPushButton();
                mEditLabelButton = new QPushButton( "Etiketi Tasarla" );
                auto* buttonRow = new QHBoxLayout();
                buttonRow->addWidget( mEditLabelButton );
                buttonRow->addWidget( mChooseMembersButton );
                buttonRow->addWidget( mSelectAllMembersButton );
                buttonRow->addStretch( 1 );
                auto* noticeLayout = new QVBoxLayout( mMemberNotice );
                noticeLayout->addWidget( mMemberNoticeLabel );
                noticeLayout->addLayout( buttonRow );
                mMemberNotice->setVisible( false );
                verticalLayout_2->insertWidget( verticalLayout_2->indexOf( titleLabel ) + 1, mMemberNotice );

                connect( mChooseMembersButton, &QPushButton::clicked, this, [this] {
                        if ( mModel && mModel->merge()->id() == merge::MemberDb::id() )
                        {
                                MemberPickerDialog dialog( mModel->merge(), this );
                                dialog.exec();
                        }
                } );
                connect( mEditLabelButton, &QPushButton::clicked, this, &PrintView::editLabelRequested );
                connect( mSelectAllMembersButton, &QPushButton::clicked, this, [this] {
                        if ( mModel )
                        {
                                mModel->merge()->selectAll();
                        }
                } );
        }


        ///
        /// Show / hide the "no members selected" notice
        ///
        void PrintView::setNoticeStyle( bool warning )
        {
                mMemberNotice->setStyleSheet( warning
                        ? "QFrame { background-color: #fff3e0; border: 1px solid #ffb74d; border-radius: 6px; }"
                          "QLabel { border: none; background: transparent; color: #4e2600; }"
                        : "QFrame { background-color: #e8f1fd; border: 1px solid #90b8ec; border-radius: 6px; }"
                          "QLabel { border: none; background: transparent; color: #0d2b52; }" );
        }


        void PrintView::updateMemberNotice()
        {
                // An empty design prints blank labels: say so first
                if ( mModel && mModel->objectList().isEmpty() )
                {
                        mMemberNoticeLabel->setText( "<b>Etiket tasarımı boş.</b><br>Etikette henüz yazı yok, bu yüzden "
                                                     "önizleme ve baskı boş çıkar. Düzenle sayfasında soldaki "
                                                     "<b>Üye Alanları</b> listesinden alan ekleyin." );
                        setNoticeStyle( true );
                        mEditLabelButton->setVisible( true );
                        mChooseMembersButton->setVisible( false );
                        mSelectAllMembersButton->setVisible( false );
                        mMemberNotice->setVisible( true );
                        return;
                }
                mEditLabelButton->setVisible( false );
                mChooseMembersButton->setVisible( true );

                bool isMembers = mModel && mModel->merge()->id() == merge::MemberDb::id();
                mMemberNotice->setVisible( isMembers );
                if ( !isMembers )
                {
                        return;
                }
                int nRecords = int( mModel->merge()->recordList().size() );
                int nSelected = mModel->merge()->nSelectedRecords();

                if ( nSelected > 0 )
                {
                        // Who will be printed, always visible, changeable right here
                        QStringList names;
                        for ( const auto& record : mModel->merge()->recordList() )
                        {
                                if ( !record.isSelected() )
                                {
                                        continue;
                                }
                                if ( names.size() == 4 )
                                {
                                        names << QString( "ve %1 üye daha" ).arg( nSelected - 4 );
                                        break;
                                }
                                QString name = record.value( "FIRMA" ).trimmed();
                                if ( name.isEmpty() )
                                {
                                        name = ( record.value( "AD" ) + " " + record.value( "SOYAD" ) ).trimmed();
                                }
                                names << name.toHtmlEscaped();
                        }
                        setNoticeStyle( false );
                        mMemberNoticeLabel->setText( QString( "<b>%1 üye basılacak:</b> %2" )
                                                     .arg( nSelected ).arg( names.join( ", " ) ) );
                        mChooseMembersButton->setText( "Üyeleri Değiştir…" );
                        mSelectAllMembersButton->setVisible( false );
                        return;
                }

                setNoticeStyle( true );
                mChooseMembersButton->setText( "Üyeleri Seç…" );
                mChooseMembersButton->setVisible( nRecords > 0 );
                if ( nRecords == 0 )
                {
                        mMemberNoticeLabel->setText( "<b>Basılacak üye yok.</b><br>Üye veritabanında aktif üye bulunmuyor. "
                                                     "Önce Üyeler sayfasından üye ekleyin." );
                        mSelectAllMembersButton->setVisible( false );
                }
                else
                {
                        mMemberNoticeLabel->setText( "<b>Basılacak üye seçilmedi.</b><br>Kimlere basılacağını buradan "
                                                     "seçebilir ya da tüm aktif üyeleri seçebilirsiniz." );
                        mSelectAllMembersButton->setText( QString( "Tüm aktif üyeleri seç (%1)" ).arg( nRecords ) );
                        mSelectAllMembersButton->setVisible( true );
                }
        }


        ///
        /// Set Model
        ///
        void PrintView::setModel( model::Model* model )
        {
                mModel = model;
                mRenderer.setModel( mModel );

                connect( mModel, SIGNAL(changed()), this, SLOT(onModelChanged()) );

                onModelChanged();
        }


        ///
        /// Available printers changed handler
        ///
        void PrintView::onAvailablePrintersChanged( QStringList printers )
        {
                auto savedSelection = destinationCombo->currentText();
                loadDestinations( printers );
                setDestination( savedSelection );
        }


        ///
        /// Model changed handler
        ///
        void PrintView::onModelChanged()
        {
                printRangeStartPositionSpin->setRange( 1, mModel->frame()->nLabels() );
                printRangeLastPositionSpin->setRange( 1, mModel->frame()->nLabels() );
                mergeStartPositionSpin->setRange( 1, mModel->frame()->nLabels() );

                printRangeBox->setVisible( mModel->merge()->keys().empty() );
                mergeBox->setVisible( !mModel->merge()->keys().empty() );
                mergeOnlyOptions->setVisible( !mModel->merge()->keys().empty() );

                updateMemberNotice();
                onFormChanged();
        }


        ///
        /// Update view
        ///
        void PrintView::updateView()
        {
                if ( mRenderer.nPages() == 1 )
                {
                        if ( mRenderer.nItems() == 1 )
                        {
                                printDescriptionLabel->setText( tr("(Will print a total of 1 item on 1 page.)") );
                        }
                        else
                        {
                                printDescriptionLabel->setText( tr("(Will print a total of %1 items on 1 page.)")
                                                                .arg(mRenderer.nItems()) );
                        }
                }
                else
                {
                        printDescriptionLabel->setText( tr("(Will print a total of %1 items on %2 pages.)")
                                                        .arg(mRenderer.nItems()).arg(mRenderer.nPages()) );
                }

                pageSpin->setRange( 1, mRenderer.nPages() );
                nPagesLabel->setText( QString::number( mRenderer.nPages() ) );

                bool canPrint = mRenderer.nItems();
                printButton->setEnabled( canPrint );
                printDescriptionLabel->setEnabled( canPrint );
                systemDialogButton->setEnabled( canPrint );

                mRenderer.setIPage( pageSpin->value() - 1 ); // Update preview
        }


        ///
        /// Form changed handler
        ///
        void PrintView::onFormChanged()
        {
                if ( !mBlocked )
                {
                        mBlocked = true;

                        if ( mModel->merge()->keys().empty() )
                        {
                                // Simple project (no merge)
                                if ( printRangePagesRadio->isChecked() )
                                {
                                        // Print full sheets of labels
                                        int nItemsPerPage = mModel->frame()->nLabels();

                                        printRangePagesSpin->setEnabled( true );

                                        printRangeStartPositionSpin->setEnabled( false );
                                        printRangeLastPositionSpin->setEnabled( false );

                                        printRangeStartPositionSpin->setMaximum( nItemsPerPage );
                                        printRangeLastPositionSpin->setMinimum( 1 );

                                        printRangeStartPositionSpin->setValue( 1 );
                                        printRangeLastPositionSpin->setValue( nItemsPerPage );

                                        mRenderer.setNCopies( printRangePagesSpin->value()*nItemsPerPage );
                                        mRenderer.setStartItem( 0 );
                                }
                                else
                                {
                                        // Print a partial sheet of labels
                                        int iStart = printRangeStartPositionSpin->value();
                                        int iLast  = printRangeLastPositionSpin->value();

                                        printRangePagesSpin->setEnabled( false );

                                        printRangeStartPositionSpin->setEnabled( true );
                                        printRangeLastPositionSpin->setEnabled( true );

                                        printRangeStartPositionSpin->setMaximum( iLast );
                                        printRangeLastPositionSpin->setMinimum( iStart );

                                        mRenderer.setNCopies( iLast - iStart + 1 );
                                        mRenderer.setStartItem( iStart - 1 );
                                }
                        }
                        else
                        {
                                // Project with merge
                                bool isMultipleCopies = mergeCopiesSpin->value() > 1;

                                mergeCollateCombo->setEnabled ( isMultipleCopies );
                                mergeGroupCombo->setEnabled ( isMultipleCopies );

                                mRenderer.setNCopies( mergeCopiesSpin->value() );
                                mRenderer.setIsCollated( mergeCollateCombo->currentIndex() == 1 );
                                mRenderer.setAreGroupsContiguous( mergeGroupCombo->currentIndex() == 0 );
                                mRenderer.setStartItem( mergeStartPositionSpin->value() - 1 );
                        }

                        mRenderer.setPrintOutlines( printOutlinesCheck->isChecked() );
                        mRenderer.setPrintCropMarks( printCropMarksCheck->isChecked() );
                        mRenderer.setPrintReverse( printReverseCheck->isChecked() );

                        updateView();

                        mBlocked = false;
                }
        }


        ///
        /// Print Button Clicked handler
        ///
        void PrintView::onPrintButtonClicked()
        {
                auto printerName = destinationCombo->currentText();
                auto printerInfo = QPrinterInfo::printerInfo( printerName );
                bool isPrinter = !printerInfo.isNull();

                QPrinter printer( QPrinter::HighResolution );
                printer.setColorMode( QPrinter::Color );

                if ( isPrinter )
                {
                        printer.setPrinterName( printerName );
                        mRenderer.print( &printer );
                        model::Settings::setRecentPrinter( printerName );
                }
                else
                {
                        QString fileName =
                                QFileDialog::getSaveFileName( this,
                                                              tr("Print to file (PDF)"),
                                                              defaultPdf(),
                                                              tr("PDF files (*.pdf);;All files (*)"),
                                                              nullptr,
                                                              QFileDialog::DontConfirmOverwrite        );
                        if ( !fileName.isEmpty() )
                        {
                                if ( QFileInfo::exists(fileName) )
                                {
                                        QMessageBox msgBox( this );
                                        msgBox.setWindowTitle( tr("Print to file (PDF)") );
                                        msgBox.setIcon( QMessageBox::Warning );
                                        msgBox.setText( tr("%1 already exists.").arg(fileName) );
                                        msgBox.setInformativeText( tr("Do you want to overwrite it?") );
                                        msgBox.setStandardButtons( QMessageBox::Yes | QMessageBox::No );
                                        msgBox.setDefaultButton( QMessageBox::No );

                                        if ( msgBox.exec() == QMessageBox::No )
                                        {
                                                return;
                                        }
                                }

                                printer.setOutputFileName( fileName );
                                printer.setOutputFormat( QPrinter::PdfFormat );
                                mRenderer.print( &printer );
                        }
                }
        }


        ///
        /// System Dialog Button Clicked handler
        ///
        void PrintView::onSystemDialogButtonClicked()
        {
                QPrinter printer( QPrinter::HighResolution );
                printer.setColorMode( QPrinter::Color );
                printer.setPrinterName( destinationCombo->currentText() );

                QPrintDialog printDialog( &printer, this );
                printDialog.setOption( QAbstractPrintDialog::PrintToFile,        true );
                printDialog.setOption( QAbstractPrintDialog::PrintSelection,     false );
                printDialog.setOption( QAbstractPrintDialog::PrintPageRange,     false );
                printDialog.setOption( QAbstractPrintDialog::PrintShowPageSize,  true );
                printDialog.setOption( QAbstractPrintDialog::PrintCollateCopies, false );
                printDialog.setOption( QAbstractPrintDialog::PrintCurrentPage,   false );

                if ( printDialog.exec() == QDialog::Accepted )
                {
                        mRenderer.print( &printer );

                        if ( !printer.printerName().isEmpty() )
                        {
                                model::Settings::setRecentPrinter( printer.printerName() );
                        }
                }
        }


        ///
        /// Load available printers
        ///
        void PrintView::loadDestinations( const QStringList& printers )
        {
                destinationCombo->blockSignals( true );

                destinationCombo->clear();
                for ( auto& printerName : printers )
                {
                        destinationCombo->addItem( QIcon::fromTheme( "glabels-print" ), printerName  );
                }

                if ( destinationCombo->count() )
                {
                        destinationCombo->insertSeparator( destinationCombo->count() );
                }
                destinationCombo->addItem( QIcon::fromTheme( "glabels-file-new" ), tr( "Print to file (PDF)" ) );

                destinationCombo->blockSignals( false );
        }


        ///
        /// Generate default PDF filename
        ///
        QString PrintView::defaultPdf()
        {
                if ( !mModel )
                {
                        return "output.pdf";
                }
                return mModel->dirPath() + "/" + mModel->shortName() + ".pdf";
        }


        ///
        /// Set destination to printerName if valid, or to a fallback value
        ///
        void PrintView::setDestination( const QString& printerName )
        {
                destinationCombo->blockSignals( true );

                auto printerInfo = QPrinterInfo::printerInfo( printerName );

                if ( !printerInfo.isNull() )
                {
                        // printerName is a valid printer
                        destinationCombo->setCurrentText( printerName );
                }
                else
                {
                        auto defaultPrinterName = QPrinterInfo::defaultPrinterName();
                        auto defaultPrinterInfo = QPrinterInfo::printerInfo( defaultPrinterName );
                        if ( !defaultPrinterInfo.isNull() )
                        {
                                // defaultPinterName is a valid printer
                                destinationCombo->setCurrentText( defaultPrinterName );
                        }
                        else
                        {
                                // No default printer available, set to first item in combo (probably "print to file")
                                destinationCombo->setCurrentIndex( 0 );
                        }
                }

                destinationCombo->blockSignals( false );
        }


} // namespace glabels
