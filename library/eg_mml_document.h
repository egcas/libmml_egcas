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

#ifndef _QWT_MML_DOCUMENT_H_
#define _QWT_MML_DOCUMENT_H_

#include <qcolor.h>
#include <qstring.h>
#include <qsize.h>
#include <QObject>
#include <QRectF>

class QPainter;
class QPointF;

class EgMmlDocument;

enum class EgMathMlNodeType
{
    NoNode = 0, MiNode, MnNode, MfracNode, MrowNode, MsqrtNode,
    MrootNode, MsupNode, MsubNode, MsubsupNode, MoNode,
    MstyleNode, TextNode, MphantomNode, MfencedNode,
    MtableNode, MtrNode, MtdNode, MoverNode, MunderNode,
    MunderoverNode, MerrorNode, MtextNode, MpaddedNode,
    MspaceNode, MalignMarkNode, UnknownNode
};

/**
 * @brief The EgRenderingData class that encapsulates all dimension data that is calculated during rendering.
 * With this data it is possible to calculate where the formula characters are shown on the screen.
 */
class EgRenderingData {
public:
        EgRenderingData(): m_nodeId{0}, m_index{0}, m_itemRect{QRectF()} {}
        quint32 m_nodeId;       ///< the node id that is extracted from the mathml node (id attribute of the node)
        quint32 m_index;        ///< usually 0, is incremented with each character (if there are more) within one malthml node
        QRectF m_itemRect;      ///< the rectangle that surrounds a item shown on the screen
};

class EgMathMLDocument : public QObject
{
        Q_OBJECT

public:

    enum MmlFont
    {
        NormalFont,
        FrakturFont,
        SansSerifFont,
        ScriptFont,
        MonospaceFont,
        DoublestruckFont
    };

    EgMathMLDocument();
    ~EgMathMLDocument();

    void clear();

    bool setContent( const QString &text, QString *errorMsg = 0,
                     int *errorLine = 0, int *errorColumn = 0 );
    void paint( QPainter *, const QPointF &pos ) const;
    QSizeF size();

    QString fontName( MmlFont type ) const;
    void setFontName( MmlFont type, const QString &name );

    qreal baseFontPixelSize();
    void setBaseFontPixelSize( qreal size );

    QColor foregroundColor() const;
    void setForegroundColor( const QColor &color );

    QColor backgroundColor() const;
    void setBackgroundColor( const QColor &color );

    bool drawFrames() const;
    void setDrawFrames( bool );

    /**
     * @brief setMmToPixelFactor
     * @param factor
     */
    static void setMmToPixelFactor(qreal factor);

    /**
     * @brief MmToPixelFactor
     * @return
     */
    static qreal MmToPixelFactor(void);

signals:
    /**
     * @brief updatedRect defines a signal to update the surrounding rectangle and other data of a MathML math element
     * @param node the node type that caused the signal to be emitted (enum EgMathMLNodeType)
     * @param layer the layer number on which the current node is (starts with 0).
     * @param sibling is the number of the current sibling inside a layer (starts with 0).
     * @param rect the rect of the current node
     */
    void updatedRect(const EgMathMlNodeType& node, const quint32& layer, const quint32& sibling, const QRectF& rect);

private:

    EgMmlDocument *m_doc;        ///< pointer to math Ml formula
    QSizeF m_size;                ///< this improves speed when calculating the bounding box algorithm while painting
};

#endif
