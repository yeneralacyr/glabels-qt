//  CheckMarkDelegate.hpp
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

#ifndef CheckMarkDelegate_hpp
#define CheckMarkDelegate_hpp


#include <QStyledItemDelegate>


namespace glabels
{

        ///
        /// Draws a clear check box for a Qt::CheckStateRole column. Native check boxes inside
        /// table cells are faint or invisible on some platforms/styles (notably macOS).
        /// The delegate never edits: views toggle the state themselves on a row click.
        ///
        class CheckMarkDelegate : public QStyledItemDelegate
        {
        public:
                using QStyledItemDelegate::QStyledItemDelegate;

                void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override;
                bool editorEvent( QEvent* event, QAbstractItemModel* model,
                                  const QStyleOptionViewItem& option, const QModelIndex& index ) override;
        };

}


#endif // CheckMarkDelegate_hpp
