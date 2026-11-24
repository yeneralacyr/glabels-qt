//  CheckMarkDelegate.cpp
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

#include "CheckMarkDelegate.hpp"

#include <QApplication>
#include <QPainter>
#include <QPolygonF>
#include <QStyle>

#include <algorithm>


namespace glabels
{

        void CheckMarkDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
        {
                // Background (row tint / current-row highlight) without the native indicator
                QStyleOptionViewItem opt( option );
                initStyleOption( &opt, index );
                opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;
                opt.text.clear();
                const QWidget* widget = option.widget;
                QStyle* style = widget ? widget->style() : QApplication::style();
                style->drawControl( QStyle::CE_ItemViewItem, &opt, painter, widget );

                bool checked = index.data( Qt::CheckStateRole ).toInt() == Qt::Checked;
                int side = std::min( 18, option.rect.height() - 6 );
                QRectF box( option.rect.center().x() - side / 2.0 + 0.5,
                            option.rect.center().y() - side / 2.0 + 0.5, side, side );

                painter->save();
                painter->setRenderHint( QPainter::Antialiasing );
                if ( checked )
                {
                        painter->setPen( Qt::NoPen );
                        painter->setBrush( QColor( 0x1e, 0x6f, 0xe0 ) );
                        painter->drawRoundedRect( box, 4, 4 );
                        QPen pen( Qt::white, std::max( 2.0, side / 7.0 ) );
                        pen.setCapStyle( Qt::RoundCap );
                        pen.setJoinStyle( Qt::RoundJoin );
                        painter->setPen( pen );
                        QPointF a( box.left() + side * 0.24, box.top() + side * 0.52 );
                        QPointF b( box.left() + side * 0.43, box.top() + side * 0.71 );
                        QPointF c( box.left() + side * 0.77, box.top() + side * 0.31 );
                        painter->drawPolyline( QPolygonF( { a, b, c } ) );
                }
                else
                {
                        painter->setPen( QPen( QColor( 0x7a, 0x7a, 0x7a ), 1.5 ) );
                        painter->setBrush( Qt::white );
                        painter->drawRoundedRect( box, 4, 4 );
                }
                painter->restore();
        }


        bool CheckMarkDelegate::editorEvent( QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex& )
        {
                return false;
        }

}
