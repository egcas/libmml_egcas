/****************************************************************************
** 
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
** 
** This file is part of a Qt Solutions component.
**
** Commercial Usage  
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Solutions Commercial License Agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and Nokia.
** 
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
** 
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.1, included in the file LGPL_EXCEPTION.txt in this
** package.
** 
** GNU General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
** 
** Please note Third Party Software included with Qt Solutions may impose
** additional restrictions and it is the user's responsibility to ensure
** that they have met the licensing requirements of the GPL, LGPL, or Qt
** Solutions Commercial license and the relevant license of the Third
** Party Software they are using.
** 
** If you are unsure which license is appropriate for your use, please
** contact Nokia at qt-info@nokia.com.
** 
****************************************************************************/

/*Some further improvements done by:
  Uwe Rat
  Alan Garny
  Johannes Maier  03.05.2014
*/


#ifndef _QWT_MML_ENTITY_TABLE_H_
#define _QWT_MML_ENTITY_TABLE_H_

#include <qstring.h>
#include <QHash>

/**
 * @brief The QwtMMLEntityTable class implements some hash lists for searching
 * for MathMl spec names and their character representation
 */
class QwtMMLEntityTable
{
public:
    QwtMMLEntityTable();
    virtual ~QwtMMLEntityTable() {}
    /**
     * @brief setup the document with all character entities in the mathml code
     * @param text the mathml text
     * @param prefix_lines the number of prefix lines
     * @return the mathml code with prefixed xml lines (entities)
     */
    QString entities(QString text, uint &prefix_lines);
    /**
     * @brief lookup the name of the character entitiy given
     * @param value character entity as value representation
     * @return the character entity name found
     */
    QList<QString> search(const QString &value) const;
private:
    static QHash<QString, QString> valueLookup;
    static QMultiHash<QString, QString> nameLookup;
    static QHash<QString, QString> initValueLookup(void);
    static QHash<QString, QString> initNameLookup(void);

};

#endif





