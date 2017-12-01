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


#include "eg_mml_document.h"
#include "eg_mml_entity_table.h"

#include <qapplication.h>
#include <qdebug.h>
#include <qdesktopwidget.h>
#include <qdom.h>
#include <qfontdatabase.h>
#include <qmap.h>
#include <qmath.h>
#include <qpainter.h>

// *******************************************************************
// Declarations
// *******************************************************************

static const qreal   g_mfrac_spacing          = 0.05;
static const qreal   g_mroot_base_margin      = 0.1;
static const qreal   g_mroot_base_line        = 0.5;
static const qreal   g_script_size_multiplier = 0.5;
static const qreal   g_sup_shift_multiplier   = 0.5;
static const char *  g_subsup_spacing         = "veryverythinmathspace";
static const qreal   g_min_font_pixel_size    = 8.0;
static qreal         g_min_font_pixel_size_calc = 8.0;
static const ushort  g_radical                = ( 0x22 << 8 ) | 0x1B;
static const int     g_oper_spec_rows         = 9;

static const int g_radical_points_size = 11;
static const QPointF g_radical_points[] = { QPointF( 0.0,         0.344439758 ),
                                            QPointF( 0.217181096, 0.419051636 ),
                                            QPointF( 0.557377049, 0.102829829 ),
                                            QPointF( 0.942686988, 1.048864253 ),
                                            QPointF( 1.0,         1.048864253 ),
                                            QPointF( 1.0,         1.0         ),
                                            QPointF( 1.0,         1.0         ),
                                            QPointF( 0.594230277, 0.0         ),
                                            QPointF( 0.516457480, 0.0         ),
                                            QPointF( 0.135213883, 0.352172079 ),
                                            QPointF( 0.024654201, 0.316221808 )
                                          };

static EgMMLEntityTable mmlEntityTable;

struct EgMml
{

    enum MathVariant
    {
        NormalMV       = 0x0000,
        BoldMV         = 0x0001,
        ItalicMV       = 0x0002,
        DoubleStruckMV = 0x0004,
        ScriptMV       = 0x0008,
        FrakturMV      = 0x0010,
        SansSerifMV    = 0x0020,
        MonospaceMV    = 0x0040
    };

    enum FormType { PrefixForm, InfixForm, PostfixForm };
    enum ColAlign { ColAlignLeft, ColAlignCenter, ColAlignRight };
    enum RowAlign { RowAlignTop, RowAlignCenter, RowAlignBottom,
                    RowAlignAxis, RowAlignBaseline
                  };
    enum FrameType { FrameNone, FrameSolid, FrameDashed };

    struct FrameSpacing
    {
        FrameSpacing( qreal hor = 0.0, qreal ver = 0.0 )
            : m_hor( hor ), m_ver( ver ) {}
        qreal m_hor, m_ver;
    };
};

struct EgMmlOperSpec
{
    enum StretchDir { NoStretch, HStretch, VStretch, HVStretch };

#if 1
    QString name;
#endif
    EgMml::FormType form;
    const char *attributes[g_oper_spec_rows];
    StretchDir stretch_dir;
};

struct EgMmlNodeSpec
{
    EgMathMlNodeType type;
    const char *tag;
    const char *type_str;
    int child_spec;
    const char *child_types;
    const char *attributes;

    enum ChildSpec
    {
        ChildAny     = -1, // any number of children allowed
        ChildIgnore  = -2, // do not build subexpression of children
        ImplicitMrow = -3  // if more than one child, build mrow
    };
};

typedef QMap<QString, QString> EgMmlAttributeMap;
class EgMmlNode;

/**
 * @brief The EgRendAdjustBits enum some adjustment bits for rendering stuff
 */
enum class EgRendAdjustBits {
        Nothing = 0, translateLspace = 1, translateTxt = 2
};

/**
 * @brief operator| operator overloading for EgRendAdjustBits
 */
inline EgRendAdjustBits operator|(EgRendAdjustBits a, EgRendAdjustBits b)
{
  return EgRendAdjustBits(static_cast<int>(a) | static_cast<int>(b));
}

/**
 * @brief operator| operator overloading for EgRendAdjustBits
 */
inline EgRendAdjustBits operator&(EgRendAdjustBits a, EgRendAdjustBits b)
{
  return EgRendAdjustBits(static_cast<int>(a) & static_cast<int>(b));
}

/**
 * @brief The EgAddRendData class provides some additional data for post calculation of the rendering data
 */
class EgAddRendData {
public:
        EgAddRendData() : m_index{0}, m_node{nullptr}, m_bits{EgRendAdjustBits::Nothing} {}
        EgAddRendData(int index, EgMmlNode* node = nullptr, EgRendAdjustBits bits = EgRendAdjustBits::Nothing) :
                      m_index{index}, m_node{node}, m_bits{bits} {}
        int m_index;                              ///< index position inside the rendering data vector
        EgMmlNode *m_node;                        ///< pointer to the node we need for later adjustments
        EgRendAdjustBits m_bits;                  ///< field to save what needs to be adjusted later on
};

class EgMmlDocument : public EgMml
{
        friend class EgMmlNode;
        friend class EgMmlTextNode;

public:
    EgMmlDocument();
    EgMmlDocument(EgMathMLDocument* egMathMLDocument);
    ~EgMmlDocument();
    void clear();

    bool setContent( const QString &text, QString *errorMsg = 0,
                     int *errorLine = 0, int *errorColumn = 0 );
    void paint( QPainter *painter, const QPointF &pos );
    void dump() const;
    QSizeF size() const;
    void layout();

    QString fontName( EgMathMLDocument::MmlFont type ) const;
    void setFontName( EgMathMLDocument::MmlFont type, const QString &name );

    qreal baseFontPixelSize() const { return m_base_font_pixel_size; }
    void setBaseFontPixelSize( qreal size ) { m_base_font_pixel_size = size; clearRendering();}

    QColor foregroundColor() const { return m_foreground_color; }
    void setForegroundColor( const QColor &color ) { m_foreground_color = color; }

    QColor backgroundColor() const { return m_background_color; }
    void setBackgroundColor( const QColor &color ) { m_background_color = color; }

    bool drawFrames() const { return m_draw_frames; }
    void setDrawFrames( const bool &drawFrames ) { m_draw_frames = drawFrames; }

    static void setMmToPixelFactor(qreal factor) { s_MmToPixelFactor = factor; g_min_font_pixel_size_calc = factor * g_min_font_pixel_size; }
    static qreal MmToPixelFactor(void) { return s_MmToPixelFactor; }

    /**
     * @brief doPostProcessing do some post processing of the rendering data
     */
    void doPostProcessing(void);
    /**
     * @brief optimizeSize reduces memory size of the formula data
     */
    void optimizeSize(void);
    /**
     * @brief getRenderingPositions returns the rendering positions (and dimensions) of any symbol rendered that has an
     * id as MathMl attribute. The id given must be a number.
     * @return the rendering position and dimension, along with the id information given with the mathml code as
     * attribute
     */
    QVector<EgRenderingPosition> getRenderingPositions(void);
    /**
     * @brief appendRenderingData append rendering data
     * @param nodeId rendering data with the node id to add
     * @param index rendering data with the index to add
     * @param node a pointer to the node to add rendering data for
     * @param data must a special post processing be done on the rendering data
     */
    bool appendRenderingData(quint32 nodeId, quint32 index, EgMmlNode* node, EgRendAdjustBits data);
    /**
     * @brief updateRenderingData update rendering data
     * @param nodeId rendering data with the node id to add
     * @param index rendering data with the index to add
     * @param position position to update
     */
    void updateRenderingData(quint32 nodeId, quint32 index, QRectF position);
    /**
     * @brief clearRendering clears all rendering data
     */
    void clearRendering(void);
    /**
     * @brief renderingComplete check if rendering data has already been completed (no need to adjust positon data
     * anymore)
     * @return true if completed, false if not
     */
    bool renderingComplete(void) { return m_renderingComplete; }
    /**
     * @brief setRenderingComplete marks rendering as completed, subsequent calls to paint won't modify rendering
     * positions
     */
    void setRenderingComplete(void) { m_renderingComplete = true; }
private:
    void init(void);
    static void static_init(void);
    void _dump( const EgMmlNode *node, const QString &indent ) const;
    bool insertChild( EgMmlNode *parent, EgMmlNode *new_node,
                      QString *errorMsg );

    EgMmlNode *domToMml( const QDomNode &dom_node, bool *ok,
                          QString *errorMsg );
    EgMmlNode *createNode( EgMathMlNodeType type,
                            const EgMmlAttributeMap &mml_attr,
                            const QString &mml_value, QString *errorMsg );
    EgMmlNode *createImplicitMrowNode( const QDomNode &dom_node, bool *ok,
                                     QString *errorMsg );

    void insertOperator( EgMmlNode *node, const QString &text );

    EgMmlNode *m_root_node;

    static QString s_normal_font_name;
    static QString s_fraktur_font_name;
    static QString s_sans_serif_font_name;
    static QString s_script_font_name;
    static QString s_monospace_font_name;
    static QString s_doublestruck_font_name;
    qreal m_base_font_pixel_size;
    QColor m_foreground_color;
    QColor m_background_color;
    bool m_draw_frames;
    static qreal s_MmToPixelFactor;
    static bool s_initialized;
    EgMathMLDocument* m_EgMathMLDocument;
    QHash<quint64, EgAddRendData> m_nodeIdLookup;   ///< lookup if any node id has already been handled
    QHash <quint32, QVector<EgRenderingPosition>> m_renderingData;   ///< rendering and dimension data of current formula document

    bool m_renderingComplete;           ///< marks all rendering data as complete
};

qreal EgMmlDocument::s_MmToPixelFactor = 1;
QString EgMmlDocument::s_normal_font_name = "";
QString EgMmlDocument::s_fraktur_font_name = "Fraktur";
QString EgMmlDocument::s_sans_serif_font_name = "Luxi Sans";
QString EgMmlDocument::s_script_font_name = "Urw Chancery L";
QString EgMmlDocument::s_monospace_font_name = "Luxi Mono";
QString EgMmlDocument::s_doublestruck_font_name = "Doublestruck";
bool EgMmlDocument::s_initialized = false;


class EgMmlNode : public EgMml
{
    friend class EgMmlDocument;

public:
    EgMmlNode( EgMathMlNodeType type, EgMmlDocument *document,
                const EgMmlAttributeMap &attribute_map );
    virtual ~EgMmlNode();

    // Mml stuff
    EgMathMlNodeType nodeType() const { return m_node_type; }

    virtual QString toStr() const;

    void setRelOrigin( const QPointF &rel_origin );

    void stretchTo( const QRectF &rect );
    QPointF devicePoint( const QPointF &p ) const;

    QRectF myRect() const { return m_my_rect; }
    virtual void setMyRect( const QRectF &rect ) { m_my_rect = rect; }
    void updateMyRect();

    QRectF parentRect() const;
    virtual QRectF deviceRect() const;

    virtual void stretch();
    virtual void layout();
    /**
     * @brief layoutHook is called before layout function of each child is called. Subclasses can therefore intercept
     * the layout functions of their the childs
     * @param childIndex the child index of the childs
     */
    virtual void layoutHook(quint32 childIndex) {}
    virtual void paint( QPainter *painter, qreal x_scaling, qreal y_scaling );

    qreal basePos() const;

    qreal em() const;
    qreal ex() const;

    QString explicitAttribute( const QString &name, const QString &def = QString::null ) const;
    QString inheritAttributeFromMrow( const QString &name, const QString &def = QString::null ) const;

    virtual QFont font() const;
    virtual QColor color() const;
    virtual QColor background() const;
    virtual int scriptlevel( const EgMmlNode *child = 0 ) const;
    qreal baseFontPixelSize( void ) const { return m_document->baseFontPixelSize(); }


    // Node stuff
    EgMmlNode *parent() const { return m_parent; }
    EgMmlNode *firstChild() const { return m_first_child; }
    EgMmlNode *nextSibling() const { return m_next_sibling; }
    EgMmlNode *previousSibling() const { return m_previous_sibling; }
    EgMmlNode *lastSibling() const;
    EgMmlNode *firstSibling() const;
    bool isLastSibling() const { return m_next_sibling == 0; }
    bool isFirstSibling() const { return m_previous_sibling == 0; }
    bool hasChildNodes() const { return m_first_child != 0; }
    /**
     * @brief getNodeId returns the node id of the current node
     * @return the node id of the current node
     */
    quint32 getNodeId(void) const {return m_nodeId;}

protected:
    virtual void layoutSymbol();
    virtual void paintSymbol( QPainter *painter, qreal, qreal ) const;
    virtual QRectF symbolRect() const { return QRectF( 0.0, 0.0, 0.0, 0.0 ); }
    EgMmlNode *parentWithExplicitAttribute( const QString &name, EgMathMlNodeType type = EgMathMlNodeType::NoNode );
    qreal interpretSpacing( const QString &value, bool *ok ) const;
    qreal lineWidth() const;

    quint32 m_nodeId;   ///< the node id given with attribute id
    EgMmlDocument *m_document;
    EgMmlNode *m_parent,
               *m_first_child,
               *m_next_sibling,
               *m_previous_sibling;

private:
    EgMmlAttributeMap m_attribute_map;
    bool m_stretched;
    QRectF m_my_rect, m_parent_rect;
    QPointF m_rel_origin;
    EgMathMlNodeType m_node_type;
};

class EgMmlTokenNode : public EgMmlNode
{
public:
    EgMmlTokenNode( EgMathMlNodeType type, EgMmlDocument *document,
                  const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( type, document, attribute_map ) {}

    QString text() const;
};

class EgMmlMphantomNode : public EgMmlNode
{
public:
    EgMmlMphantomNode( EgMmlDocument *document,
                     const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MphantomNode, document, attribute_map ) {}

    virtual void paint( QPainter *, qreal, qreal ) {}
};

class EgMmlUnknownNode : public EgMmlNode
{
public:
    EgMmlUnknownNode( EgMmlDocument *document,
                    const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::UnknownNode, document, attribute_map ) {}
};

class EgMmlMfencedNode : public EgMmlNode
{
public:
    EgMmlMfencedNode( EgMmlDocument *document,
                    const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MfencedNode, document, attribute_map ) {}
};

class EgMmlMalignMarkNode : public EgMmlNode
{
public:
    EgMmlMalignMarkNode( EgMmlDocument *document )
        : EgMmlNode( EgMathMlNodeType::MalignMarkNode, document, EgMmlAttributeMap() ) {}
};

class EgMmlMfracNode : public EgMmlNode
{
public:
    EgMmlMfracNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MfracNode, document, attribute_map ) {}

    EgMmlNode *numerator() const;
    EgMmlNode *denominator() const;
    /**
     * @brief layoutHook is called before layout function of each child is called. Subclasses can therefore intercept
     * the layout functions of their the childs
     * @param childIndex the child index of the childs
     */
    virtual void layoutHook(quint32 childIndex);

protected:
    virtual void layoutSymbol();
    virtual void paintSymbol( QPainter *painter, qreal x_scaling, qreal y_scaling ) const;
    virtual QRectF symbolRect() const;

private:
    qreal lineThickness() const;
};

class EgMmlMrowNode : public EgMmlNode
{
public:
    EgMmlMrowNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MrowNode, document, attribute_map ) {}
};

class EgMmlRootBaseNode : public EgMmlNode
{
public:
    EgMmlRootBaseNode( EgMathMlNodeType type, EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( type, document, attribute_map ) {}

    EgMmlNode *base() const;
    EgMmlNode *index() const;

    virtual int scriptlevel( const EgMmlNode *child = 0 ) const;

protected:
    virtual void layoutSymbol();
    virtual void paintSymbol( QPainter *painter, qreal x_scaling, qreal y_scaling ) const;
    virtual QRectF symbolRect() const;

private:
    QRectF baseRect() const;
    QRectF radicalRect() const;
    qreal radicalMargin() const;
    qreal radicalLineWidth() const;
};

class EgMmlMrootNode : public EgMmlRootBaseNode
{
public:
    EgMmlMrootNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlRootBaseNode( EgMathMlNodeType::MrootNode, document, attribute_map ) {}
};

class EgMmlMsqrtNode : public EgMmlRootBaseNode
{
public:
    EgMmlMsqrtNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlRootBaseNode( EgMathMlNodeType::MsqrtNode, document, attribute_map ) {}

};


class EgMmlTextNode : public EgMmlNode
{
public:
    EgMmlTextNode( const QString &text, EgMmlDocument *document );

    virtual QString toStr() const;
    QString text() const { return m_text; }

    // TextNodes are not xml elements, so they can't have attributes of
    // their own. Everything is taken from the parent.
    virtual QFont font() const { return parent()->font(); }
    virtual int scriptlevel( const EgMmlNode* = 0 ) const { return parent()->scriptlevel( this ); }
    virtual QColor color() const { return parent()->color(); }
    virtual QColor background() const { return parent()->background(); }

protected:
    virtual void paintSymbol( QPainter *painter, qreal x_scaling, qreal y_scaling ) const;
    virtual QRectF symbolRect() const;

    QString m_text;

private:
    bool isInvisibleOperator() const;
    /**
     * @brief generateTxtRenderingData generates position data from text node during rendering
     * @param rect the rectangle that surrounds the complete text
     */
    void generateTxtRenderingData(QRectF rect) const;
    /**
     * @brief TxtRenderingDataHelper calculates the new width of the text given and sets the position rectangles in the
     * document
     * @param parentRect the parent rectangle containing the char rectangle to calculate
     * @param text the text to calculate (usually with one char more than in the previous run)
     * @param previousWidth the previous width of the text from the previous run
     * @return the with calculated this time
     */
    qreal TxtRenderingDataHelper(QRectF parentRect, QString text, qreal previousWidth) const;
};

class EgMmlMiNode : public EgMmlTokenNode
{
public:
    EgMmlMiNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlTokenNode( EgMathMlNodeType::MiNode, document, attribute_map ) {}
};

class EgMmlMnNode : public EgMmlTokenNode
{
public:
    EgMmlMnNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlTokenNode( EgMathMlNodeType::MnNode, document, attribute_map ) {}
};

class EgMmlSubsupBaseNode : public EgMmlNode
{
public:
    EgMmlSubsupBaseNode( EgMathMlNodeType type, EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( type, document, attribute_map ) {}

    EgMmlNode *base() const;
    EgMmlNode *sscript() const;

    virtual int scriptlevel( const EgMmlNode *child = 0 ) const;
};

class EgMmlMsupNode : public EgMmlSubsupBaseNode
{
public:
    EgMmlMsupNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlSubsupBaseNode( EgMathMlNodeType::MsupNode, document, attribute_map ) {}

protected:
    virtual void layoutSymbol();
};

class EgMmlMsubNode : public EgMmlSubsupBaseNode
{
public:
    EgMmlMsubNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlSubsupBaseNode( EgMathMlNodeType::MsubNode, document, attribute_map ) {}

protected:
    virtual void layoutSymbol();
};

class EgMmlMsubsupNode : public EgMmlNode
{
public:
    EgMmlMsubsupNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MsubsupNode, document, attribute_map ) {}

    EgMmlNode *base() const;
    EgMmlNode *superscript() const;
    EgMmlNode *subscript() const;

    virtual int scriptlevel( const EgMmlNode *child = 0 ) const;

protected:
    virtual void layoutSymbol();
};

class EgMmlMoNode : public EgMmlTokenNode
{
public:
    EgMmlMoNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map );

    QString dictionaryAttribute( const QString &name ) const;
    virtual void stretch();
    virtual qreal lspace() const;
    virtual qreal rspace() const;

    virtual QString toStr() const;

protected:
    virtual void layoutSymbol();
    virtual QRectF symbolRect() const;

    virtual FormType form() const;

private:
    const EgMmlOperSpec *m_oper_spec;
};

class EgMmlMstyleNode : public EgMmlNode
{
public:
    EgMmlMstyleNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MstyleNode, document, attribute_map ) {}
};

class EgMmlTableBaseNode : public EgMmlNode
{
public:
    EgMmlTableBaseNode( EgMathMlNodeType type, EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( type, document, attribute_map ) {}
};

class EgMmlMtableNode : public EgMmlTableBaseNode
{
public:
    EgMmlMtableNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlTableBaseNode( EgMathMlNodeType::MtableNode, document, attribute_map ) {}

    qreal rowspacing() const;
    qreal columnspacing() const;
    qreal framespacing_hor() const;
    qreal framespacing_ver() const;
    FrameType frame() const;
    FrameType columnlines( int idx ) const;
    FrameType rowlines( int idx ) const;

protected:
    virtual void layoutSymbol();
    virtual QRectF symbolRect() const;
    virtual void paintSymbol( QPainter *painter, qreal x_scaling, qreal y_scaling ) const;

private:
    struct CellSizeData
    {
        void init( const EgMmlNode *first_row );
        QList<qreal> col_widths, row_heights;
        int numCols() const { return col_widths.count(); }
        int numRows() const { return row_heights.count(); }
        qreal colWidthSum() const;
        qreal rowHeightSum() const;
    };

    CellSizeData m_cell_size_data;
    qreal m_content_width, m_content_height;
};

class EgMmlMtrNode : public EgMmlTableBaseNode
{
public:
    EgMmlMtrNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlTableBaseNode( EgMathMlNodeType::MtrNode, document, attribute_map ) {}
    void layoutCells( const QList<qreal> &col_widths, qreal col_spc );
};

class EgMmlMtdNode : public EgMmlTableBaseNode
{
public:
    EgMmlMtdNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlTableBaseNode( EgMathMlNodeType::MtdNode, document, attribute_map ) { m_scriptlevel_adjust = 0; }
    virtual void setMyRect( const QRectF &rect );

    ColAlign columnalign();
    RowAlign rowalign();
    int colNum() const;
    int rowNum() const;
    virtual int scriptlevel( const EgMmlNode *child = 0 ) const;

private:
    int m_scriptlevel_adjust; // added or subtracted to scriptlevel to
                              // make contents fit the cell
};

class EgMmlMoverNode : public EgMmlNode
{
public:
    EgMmlMoverNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MoverNode, document, attribute_map ) {}
    virtual int scriptlevel( const EgMmlNode *node = 0 ) const;

protected:
    virtual void layoutSymbol();
};

class EgMmlMunderNode : public EgMmlNode
{
public:
    EgMmlMunderNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MunderNode, document, attribute_map ) {}
    virtual int scriptlevel( const EgMmlNode *node = 0 ) const;

protected:
    virtual void layoutSymbol();
};

class EgMmlMunderoverNode : public EgMmlNode
{
public:
    EgMmlMunderoverNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MunderoverNode, document, attribute_map ) {}
    virtual int scriptlevel( const EgMmlNode *node = 0 ) const;

protected:
    virtual void layoutSymbol();
};

class EgMmlMerrorNode : public EgMmlNode
{
public:
    EgMmlMerrorNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MerrorNode, document, attribute_map ) {}
};

class EgMmlMtextNode : public EgMmlNode
{
public:
    EgMmlMtextNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( EgMathMlNodeType::MtextNode, document, attribute_map ) {}
};

class EgMmlSpacingNode : public EgMmlNode
{
public:
    EgMmlSpacingNode( const EgMathMlNodeType &node_type, EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlNode( node_type, document, attribute_map ) {}

public:
    qreal width() const;
    qreal height() const;
    qreal depth() const;

protected:
    virtual void layoutSymbol();
    virtual QRectF symbolRect() const;

    qreal interpretSpacing( QString value, qreal base_value, bool *ok ) const;
};

class EgMmlMpaddedNode : public EgMmlSpacingNode
{
public:
    EgMmlMpaddedNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlSpacingNode( EgMathMlNodeType::MpaddedNode, document, attribute_map ) {}

public:
    virtual qreal lspace() const;

protected:
    virtual QRectF symbolRect() const;
};

class EgMmlMspaceNode : public EgMmlSpacingNode
{
public:
    EgMmlMspaceNode( EgMmlDocument *document, const EgMmlAttributeMap &attribute_map )
        : EgMmlSpacingNode( EgMathMlNodeType::MspaceNode, document, attribute_map ) {}
};

static const EgMmlNodeSpec *mmlFindNodeSpec( EgMathMlNodeType type );
static const EgMmlNodeSpec *mmlFindNodeSpec( const QString &tag );
static bool mmlCheckChildType( EgMathMlNodeType parent_type,
                               EgMathMlNodeType child_type, QString *error_str );
static bool mmlCheckAttributes( EgMathMlNodeType child_type,
                                const EgMmlAttributeMap &attr, QString *error_str );
static QString mmlDictAttribute( const QString &name, const EgMmlOperSpec *spec );
static const EgMmlOperSpec *mmlFindOperSpec( const QString &name, EgMml::FormType form );
static qreal mmlInterpretSpacing(QString name, qreal em, qreal ex, bool *ok , const qreal fontsize, const qreal MmToPixFactor);
static qreal mmlInterpretPercentSpacing( QString value, qreal base, bool *ok );
static int mmlInterpretMathVariant( const QString &value, bool *ok );
static EgMml::FormType mmlInterpretForm( const QString &value, bool *ok );
static EgMml::FrameType mmlInterpretFrameType( const QString &value_list, int idx, bool *ok );
static EgMml::FrameSpacing mmlInterpretFrameSpacing(const QString &value_list, qreal em, qreal ex, bool *ok, const qreal fontsize);
static EgMml::ColAlign mmlInterpretColAlign( const QString &value_list, int colnum, bool *ok );
static EgMml::RowAlign mmlInterpretRowAlign( const QString &value_list, int rownum, bool *ok );
static EgMml::FrameType mmlInterpretFrameType( const QString &value_list, int idx, bool *ok );
static QFont mmlInterpretDepreciatedFontAttr(const EgMmlAttributeMap &font_attr, QFont &fn, qreal em, qreal ex , const qreal fontsize);
static QFont mmlInterpretMathSize(const QString &value, QFont &fn, qreal em, qreal ex, bool *ok, const qreal fontsize);
static QString mmlInterpretListAttr( const QString &value_list, int idx, const QString &def );
static qreal mmlQstringToQreal( const QString &string, bool *ok = 0 );

#define MML_ATT_COMMON      " class style id xref actiontype "
#define MML_ATT_FONTSIZE    " fontsize fontweight fontstyle fontfamily color "
#define MML_ATT_MATHVARIANT " mathvariant mathsize mathcolor mathbackground "
#define MML_ATT_FONTINFO    MML_ATT_FONTSIZE MML_ATT_MATHVARIANT
#define MML_ATT_OPINFO      " form fence separator lspace rspace stretchy symmetric " \
    " maxsize minsize largeop movablelimits accent "
#define MML_ATT_SIZEINFO    " width height depth "
#define MML_ATT_TABLEINFO   " align rowalign columnalign columnwidth groupalign " \
    " alignmentscope side rowspacing columnspacing rowlines " \
    " columnlines width frame framespacing equalrows " \
    " equalcolumns displaystyle "
#define MML_ATT_MFRAC       " bevelled numalign denomalign linethickness "
#define MML_ATT_MSTYLE      MML_ATT_FONTINFO MML_ATT_OPINFO \
    " scriptlevel lquote rquote linethickness displaystyle " \
    " scriptsizemultiplier scriptminsize background " \
    " veryverythinmathspace verythinmathspace thinmathspace " \
    " mediummathspace thickmathspace verythickmathspace " \
    " veryverythickmathspace open close separators " \
    " subscriptshift superscriptshift accentunder tableinfo " \
    " rowspan columnspan edge selection bevelled "
#define MML_ATT_MTABLE      " align rowalign columnalign groupalign alignmentscope " \
    " columnwidth width rowspacing columnspacing rowlines columnlines " \
    " frame framespacing equalrows equalcolumns displaystyle side " \
    " minlabelspacing "

static const EgMmlNodeSpec g_node_spec_data[] =
{
//    type                    tag           type_str          child_spec                    child_types              attributes ""=none, 0=any
//    ----------------------- ------------- ----------------- ----------------------------- ------------------------ ---------------------------------------------------------------------
    { EgMathMlNodeType::MiNode,         "mi",         "MiNode",         EgMmlNodeSpec::ChildAny,     " TextNode MalignMark ", MML_ATT_COMMON MML_ATT_FONTINFO                                       },
    { EgMathMlNodeType::MnNode,         "mn",         "MnNode",         EgMmlNodeSpec::ChildAny,     " TextNode MalignMark ", MML_ATT_COMMON MML_ATT_FONTINFO                                       },
    { EgMathMlNodeType::MfracNode,      "mfrac",      "MfracNode",      2,                            0,                       MML_ATT_COMMON MML_ATT_MFRAC                                          },
    { EgMathMlNodeType::MrowNode,       "mrow",       "MrowNode",       EgMmlNodeSpec::ChildAny,     0,                       MML_ATT_COMMON " display mode "                                       },
    { EgMathMlNodeType::MsqrtNode,      "msqrt",      "MsqrtNode",      EgMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON                                                        },
    { EgMathMlNodeType::MrootNode,      "mroot",      "MrootNode",      2,                            0,                       MML_ATT_COMMON                                                        },
    { EgMathMlNodeType::MsupNode,       "msup",       "MsupNode",       2,                            0,                       MML_ATT_COMMON " subscriptshift "                                     },
    { EgMathMlNodeType::MsubNode,       "msub",       "MsubNode",       2,                            0,                       MML_ATT_COMMON " superscriptshift "                                   },
    { EgMathMlNodeType::MsubsupNode,    "msubsup",    "MsubsupNode",    3,                            0,                       MML_ATT_COMMON " subscriptshift superscriptshift "                    },
    { EgMathMlNodeType::MoNode,         "mo",         "MoNode",         EgMmlNodeSpec::ChildAny,     " TextNode MalignMark ", MML_ATT_COMMON MML_ATT_FONTINFO MML_ATT_OPINFO                        },
    { EgMathMlNodeType::MstyleNode,     "mstyle",     "MstyleNode",     EgMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON MML_ATT_MSTYLE                                         },
    { EgMathMlNodeType::MphantomNode,   "mphantom",   "MphantomNode",   EgMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON                                                        },
    { EgMathMlNodeType::MalignMarkNode, "malignmark", "MalignMarkNode", 0,                            0,                       ""                                                                    },
    { EgMathMlNodeType::MfencedNode,    "mfenced",    "MfencedNode",    EgMmlNodeSpec::ChildAny,     0,                       MML_ATT_COMMON " open close separators "                              },
    { EgMathMlNodeType::MtableNode,     "mtable",     "MtableNode",     EgMmlNodeSpec::ChildAny,     " MtrNode ",             MML_ATT_COMMON MML_ATT_MTABLE                                         },
    { EgMathMlNodeType::MtrNode,        "mtr",        "MtrNode",        EgMmlNodeSpec::ChildAny,     " MtdNode ",             MML_ATT_COMMON " rowalign columnalign groupalign "                    },
    { EgMathMlNodeType::MtdNode,        "mtd",        "MtdNode",        EgMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON " rowspan columnspan rowalign columnalign groupalign " },
    { EgMathMlNodeType::MoverNode,      "mover",      "MoverNode",      2,                            0,                       MML_ATT_COMMON " accent align "                                       },
    { EgMathMlNodeType::MunderNode,     "munder",     "MunderNode",     2,                            0,                       MML_ATT_COMMON " accentunder align "                                  },
    { EgMathMlNodeType::MunderoverNode, "munderover", "MunderoverNode", 3,                            0,                       MML_ATT_COMMON " accentunder accent align "                           },
    { EgMathMlNodeType::MerrorNode,     "merror",     "MerrorNode",     EgMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON                                                        },
    { EgMathMlNodeType::MtextNode,      "mtext",      "MtextNode",      1,                            " TextNode ",            MML_ATT_COMMON " width height depth linebreak "                       },
    { EgMathMlNodeType::MpaddedNode,    "mpadded",    "MpaddedNode",    EgMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON " width height depth lspace "                          },
    { EgMathMlNodeType::MspaceNode,     "mspace",     "MspaceNode",     EgMmlNodeSpec::ImplicitMrow, 0,                       MML_ATT_COMMON " width height depth linebreak "                       },
    { EgMathMlNodeType::TextNode,       0,            "TextNode",       EgMmlNodeSpec::ChildIgnore,  0,                       ""                                                                    },
    { EgMathMlNodeType::UnknownNode,    0,            "UnknownNode",    EgMmlNodeSpec::ChildAny,     0,                       0                                                                     },
    { EgMathMlNodeType::NoNode,         0,            0,                0,                            0,                       0                                                                     }
};

static const char *g_oper_spec_names[g_oper_spec_rows] =
{
    "accent", "fence", "largeop", "lspace", "minsize", "movablelimits",
    "rspace", "separator", "stretchy" /* stretchdir */
};

static const EgMmlOperSpec g_oper_spec_data[] =
{
//                                                                accent   fence    largeop  lspace               minsize movablelimits rspace                   separator stretchy
//                                                                -------- -------- -------- -------------------- ------- ------------- ------------------------ --------- --------
    { "!!",                                EgMml::PostfixForm, { 0,       0,       0,       "verythinmathspace", 0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "!!"
    { "!",                                 EgMml::PostfixForm, { 0,       0,       0,       "verythinmathspace", 0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "!"
    { "!=",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "!="
    { "&And;",                             EgMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&And;"
    { "&ApplyFunction;",                   EgMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&ApplyFunction;"
    { "&Assign;",                          EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Assign;"
    { "&Backslash;",                       EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&Backslash;"
    { "&Because;",                         EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Because;"
    { "&Breve;",                           EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&Breve;"
    { "&Cap;",                             EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Cap;"
    { "&CapitalDifferentialD;",            EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&CapitalDifferentialD;"
    { "&Cedilla;",                         EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&Cedilla;"
    { "&CenterDot;",                       EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&CenterDot;"
    { "&CircleDot;",                       EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&CircleDot;"
    { "&CircleMinus;",                     EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&CircleMinus;"
    { "&CirclePlus;",                      EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&CirclePlus;"
    { "&CircleTimes;",                     EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&CircleTimes;"
    { "&ClockwiseContourIntegral;",        EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&ClockwiseContourIntegral;"
    { "&CloseCurlyDoubleQuote;",           EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&CloseCurlyDoubleQuote;"
    { "&CloseCurlyQuote;",                 EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&CloseCurlyQuote;"
    { "&Colon;",                           EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Colon;"
    { "&Congruent;",                       EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Congruent;"
    { "&ContourIntegral;",                 EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&ContourIntegral;"
    { "&Coproduct;",                       EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Coproduct;"
    { "&CounterClockwiseContourIntegral;", EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&CounterClockwiseContourInteg
    { "&Cross;",                           EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&Cross;"
    { "&Cup;",                             EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Cup;"
    { "&CupCap;",                          EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&CupCap;"
    { "&Del;",                             EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&Del;"
    { "&DiacriticalAcute;",                EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&DiacriticalAcute;"
    { "&DiacriticalDot;",                  EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&DiacriticalDot;"
    { "&DiacriticalDoubleAcute;",          EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&DiacriticalDoubleAcute;"
    { "&DiacriticalGrave;",                EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&DiacriticalGrave;"
    { "&DiacriticalLeftArrow;",            EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DiacriticalLeftArrow;"
    { "&DiacriticalLeftRightArrow;",       EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DiacriticalLeftRightArrow;"
    { "&DiacriticalLeftRightVector;",      EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DiacriticalLeftRightVector;"
    { "&DiacriticalLeftVector;",           EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DiacriticalLeftVector;"
    { "&DiacriticalRightArrow;",           EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DiacriticalRightArrow;"
    { "&DiacriticalRightVector;",          EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DiacriticalRightVector;"
    { "&DiacriticalTilde;",                EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::NoStretch }, // "&DiacriticalTilde;"
    { "&Diamond;",                         EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Diamond;"
    { "&DifferentialD;",                   EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&DifferentialD;"
    { "&DotEqual;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&DotEqual;"
    { "&DoubleContourIntegral;",           EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&DoubleContourIntegral;"
    { "&DoubleDot;",                       EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&DoubleDot;"
    { "&DoubleDownArrow;",                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&DoubleDownArrow;"
    { "&DoubleLeftArrow;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DoubleLeftArrow;"
    { "&DoubleLeftRightArrow;",            EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DoubleLeftRightArrow;"
    { "&DoubleLeftTee;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&DoubleLeftTee;"
    { "&DoubleLongLeftArrow;",             EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DoubleLongLeftArrow;"
    { "&DoubleLongLeftRightArrow;",        EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DoubleLongLeftRightArrow;"
    { "&DoubleLongRightArrow;",            EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DoubleLongRightArrow;"
    { "&DoubleRightArrow;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&DoubleRightArrow;"
    { "&DoubleRightTee;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&DoubleRightTee;"
    { "&DoubleUpArrow;",                   EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&DoubleUpArrow;"
    { "&DoubleUpDownArrow;",               EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&DoubleUpDownArrow;"
    { "&DoubleVerticalBar;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&DoubleVerticalBar;"
    { "&DownArrow;",                       EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&DownArrow;"
    { "&DownArrowBar;",                    EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&DownArrowBar;"
    { "&DownArrowUpArrow;",                EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&DownArrowUpArrow;"
    { "&DownBreve;",                       EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&DownBreve;"
    { "&DownLeftRightVector;",             EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&DownLeftRightVector;"
    { "&DownLeftTeeVector;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&DownLeftTeeVector;"
    { "&DownLeftVector;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&DownLeftVector;"
    { "&DownLeftVectorBar;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&DownLeftVectorBar;"
    { "&DownRightTeeVector;",              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&DownRightTeeVector;"
    { "&DownRightVector;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&DownRightVector;"
    { "&DownRightVectorBar;",              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&DownRightVectorBar;"
    { "&DownTee;",                         EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&DownTee;"
    { "&DownTeeArrow;",                    EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&DownTeeArrow;"
    { "&Element;",                         EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Element;"
    { "&Equal;",                           EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Equal;"
    { "&EqualTilde;",                      EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&EqualTilde;"
    { "&Equilibrium;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&Equilibrium;"
    { "&Exists;",                          EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Exists;"
    { "&ForAll;",                          EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&ForAll;"
    { "&GreaterEqual;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&GreaterEqual;"
    { "&GreaterEqualLess;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&GreaterEqualLess;"
    { "&GreaterFullEqual;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&GreaterFullEqual;"
    { "&GreaterGreater;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&GreaterGreater;"
    { "&GreaterLess;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&GreaterLess;"
    { "&GreaterSlantEqual;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&GreaterSlantEqual;"
    { "&GreaterTilde;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&GreaterTilde;"
    { "&Hacek;",                           EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::NoStretch }, // "&Hacek;"
    { "&Hat;",                             EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&Hat;"
    { "&HorizontalLine;",                  EgMml::InfixForm,   { 0,       0,       0,       "0em",               "0",    0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&HorizontalLine;"
    { "&HumpDownHump;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&HumpDownHump;"
    { "&HumpEqual;",                       EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&HumpEqual;"
    { "&Implies;",                         EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&Implies;"
    { "&Integral;",                        EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&Integral;"
    { "&Intersection;",                    EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&Intersection;"
    { "&InvisibleComma;",                  EgMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   "true",   0        }, EgMmlOperSpec::NoStretch }, // "&InvisibleComma;"
    { "&InvisibleTimes;",                  EgMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&InvisibleTimes;"
    { "&LeftAngleBracket;",                EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&LeftAngleBracket;"
    { "&LeftArrow;",                       EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftArrow;"
    { "&LeftArrowBar;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftArrowBar;"
    { "&LeftArrowRightArrow;",             EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftArrowRightArrow;"
    { "&LeftBracketingBar;",               EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&LeftBracketingBar;"
    { "&LeftCeiling;",                     EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&LeftCeiling;"
    { "&LeftDoubleBracket;",               EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftDoubleBracket;"
    { "&LeftDoubleBracketingBar;",         EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftDoubleBracketingBar;"
    { "&LeftDownTeeVector;",               EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LeftDownTeeVector;"
    { "&LeftDownVector;",                  EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LeftDownVector;"
    { "&LeftDownVectorBar;",               EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LeftDownVectorBar;"
    { "&LeftFloor;",                       EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&LeftFloor;"
    { "&LeftRightArrow;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftRightArrow;"
    { "&LeftRightVector;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftRightVector;"
    { "&LeftSkeleton;",                    EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&LeftSkeleton;"
    { "&LeftTee;",                         EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LeftTee;"
    { "&LeftTeeArrow;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftTeeArrow;"
    { "&LeftTeeVector;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftTeeVector;"
    { "&LeftTriangle;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LeftTriangle;"
    { "&LeftTriangleBar;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LeftTriangleBar;"
    { "&LeftTriangleEqual;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LeftTriangleEqual;"
    { "&LeftUpDownVector;",                EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LeftUpDownVector;"
    { "&LeftUpTeeVector;",                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LeftUpTeeVector;"
    { "&LeftUpVector;",                    EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LeftUpVector;"
    { "&LeftUpVectorBar;",                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LeftUpVectorBar;"
    { "&LeftVector;",                      EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LeftVector;"
    { "&LeftVectorBar;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&LeftVectorBar;"
    { "&LessEqualGreater;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LessEqualGreater;"
    { "&LessFullEqual;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LessFullEqual;"
    { "&LessGreater;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LessGreater;"
    { "&LessLess;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LessLess;"
    { "&LessSlantEqual;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LessSlantEqual;"
    { "&LessTilde;",                       EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&LessTilde;"
    { "&LongLeftArrow;",                   EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LongLeftArrow;"
    { "&LongLeftRightArrow;",              EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LongLeftRightArrow;"
    { "&LongRightArrow;",                  EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&LongRightArrow;"
    { "&LowerLeftArrow;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LowerLeftArrow;"
    { "&LowerRightArrow;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&LowerRightArrow;"
    { "&MinusPlus;",                       EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "veryverythinmathspace", 0,        0        }, EgMmlOperSpec::NoStretch }, // "&MinusPlus;"
    { "&NestedGreaterGreater;",            EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NestedGreaterGreater;"
    { "&NestedLessLess;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NestedLessLess;"
    { "&Not;",                             EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Not;"
    { "&NotCongruent;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotCongruent;"
    { "&NotCupCap;",                       EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotCupCap;"
    { "&NotDoubleVerticalBar;",            EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotDoubleVerticalBar;"
    { "&NotElement;",                      EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotElement;"
    { "&NotEqual;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotEqual;"
    { "&NotEqualTilde;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotEqualTilde;"
    { "&NotExists;",                       EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotExists;"
    { "&NotGreater;",                      EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotGreater;"
    { "&NotGreaterEqual;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotGreaterEqual;"
    { "&NotGreaterFullEqual;",             EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotGreaterFullEqual;"
    { "&NotGreaterGreater;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotGreaterGreater;"
    { "&NotGreaterLess;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotGreaterLess;"
    { "&NotGreaterSlantEqual;",            EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotGreaterSlantEqual;"
    { "&NotGreaterTilde;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotGreaterTilde;"
    { "&NotHumpDownHump;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotHumpDownHump;"
    { "&NotHumpEqual;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotHumpEqual;"
    { "&NotLeftTriangle;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLeftTriangle;"
    { "&NotLeftTriangleBar;",              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLeftTriangleBar;"
    { "&NotLeftTriangleEqual;",            EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLeftTriangleEqual;"
    { "&NotLess;",                         EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLess;"
    { "&NotLessEqual;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLessEqual;"
    { "&NotLessFullEqual;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLessFullEqual;"
    { "&NotLessGreater;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLessGreater;"
    { "&NotLessLess;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLessLess;"
    { "&NotLessSlantEqual;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLessSlantEqual;"
    { "&NotLessTilde;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotLessTilde;"
    { "&NotNestedGreaterGreater;",         EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotNestedGreaterGreater;"
    { "&NotNestedLessLess;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotNestedLessLess;"
    { "&NotPrecedes;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotPrecedes;"
    { "&NotPrecedesEqual;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotPrecedesEqual;"
    { "&NotPrecedesSlantEqual;",           EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotPrecedesSlantEqual;"
    { "&NotPrecedesTilde;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotPrecedesTilde;"
    { "&NotReverseElement;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotReverseElement;"
    { "&NotRightTriangle;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotRightTriangle;"
    { "&NotRightTriangleBar;",             EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotRightTriangleBar;"
    { "&NotRightTriangleEqual;",           EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotRightTriangleEqual;"
    { "&NotSquareSubset;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSquareSubset;"
    { "&NotSquareSubsetEqual;",            EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSquareSubsetEqual;"
    { "&NotSquareSuperset;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSquareSuperset;"
    { "&NotSquareSupersetEqual;",          EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSquareSupersetEqual;"
    { "&NotSubset;",                       EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSubset;"
    { "&NotSubsetEqual;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSubsetEqual;"
    { "&NotSucceeds;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSucceeds;"
    { "&NotSucceedsEqual;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSucceedsEqual;"
    { "&NotSucceedsSlantEqual;",           EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSucceedsSlantEqual;"
    { "&NotSucceedsTilde;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSucceedsTilde;"
    { "&NotSuperset;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSuperset;"
    { "&NotSupersetEqual;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotSupersetEqual;"
    { "&NotTilde;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotTilde;"
    { "&NotTildeEqual;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotTildeEqual;"
    { "&NotTildeFullEqual;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotTildeFullEqual;"
    { "&NotTildeTilde;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotTildeTilde;"
    { "&NotVerticalBar;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&NotVerticalBar;"
    { "&OpenCurlyDoubleQuote;",            EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&OpenCurlyDoubleQuote;"
    { "&OpenCurlyQuote;",                  EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&OpenCurlyQuote;"
    { "&Or;",                              EgMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&Or;"
    { "&OverBar;",                         EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&OverBar;"
    { "&OverBrace;",                       EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&OverBrace;"
    { "&OverBracket;",                     EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&OverBracket;"
    { "&OverParenthesis;",                 EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&OverParenthesis;"
    { "&PartialD;",                        EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&PartialD;"
    { "&PlusMinus;",                       EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "veryverythinmathspace", 0,        0        }, EgMmlOperSpec::NoStretch }, // "&PlusMinus;"
    { "&Precedes;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Precedes;"
    { "&PrecedesEqual;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&PrecedesEqual;"
    { "&PrecedesSlantEqual;",              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&PrecedesSlantEqual;"
    { "&PrecedesTilde;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&PrecedesTilde;"
    { "&Product;",                         EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Product;"
    { "&Proportion;",                      EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Proportion;"
    { "&Proportional;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Proportional;"
    { "&ReverseElement;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&ReverseElement;"
    { "&ReverseEquilibrium;",              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&ReverseEquilibrium;"
    { "&ReverseUpEquilibrium;",            EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&ReverseUpEquilibrium;"
    { "&RightAngleBracket;",               EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&RightAngleBracket;"
    { "&RightArrow;",                      EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&RightArrow;"
    { "&RightArrowBar;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&RightArrowBar;"
    { "&RightArrowLeftArrow;",             EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&RightArrowLeftArrow;"
    { "&RightBracketingBar;",              EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&RightBracketingBar;"
    { "&RightCeiling;",                    EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&RightCeiling;"
    { "&RightDoubleBracket;",              EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&RightDoubleBracket;"
    { "&RightDoubleBracketingBar;",        EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&RightDoubleBracketingBar;"
    { "&RightDownTeeVector;",              EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&RightDownTeeVector;"
    { "&RightDownVector;",                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&RightDownVector;"
    { "&RightDownVectorBar;",              EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&RightDownVectorBar;"
    { "&RightFloor;",                      EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&RightFloor;"
    { "&RightSkeleton;",                   EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&RightSkeleton;"
    { "&RightTee;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&RightTee;"
    { "&RightTeeArrow;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&RightTeeArrow;"
    { "&RightTeeVector;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&RightTeeVector;"
    { "&RightTriangle;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&RightTriangle;"
    { "&RightTriangleBar;",                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&RightTriangleBar;"
    { "&RightTriangleEqual;",              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&RightTriangleEqual;"
    { "&RightUpDownVector;",               EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&RightUpDownVector;"
    { "&RightUpTeeVector;",                EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&RightUpTeeVector;"
    { "&RightUpVector;",                   EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&RightUpVector;"
    { "&RightUpVectorBar;",                EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&RightUpVectorBar;"
    { "&RightVector;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&RightVector;"
    { "&RightVectorBar;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&RightVectorBar;"
    { "&RoundImplies;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&RoundImplies;"
    { "&ShortDownArrow;",                  EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&ShortDownArrow;"
    { "&ShortLeftArrow;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::HStretch  }, // "&ShortLeftArrow;"
    { "&ShortRightArrow;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::HStretch  }, // "&ShortRightArrow;"
    { "&ShortUpArrow;",                    EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::VStretch  }, // "&ShortUpArrow;"
    { "&SmallCircle;",                     EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&SmallCircle;"
    { "&Sqrt;",                            EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&Sqrt;"
    { "&Square;",                          EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&Square;"
    { "&SquareIntersection;",              EgMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&SquareIntersection;"
    { "&SquareSubset;",                    EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SquareSubset;"
    { "&SquareSubsetEqual;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SquareSubsetEqual;"
    { "&SquareSuperset;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SquareSuperset;"
    { "&SquareSupersetEqual;",             EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SquareSupersetEqual;"
    { "&SquareUnion;",                     EgMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&SquareUnion;"
    { "&Star;",                            EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Star;"
    { "&Subset;",                          EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Subset;"
    { "&SubsetEqual;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SubsetEqual;"
    { "&Succeeds;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Succeeds;"
    { "&SucceedsEqual;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SucceedsEqual;"
    { "&SucceedsSlantEqual;",              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SucceedsSlantEqual;"
    { "&SucceedsTilde;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SucceedsTilde;"
    { "&SuchThat;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SuchThat;"
    { "&Sum;",                             EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Sum;"
    { "&Superset;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Superset;"
    { "&SupersetEqual;",                   EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&SupersetEqual;"
    { "&Therefore;",                       EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Therefore;"
    { "&Tilde;",                           EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&Tilde;"
    { "&TildeEqual;",                      EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&TildeEqual;"
    { "&TildeFullEqual;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&TildeFullEqual;"
    { "&TildeTilde;",                      EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&TildeTilde;"
    { "&TripleDot;",                       EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "&TripleDot;"
    { "&UnderBar;",                        EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&UnderBar;"
    { "&UnderBrace;",                      EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&UnderBrace;"
    { "&UnderBracket;",                    EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&UnderBracket;"
    { "&UnderParenthesis;",                EgMml::PostfixForm, { "true",  0,       0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::HStretch  }, // "&UnderParenthesis;"
    { "&Union;",                           EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&Union;"
    { "&UnionPlus;",                       EgMml::PrefixForm,  { 0,       0,       "true",  "0em",               0,      "true",       "thinmathspace",         0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&UnionPlus;"
    { "&UpArrow;",                         EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&UpArrow;"
    { "&UpArrowBar;",                      EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&UpArrowBar;"
    { "&UpArrowDownArrow;",                EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&UpArrowDownArrow;"
    { "&UpDownArrow;",                     EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&UpDownArrow;"
    { "&UpEquilibrium;",                   EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&UpEquilibrium;"
    { "&UpTee;",                           EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&UpTee;"
    { "&UpTeeArrow;",                      EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&UpTeeArrow;"
    { "&UpperLeftArrow;",                  EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&UpperLeftArrow;"
    { "&UpperRightArrow;",                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::HVStretch }, // "&UpperRightArrow;"
    { "&Vee;",                             EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Vee;"
    { "&VerticalBar;",                     EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&VerticalBar;"
    { "&VerticalLine;",                    EgMml::InfixForm,   { 0,       0,       0,       "0em",               "0",    0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&VerticalLine;"
    { "&VerticalSeparator;",               EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::VStretch  }, // "&VerticalSeparator;"
    { "&VerticalTilde;",                   EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&VerticalTilde;"
    { "&Wedge;",                           EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "&Wedge;"
    { "&amp;",                             EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&amp;"
    { "&amp;&amp;",                        EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&amp;&amp;"
    { "&le;",                              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&le;"
    { "&lt;",                              EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&lt;"
    { "&lt;=",                             EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "&lt;="
    { "&lt;>",                             EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "&lt;>"
    { "'",                                 EgMml::PostfixForm, { 0,       0,       0,       "verythinmathspace", 0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "'"
    { "(",                                 EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "("
    { ")",                                 EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // ")"
    { "*",                                 EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "*"
    { "**",                                EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "**"
    { "*=",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "*="
    { "+",                                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "+"
    { "+",                                 EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "veryverythinmathspace", 0,        0        }, EgMmlOperSpec::NoStretch }, // "+"
    { "++",                                EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "++"
    { "+=",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "+="
    { ",",                                 EgMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "verythickmathspace",    "true",   0        }, EgMmlOperSpec::NoStretch }, // ","
    { "-",                                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "-"
    { "-",                                 EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "veryverythinmathspace", 0,        0        }, EgMmlOperSpec::NoStretch }, // "-"
    { "--",                                EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "--"
    { "-=",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "-="
    { "->",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "->"
    { ".",                                 EgMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "."
    { "..",                                EgMml::PostfixForm, { 0,       0,       0,       "mediummathspace",   0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // ".."
    { "...",                               EgMml::PostfixForm, { 0,       0,       0,       "mediummathspace",   0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // "..."
    { "/",                                 EgMml::InfixForm,   { 0,       0,       0,       "thinmathspace",     0,      0,            "thinmathspace",         0,        "true"   }, EgMmlOperSpec::VStretch  }, // "/"
    { "//",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "//"
    { "/=",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "/="
    { ":",                                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // ":"
    { ":=",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // ":="
    { ";",                                 EgMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "verythickmathspace",    "true",   0        }, EgMmlOperSpec::NoStretch }, // ";"
    { ";",                                 EgMml::PostfixForm, { 0,       0,       0,       "0em",               0,      0,            "0em",                   "true",   0        }, EgMmlOperSpec::NoStretch }, // ";"
    { "=",                                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "="
    { "==",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // "=="
    { ">",                                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // ">"
    { ">=",                                EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        0        }, EgMmlOperSpec::NoStretch }, // ">="
    { "?",                                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "?"
    { "@",                                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "@"
    { "[",                                 EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "["
    { "]",                                 EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "]"
    { "^",                                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "^"
    { "_",                                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "_"
    { "lim",                               EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      "true",       "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "lim"
    { "max",                               EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      "true",       "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "max"
    { "min",                               EgMml::PrefixForm,  { 0,       0,       0,       "0em",               0,      "true",       "thinmathspace",         0,        0        }, EgMmlOperSpec::NoStretch }, // "min"
    { "{",                                 EgMml::PrefixForm,  { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "{"
    { "|",                                 EgMml::InfixForm,   { 0,       0,       0,       "thickmathspace",    0,      0,            "thickmathspace",        0,        "true"   }, EgMmlOperSpec::VStretch  }, // "|"
    { "||",                                EgMml::InfixForm,   { 0,       0,       0,       "mediummathspace",   0,      0,            "mediummathspace",       0,        0        }, EgMmlOperSpec::NoStretch }, // "||"
    { "}",                                 EgMml::PostfixForm, { 0,       "true",  0,       "0em",               0,      0,            "0em",                   0,        "true"   }, EgMmlOperSpec::VStretch  }, // "}"
    { "~",                                 EgMml::InfixForm,   { 0,       0,       0,       "verythinmathspace", 0,      0,            "verythinmathspace",     0,        0        }, EgMmlOperSpec::NoStretch }, // "~"
#if 1
    { QString( QChar( 0x64, 0x20 ) ),      EgMml::InfixForm,   { 0,       0,       0,       "0em",               0,      0,            "0em",                   0,        0        }, EgMmlOperSpec::NoStretch }, // Invisible addition
#endif
    { 0,                                   EgMml::InfixForm,   { 0,       0,       0,       0,                   0,      0,            0,                       0,        0        }, EgMmlOperSpec::NoStretch }
};

static const EgMmlOperSpec g_oper_spec_defaults =
{
    0,  EgMml::InfixForm, { "false", "false", "false", "thickmathspace", "1", "false", "thickmathspace", "false",  "false" }, EgMmlOperSpec::NoStretch
};

static const int g_oper_spec_count = sizeof( g_oper_spec_data ) / sizeof( EgMmlOperSpec ) - 1;

// *******************************************************************
// EgMmlDocument
// *******************************************************************

QString EgMmlDocument::fontName( EgMathMLDocument::MmlFont type ) const
{
    switch ( type )
    {
        case EgMathMLDocument::NormalFont:
            return s_normal_font_name;
        case EgMathMLDocument::FrakturFont:
            return s_fraktur_font_name;
        case EgMathMLDocument::SansSerifFont:
            return s_sans_serif_font_name;
        case EgMathMLDocument::ScriptFont:
            return s_script_font_name;
        case EgMathMLDocument::MonospaceFont:
            return s_monospace_font_name;
        case EgMathMLDocument::DoublestruckFont:
            return s_doublestruck_font_name;
    };

    return QString::null;
}

void EgMmlDocument::setFontName( EgMathMLDocument::MmlFont type,
                                  const QString &name )
{
    //clear rendering since using a new font will break rendering positions
    clearRendering();

    switch ( type )
    {
        case EgMathMLDocument::NormalFont:
            s_normal_font_name = name;
            break;
        case EgMathMLDocument::FrakturFont:
            s_fraktur_font_name = name;
            break;
        case EgMathMLDocument::SansSerifFont:
            s_sans_serif_font_name = name;
            break;
        case EgMathMLDocument::ScriptFont:
            s_script_font_name = name;
            break;
        case EgMathMLDocument::MonospaceFont:
            s_monospace_font_name = name;
            break;
        case EgMathMLDocument::DoublestruckFont:
            s_doublestruck_font_name = name;
            break;
    };
}

EgMathMlNodeType domToEgMmlNodeType( const QDomNode &dom_node )
{
    EgMathMlNodeType mml_type = EgMathMlNodeType::NoNode;

    switch ( dom_node.nodeType() )
    {
        case QDomNode::ElementNode:
        {
            QString tag = dom_node.nodeName();
            const EgMmlNodeSpec *spec = mmlFindNodeSpec( tag );

            // treat urecognised tags as mrow
            if ( spec == 0 )
                mml_type = EgMathMlNodeType::UnknownNode;
            else
                mml_type = spec->type;

            break;
        }
        case QDomNode::TextNode:
            mml_type = EgMathMlNodeType::TextNode;
            break;

        case QDomNode::DocumentNode:
            mml_type = EgMathMlNodeType::UnknownNode;
            break;

        case QDomNode::EntityReferenceNode:
#if 0
            qWarning() << "EntityReferenceNode: name=\"" + dom_node.nodeName() + "\" value=\"" + dom_node.nodeValue() + "\"";
#endif
            break;

        case QDomNode::AttributeNode:
        case QDomNode::CDATASectionNode:
        case QDomNode::EntityNode:
        case QDomNode::ProcessingInstructionNode:
        case QDomNode::CommentNode:
        case QDomNode::DocumentTypeNode:
        case QDomNode::DocumentFragmentNode:
        case QDomNode::NotationNode:
        case QDomNode::BaseNode:
        case QDomNode::CharacterDataNode:
            break;
    }

    return mml_type;
}

EgMmlDocument::EgMmlDocument(EgMathMLDocument* egMathMLDocument) : m_base_font_pixel_size(15)
{
        m_EgMathMLDocument = egMathMLDocument;
        init();
}

EgMmlDocument::EgMmlDocument() : m_base_font_pixel_size(15), m_EgMathMLDocument(NULL)
{
        init();
}

void EgMmlDocument::init()
{
    m_root_node = 0;
    m_foreground_color = Qt::black;
    m_background_color = Qt::transparent;
    m_draw_frames = false;
    clearRendering();

    if (!s_initialized)
            static_init();
}

void EgMmlDocument::static_init(void)
{
        // We set s_normal_font_name based on the information available at
        // https://vismor.com/documents/site_implementation/viewing_mathematics/S7.php
        // Note: on Linux, the Ubuntu, DejaVu Serif, FreeSerif and Liberation Serif
        //       either don't look that great or have rendering problems (e.g.
        //       FreeSerif doesn't render 0 properly!), so we simply use Century
        //       Schoolbook L...

        QFontDatabase font_database;

    #if defined( Q_OS_WIN )
        if ( font_database.hasFamily( "Cambria" ) )
            s_normal_font_name = "Cambria";
        else if ( font_database.hasFamily( "Lucida Sans Unicode" ) )
            s_normal_font_name = "Lucida Sans Unicode";
        else
            s_normal_font_name = "Times New Roman";
    #elif defined( Q_OS_LINUX )
        s_normal_font_name = "Century Schoolbook L";
    #elif defined( Q_OS_MAC )
        if ( font_database.hasFamily( "STIXGeneral" ) )
            s_normal_font_name = "STIXGeneral";
        else
            s_normal_font_name = "Times New Roman";
    #else
        s_normal_font_name = "Times New Roman";
    #endif

        s_initialized = true;
}

EgMmlDocument::~EgMmlDocument()
{
    clear();
}

void EgMmlDocument::clear()
{
    delete m_root_node;
    m_root_node = 0;
    clearRendering();
}

void EgMmlDocument::dump() const
{
    if ( m_root_node == 0 )
        return;

    QString indent;
    _dump( m_root_node, indent );
}

void EgMmlDocument::_dump(
    const EgMmlNode *node, const QString &indent ) const
{
    if ( node == 0 ) return;

    qWarning() << indent + node->toStr();

    const EgMmlNode *child = node->firstChild();
    for ( ; child != 0; child = child->nextSibling() )
        _dump( child, indent + "  " );
}

bool EgMmlDocument::setContent(
    const QString &text, QString *errorMsg, int *errorLine, int *errorColumn )
{
    clear();

    uint prefix_lines;
    QString prefix = mmlEntityTable.entities(text, prefix_lines);

    QDomDocument dom;
    if (!dom.setContent(prefix + text, false, errorMsg, errorLine, errorColumn)) {
        if (errorLine != 0)
            *errorLine -= prefix_lines;
        return false;
    }

    // we don't have access to line info from now on
    if ( errorLine != 0 ) *errorLine = -1;
    if ( errorColumn != 0 ) *errorColumn = -1;

    bool ok;
    EgMmlNode *root_node = domToMml( dom, &ok, errorMsg );
    if ( !ok )
        return false;

    if ( root_node == 0 )
    {
        if ( errorMsg != 0 )
            *errorMsg = "empty document";
        return false;
    }

    insertChild( 0, root_node, 0 );
    layout();

    return true;
}

void EgMmlDocument::layout()
{
    if ( m_root_node == 0 )
        return;

    m_root_node->layout();
    m_root_node->stretch();
}

bool EgMmlDocument::insertChild( EgMmlNode *parent, EgMmlNode *new_node,
                                  QString *errorMsg )
{
    if ( new_node == 0 )
        return true;

    Q_ASSERT( new_node->parent() == 0
              && new_node->nextSibling() == 0
              && new_node->previousSibling() == 0 );

    if ( parent != 0 )
    {
        if ( !mmlCheckChildType( parent->nodeType(), new_node->nodeType(), errorMsg ) )
            return false;
    }

    if ( parent == 0 )
    {
        if ( m_root_node == 0 )
        {
            m_root_node = new_node;
        }
        else
        {
            EgMmlNode *n = m_root_node->lastSibling();
            n->m_next_sibling = new_node;
            new_node->m_previous_sibling = n;
        }
    }
    else
    {
        new_node->m_parent = parent;
        if ( parent->hasChildNodes() )
        {
            EgMmlNode *n = parent->firstChild()->lastSibling();
            n->m_next_sibling = new_node;
            new_node->m_previous_sibling = n;
        }
        else
        {
            parent->m_first_child = new_node;
        }
    }

    return true;
}

EgMmlNode *EgMmlDocument::createNode( EgMathMlNodeType type,
                                        const EgMmlAttributeMap &mml_attr,
                                        const QString &mml_value,
                                        QString *errorMsg )
{
    Q_ASSERT( type != EgMathMlNodeType::NoNode );

    EgMmlNode *mml_node = 0;

    if ( !mmlCheckAttributes( type, mml_attr, errorMsg ) )
        return 0;

    switch ( type )
    {
        case EgMathMlNodeType::MiNode:
            mml_node = new EgMmlMiNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MnNode:
            mml_node = new EgMmlMnNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MfracNode:
            mml_node = new EgMmlMfracNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MrowNode:
            mml_node = new EgMmlMrowNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MsqrtNode:
            mml_node = new EgMmlMsqrtNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MrootNode:
            mml_node = new EgMmlMrootNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MsupNode:
            mml_node = new EgMmlMsupNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MsubNode:
            mml_node = new EgMmlMsubNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MsubsupNode:
            mml_node = new EgMmlMsubsupNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MoNode:
            mml_node = new EgMmlMoNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MstyleNode:
            mml_node = new EgMmlMstyleNode( this, mml_attr );
            break;
        case EgMathMlNodeType::TextNode:
            mml_node = new EgMmlTextNode( mml_value, this );
            break;
        case EgMathMlNodeType::MphantomNode:
            mml_node = new EgMmlMphantomNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MfencedNode:
            mml_node = new EgMmlMfencedNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MtableNode:
            mml_node = new EgMmlMtableNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MtrNode:
            mml_node = new EgMmlMtrNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MtdNode:
            mml_node = new EgMmlMtdNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MoverNode:
            mml_node = new EgMmlMoverNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MunderNode:
            mml_node = new EgMmlMunderNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MunderoverNode:
            mml_node = new EgMmlMunderoverNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MalignMarkNode:
            mml_node = new EgMmlMalignMarkNode( this );
            break;
        case EgMathMlNodeType::MerrorNode:
            mml_node = new EgMmlMerrorNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MtextNode:
            mml_node = new EgMmlMtextNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MpaddedNode:
            mml_node = new EgMmlMpaddedNode( this, mml_attr );
            break;
        case EgMathMlNodeType::MspaceNode:
            mml_node = new EgMmlMspaceNode( this, mml_attr );
            break;
        case EgMathMlNodeType::UnknownNode:
            mml_node = new EgMmlUnknownNode( this, mml_attr );
            break;
        case EgMathMlNodeType::NoNode:
            mml_node = 0;
            break;
    }

    if (mml_node) {
            if (mml_attr.contains("id"))
                    mml_node->m_nodeId = mml_attr.value("id").toUInt();
    }

    return mml_node;
}

void EgMmlDocument::insertOperator( EgMmlNode *node, const QString &text )
{
    EgMmlNode *text_node = createNode( EgMathMlNodeType::TextNode, EgMmlAttributeMap(), text, 0 );
    EgMmlNode *mo_node = createNode( EgMathMlNodeType::MoNode, EgMmlAttributeMap(), QString::null, 0 );

    bool ok = insertChild( node, mo_node, 0 );
    Q_ASSERT( ok );
    ok = insertChild( mo_node, text_node, 0 );
    Q_ASSERT( ok );
    Q_UNUSED( ok );
}

EgMmlNode *EgMmlDocument::domToMml( const QDomNode &dom_node, bool *ok,
                                      QString *errorMsg )
{
    // create the node

    Q_ASSERT( ok != 0 );

    EgMathMlNodeType mml_type = domToEgMmlNodeType( dom_node );

    if ( mml_type == EgMathMlNodeType::NoNode )
    {
        *ok = true;
        return 0;
    }

    QDomNamedNodeMap dom_attr = dom_node.attributes();
    EgMmlAttributeMap mml_attr;
    for ( int i = 0; i < dom_attr.length(); ++i )
    {
        QDomNode attr_node = dom_attr.item( i );
        Q_ASSERT( !attr_node.nodeName().isNull() );
        Q_ASSERT( !attr_node.nodeValue().isNull() );
        mml_attr[attr_node.nodeName()] = attr_node.nodeValue();
    }

    QString mml_value;
    if ( mml_type == EgMathMlNodeType::TextNode )
        mml_value = dom_node.nodeValue();
    EgMmlNode *mml_node = createNode( mml_type, mml_attr, mml_value, errorMsg );
    if ( mml_node == 0 )
    {
        *ok = false;
        return 0;
    }

    // create the node's children according to the child_spec

    const EgMmlNodeSpec *spec = mmlFindNodeSpec( mml_type );
    QDomNodeList dom_child_list = dom_node.childNodes();
    int child_cnt = dom_child_list.count();
    EgMmlNode *mml_child = 0;

    QString separator_list;
    if ( mml_type == EgMathMlNodeType::MfencedNode )
        separator_list = mml_node->explicitAttribute( "separators", "," );

    switch ( spec->child_spec )
    {
        case EgMmlNodeSpec::ChildIgnore:
            break;

        case EgMmlNodeSpec::ImplicitMrow:

            if ( child_cnt > 0 )
            {
                mml_child = createImplicitMrowNode( dom_node, ok, errorMsg );
                if ( !*ok )
                {
                    delete mml_node;
                    return 0;
                }

                if ( !insertChild( mml_node, mml_child, errorMsg ) )
                {
                    delete mml_node;
                    delete mml_child;
                    *ok = false;
                    return 0;
                }
            }

            break;

        default:
            // exact ammount of children specified - check...
            if ( spec->child_spec != child_cnt )
            {
                if ( errorMsg != 0 )
                    *errorMsg = QString( "element " )
                                + spec->tag
                                + " requires exactly "
                                + QString::number( spec->child_spec )
                                + " arguments, got "
                                + QString::number( child_cnt );
                delete mml_node;
                *ok = false;
                return 0;
            }

            // ...and continue just as in ChildAny

        case EgMmlNodeSpec::ChildAny:
            if ( mml_type == EgMathMlNodeType::MfencedNode )
                insertOperator( mml_node, mml_node->explicitAttribute( "open", "(" ) );

            for ( int i = 0; i < child_cnt; ++i )
            {
                QDomNode dom_child = dom_child_list.item( i );

                EgMmlNode *mml_child = domToMml( dom_child, ok, errorMsg );
                if ( !*ok )
                {
                    delete mml_node;
                    return 0;
                }

                if ( mml_type == EgMathMlNodeType::MtableNode && mml_child->nodeType() != EgMathMlNodeType::MtrNode )
                {
                    EgMmlNode *mtr_node = createNode( EgMathMlNodeType::MtrNode, EgMmlAttributeMap(), QString::null, 0 );
                    insertChild( mml_node, mtr_node, 0 );
                    if ( !insertChild( mtr_node, mml_child, errorMsg ) )
                    {
                        delete mml_node;
                        delete mml_child;
                        *ok = false;
                        return 0;
                    }
                }
                else if ( mml_type == EgMathMlNodeType::MtrNode && mml_child->nodeType() != EgMathMlNodeType::MtdNode )
                {
                    EgMmlNode *mtd_node = createNode( EgMathMlNodeType::MtdNode, EgMmlAttributeMap(), QString::null, 0 );
                    insertChild( mml_node, mtd_node, 0 );
                    if ( !insertChild( mtd_node, mml_child, errorMsg ) )
                    {
                        delete mml_node;
                        delete mml_child;
                        *ok = false;
                        return 0;
                    }
                }
                else
                {
                    if ( !insertChild( mml_node, mml_child, errorMsg ) )
                    {
                        delete mml_node;
                        delete mml_child;
                        *ok = false;
                        return 0;
                    }
                }

                if ( i < child_cnt - 1 && mml_type == EgMathMlNodeType::MfencedNode && !separator_list.isEmpty() )
                {
                    QChar separator;
                    if ( i >= separator_list.length() )
                        separator = separator_list.at( separator_list.length() - 1 );
                    else
                        separator = separator_list[i];
                    insertOperator( mml_node, QString( separator ) );
                }
            }

            if ( mml_type == EgMathMlNodeType::MfencedNode )
                insertOperator( mml_node, mml_node->explicitAttribute( "close", ")" ) );

            break;
    }

    *ok = true;
    return mml_node;
}

EgMmlNode *EgMmlDocument::createImplicitMrowNode( const QDomNode &dom_node,
                                                    bool *ok,
                                                    QString *errorMsg )
{
    QDomNodeList dom_child_list = dom_node.childNodes();
    int child_cnt = dom_child_list.count();

    if ( child_cnt == 0 )
    {
        *ok = true;
        return 0;
    }

    if ( child_cnt == 1 )
        return domToMml( dom_child_list.item( 0 ), ok, errorMsg );

    EgMmlNode *mml_node = createNode( EgMathMlNodeType::MrowNode, EgMmlAttributeMap(),
                                       QString::null, errorMsg );
    Q_ASSERT( mml_node != 0 ); // there is no reason in heaven or hell for this to fail

    for ( int i = 0; i < child_cnt; ++i )
    {
        QDomNode dom_child = dom_child_list.item( i );

        EgMmlNode *mml_child = domToMml( dom_child, ok, errorMsg );
        if ( !*ok )
        {
            delete mml_node;
            return 0;
        }

        if ( !insertChild( mml_node, mml_child, errorMsg ) )
        {
            delete mml_node;
            delete mml_child;
            *ok = false;
            return 0;
        }
    }

    return mml_node;
}

void EgMmlDocument::paint( QPainter *painter, const QPointF &pos )
{
    if ( m_root_node == 0 )
        return;

    m_root_node->setRelOrigin( pos - m_root_node->myRect().topLeft() );
    m_root_node->paint( painter, 1.0, 1.0 );
}

QSizeF EgMmlDocument::size() const
{
    if ( m_root_node == 0 )
        return QSizeF( 0.0, 0.0 );
    return m_root_node->deviceRect().size();
}

void EgMmlDocument::doPostProcessing(void)
{
        int n;
        Subindexes data;
        QMutableHashIterator<quint32, Subindexes> i(m_renderingData);
        QRectF rect;

        while (i.hasNext()) {
             i.next();
             data = i.value();

             for (n = 0; n < data.size(); n++) {
                     quint64 key;

                     key = (static_cast<quint64>(n) << 32) | i.key();
                     if (m_nodeIdLookup.contains(key)) {
                             EgAddRendData add_data = m_nodeIdLookup.value(key);

                             //adjust char positions
                             if (n == 0) {
                                     rect = data[0].m_itemRect;
                             } else {
                                     if ( ((add_data.m_bits) & EgRendAdjustBits::translateTxt) != EgRendAdjustBits::Nothing) {
                                             data[n].m_itemRect.translate(rect.x(), rect.y());
                                     }
                             }

                             //adjust lspaces
                             if ( ((add_data.m_bits) & EgRendAdjustBits::translateLspace) != EgRendAdjustBits::Nothing) {
                                     qreal lspace = 0.0;

                                     if (add_data.m_node->nodeType() == EgMathMlNodeType::MoNode)
                                             lspace = static_cast<EgMmlMoNode*>(add_data.m_node)->lspace();
                                     if (add_data.m_node->nodeType() == EgMathMlNodeType::MpaddedNode)
                                             lspace = static_cast<EgMmlMpaddedNode*>(add_data.m_node)->lspace();
                                     data[n].m_itemRect.translate(lspace, 0.0);
                             }
                     }
             }
             i.setValue(data);
         }
}

void EgMmlDocument::optimizeSize(void)
{
        m_nodeIdLookup.clear();
}

QVector<EgRenderingPosition> EgMmlDocument::getRenderingPositions(void)
{
        QVector<EgRenderingPosition> tmp;
        QVector<EgRenderingPosition> i;

        foreach (i, m_renderingData) {
                tmp.append(i);
        }

        return tmp;
}

bool EgMmlDocument::appendRenderingData(quint32 nodeId, quint32 index, EgMmlNode* node,
                                        EgRendAdjustBits data = EgRendAdjustBits::Nothing)
{
        bool retval = false;

        if (nodeId == 0)
                return retval;

        if (    !m_nodeIdLookup.contains(((static_cast<quint64>(index) << 32) | nodeId))
             && !m_renderingComplete) {
                retval = true;
                EgAddRendData indPos(m_renderingData.size(), node, data);
                EgRenderingPosition renderingData;
                renderingData.m_nodeId = nodeId;
                renderingData.m_subPos = index;
                m_renderingData.insert(nodeId, renderingData);
                m_nodeIdLookup.insert((static_cast<quint64>(index) << 32) | nodeId, indPos);
        }

        return retval;
}

void EgMmlDocument::updateRenderingData(quint32 nodeId, quint32 index, QRectF position)
{
        if (nodeId == 0)
                return;

        if (m_nodeIdLookup.contains(((static_cast<quint64>(index) << 32) | nodeId))) {
                EgAddRendData indPos(m_nodeIdLookup.value((static_cast<quint64>(index) << 32) | nodeId));
                m_renderingData[indPos.m_index].m_itemRect = position;
        }
}

void EgMmlDocument::clearRendering(void)
{
        m_renderingData.clear();
        m_nodeIdLookup.clear();
        m_renderingComplete = false;
}


// *******************************************************************
// EgMmlNode
// *******************************************************************

EgMmlNode::EgMmlNode( EgMathMlNodeType type, EgMmlDocument *document,
                        const EgMmlAttributeMap &attribute_map )
{
    m_parent = 0;
    m_first_child = 0;
    m_next_sibling = 0;
    m_previous_sibling = 0;

    m_node_type = type;
    m_document = document;
    m_attribute_map = attribute_map;

    m_my_rect = m_parent_rect = QRectF( 0.0, 0.0, 0.0, 0.0 );
    m_rel_origin = QPointF( 0.0, 0.0 );
    m_stretched = false;
    m_nodeId = 0;
}

EgMmlNode::~EgMmlNode()
{
    EgMmlNode *n = m_first_child;
    while ( n != 0 )
    {
        EgMmlNode *tmp = n->nextSibling();
        delete n;
        n = tmp;
    }
}

static QString rectToStr( const QRectF &rect )
{
    return QString( "[(%1, %2), %3x%4]" )
           .arg( rect.x() )
           .arg( rect.y() )
           .arg( rect.width() )
           .arg( rect.height() );
}

QString EgMmlNode::toStr() const
{
    const EgMmlNodeSpec *spec = mmlFindNodeSpec( m_node_type );
    Q_ASSERT( spec != 0 );

    return QString( "%1 %2 mr=%3 pr=%4 dr=%5 ro=(%7, %8) str=%9" )
           .arg( spec->type_str )
           .arg( ( quintptr )this, 0, 16 )
           .arg( rectToStr( m_my_rect ) )
           .arg( rectToStr( parentRect() ) )
           .arg( rectToStr( deviceRect() ) )
           .arg( m_rel_origin.x() )
           .arg( m_rel_origin.y() )
           .arg( ( int )m_stretched );
}

qreal EgMmlNode::interpretSpacing( const QString &value, bool *ok ) const
{
        return mmlInterpretSpacing( value, em(), ex(), ok , m_document->baseFontPixelSize(), EgMmlDocument::MmToPixelFactor());
}

qreal EgMmlNode::lineWidth() const
{
    return qMax( 1.0, QFontMetricsF( font() ).lineWidth() );
}

qreal EgMmlNode::basePos() const
{
    QFontMetricsF fm( font() );
    return fm.strikeOutPos();
}

EgMmlNode *EgMmlNode::lastSibling() const
{
    const EgMmlNode *n = this;
    while ( !n->isLastSibling() )
        n = n->nextSibling();
    return const_cast<EgMmlNode*>( n );
}

EgMmlNode *EgMmlNode::firstSibling() const
{
    const EgMmlNode *n = this;
    while ( !n->isFirstSibling() )
        n = n->previousSibling();
    return const_cast<EgMmlNode*>( n );
}

qreal EgMmlNode::em() const
{
    return QFontMetricsF( font() ).boundingRect( 'm' ).width();
}

qreal EgMmlNode::ex() const
{
    return QFontMetricsF( font() ).boundingRect( 'x' ).height();
}

int EgMmlNode::scriptlevel( const EgMmlNode * ) const
{
    int parent_sl;
    if ( m_parent == 0 )
        parent_sl = 0;
    else
        parent_sl = m_parent->scriptlevel( this );

    QString expl_sl_str = explicitAttribute( "scriptlevel" );
    if ( expl_sl_str.isNull() )
        return parent_sl;

    if ( expl_sl_str.startsWith( "+" ) || expl_sl_str.startsWith( "-" ) )
    {
        bool ok;
        int expl_sl = expl_sl_str.toInt( &ok );
        if ( ok )
        {
            return parent_sl + expl_sl;
        }
        else
        {
            qWarning() << "EgMmlNode::scriptlevel(): bad value " + expl_sl_str;
            return parent_sl;
        }
    }

    bool ok;
    int expl_sl = expl_sl_str.toInt( &ok );
    if ( ok )
        return expl_sl;


    if ( expl_sl_str == "+" )
        return parent_sl + 1;
    else if ( expl_sl_str == "-" )
        return parent_sl - 1;
    else
    {
        qWarning() << "EgMmlNode::scriptlevel(): could not parse value: \"" + expl_sl_str + "\"";
        return parent_sl;
    }
}

QPointF EgMmlNode::devicePoint( const QPointF &pos ) const
{
    QRectF dr = deviceRect();

    if ( m_stretched )
    {
        return dr.topLeft() + QPointF( ( pos.x() - m_my_rect.left() ) * dr.width() / m_my_rect.width(),
                                       ( pos.y() - m_my_rect.top() ) * dr.height() / m_my_rect.height() );
    }
    else
    {
        return dr.topLeft() + pos - m_my_rect.topLeft();
    }
}

QString EgMmlNode::inheritAttributeFromMrow( const QString &name,
                                              const QString &def ) const
{
    const EgMmlNode *p = this;
    for ( ; p != 0; p = p->parent() )
    {
        if ( p == this || p->nodeType() == EgMathMlNodeType::MstyleNode )
        {
            QString value = p->explicitAttribute( name );
            if ( !value.isNull() )
                return value;
        }
    }

    return def;
}

QColor EgMmlNode::color() const
{
    // If we are child of <merror> return red
    const EgMmlNode *p = this;
    for ( ; p != 0; p = p->parent() )
    {
        if ( p->nodeType() == EgMathMlNodeType::MerrorNode )
            return QColor( "red" );
    }

    QString value_str = inheritAttributeFromMrow( "mathcolor" );
    if ( value_str.isNull() )
        value_str = inheritAttributeFromMrow( "color" );
    if ( value_str.isNull() )
        return QColor();

    return QColor( value_str );
}

QColor EgMmlNode::background() const
{
    QString value_str = inheritAttributeFromMrow( "mathbackground" );
    if ( value_str.isNull() )
        value_str = inheritAttributeFromMrow( "background" );
    if ( value_str.isNull() )
        return QColor();

    return QColor( value_str );
}

static void updateFontAttr( EgMmlAttributeMap &font_attr, const EgMmlNode *n,
                            const QString &name,
                            const QString &preferred_name = QString::null )
{
    if ( font_attr.contains( preferred_name ) || font_attr.contains( name ) )
        return;
    QString value = n->explicitAttribute( name );
    if ( !value.isNull() )
        font_attr[name] = value;
}

static EgMmlAttributeMap collectFontAttributes( const EgMmlNode *node )
{
    EgMmlAttributeMap font_attr;

    for ( const EgMmlNode *n = node; n != 0; n = n->parent() )
    {
        if ( n == node || n->nodeType() == EgMathMlNodeType::MstyleNode )
        {
            updateFontAttr( font_attr, n, "mathvariant" );
            updateFontAttr( font_attr, n, "mathsize" );

            // depreciated attributes
            updateFontAttr( font_attr, n, "fontsize", "mathsize" );
            updateFontAttr( font_attr, n, "fontweight", "mathvariant" );
            updateFontAttr( font_attr, n, "fontstyle", "mathvariant" );
            updateFontAttr( font_attr, n, "fontfamily", "mathvariant" );
        }
    }

    return font_attr;
}

QFont EgMmlNode::font() const
{
    QFont fn( m_document->fontName( EgMathMLDocument::NormalFont ) );
    fn.setPixelSize(static_cast<int>(m_document->baseFontPixelSize()) );

    qreal ps = static_cast<qreal>(fn.pixelSize());
    int sl = scriptlevel();
    if ( sl >= 0 )
    {
        for ( int i = 0; i < sl; ++i )
            ps = ps * g_script_size_multiplier;
    }
    else
    {
        for ( int i = 0; i > sl; --i )
            ps = ps / g_script_size_multiplier;
    }
    if ( ps < g_min_font_pixel_size_calc )
        ps = g_min_font_pixel_size_calc;
    fn.setPixelSize( static_cast<int>(ps) );

    const qreal em = QFontMetricsF( fn ).boundingRect( 'm' ).width();
    const qreal ex = QFontMetricsF( fn ).boundingRect( 'x' ).height();

    EgMmlAttributeMap font_attr = collectFontAttributes( this );

    if ( font_attr.contains( "mathvariant" ) )
    {
        QString value = font_attr["mathvariant"];

        bool ok;
        int mv = mmlInterpretMathVariant( value, &ok );

        if ( ok )
        {
            if ( mv & ScriptMV )
                fn.setFamily( m_document->fontName( EgMathMLDocument::ScriptFont ) );

            if ( mv & FrakturMV )
                fn.setFamily( m_document->fontName( EgMathMLDocument::FrakturFont ) );

            if ( mv & SansSerifMV )
                fn.setFamily( m_document->fontName( EgMathMLDocument::SansSerifFont ) );

            if ( mv & MonospaceMV )
                fn.setFamily( m_document->fontName( EgMathMLDocument::MonospaceFont ) );

            if ( mv & DoubleStruckMV )
                fn.setFamily( m_document->fontName( EgMathMLDocument::DoublestruckFont ) );

            if ( mv & BoldMV )
                fn.setBold( true );

            if ( mv & ItalicMV )
                fn.setItalic( true );
        }
    }

    if ( font_attr.contains( "mathsize" ) )
    {
        QString value = font_attr["mathsize"];
        fn = mmlInterpretMathSize( value, fn, em, ex, 0, baseFontPixelSize() );
    }

    fn = mmlInterpretDepreciatedFontAttr( font_attr, fn, em, ex, baseFontPixelSize() );

    if ( m_node_type == EgMathMlNodeType::MiNode
            && !font_attr.contains( "mathvariant" )
            && !font_attr.contains( "fontstyle" ) )
    {
        const EgMmlMiNode *mi_node = ( const EgMmlMiNode* ) this;
        if ( mi_node->text().length() == 1 )
            fn.setItalic( true );
    }

    if ( m_node_type == EgMathMlNodeType::MoNode )
    {
        fn.setItalic( false );
        fn.setBold( false );
    }

    return fn;
}

QString EgMmlNode::explicitAttribute( const QString &name, const QString &def ) const
{
    EgMmlAttributeMap::const_iterator it = m_attribute_map.find( name );
    if ( it != m_attribute_map.end() )
        return *it;
    return def;
}


QRectF EgMmlNode::parentRect() const
{
    if ( m_stretched )
        return m_parent_rect;

    return QRectF( m_rel_origin + m_my_rect.topLeft(), m_my_rect.size() );
}


void EgMmlNode::stretchTo( const QRectF &rect )
{
    m_parent_rect = rect;
    m_stretched = true;
}

void EgMmlNode::setRelOrigin( const QPointF &rel_origin )
{
    m_rel_origin = rel_origin + QPointF( -m_my_rect.left(), 0.0 );
    m_stretched = false;
}

void EgMmlNode::updateMyRect()
{
    m_my_rect = symbolRect();
    EgMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() )
        m_my_rect |= child->parentRect();
}

void EgMmlNode::layout()
{
        m_parent_rect = QRectF( 0.0, 0.0, 0.0, 0.0 );
        m_stretched = false;
        m_rel_origin = QPointF( 0.0, 0.0 );

        //add node id to vector
        m_document->appendRenderingData(m_nodeId, 0, this);

        EgMmlNode *child = m_first_child;
        quint32 i;
        for (i = 0 ; child != 0; child = child->nextSibling() ) {
                layoutHook(i);
                child->layout();
                i++;
        }

        layoutSymbol();

        updateMyRect();

        if ( m_parent == 0 )
                m_rel_origin = QPointF( 0.0, 0.0 );
}


QRectF EgMmlNode::deviceRect() const
{
    if ( m_parent == 0 )
        return QRectF( m_rel_origin + m_my_rect.topLeft(), m_my_rect.size() );

    QRectF pdr = m_parent->deviceRect();
    QRectF pr = parentRect();
    QRectF pmr = m_parent->myRect();

    qreal scale_w = 0.0;
    if ( pmr.width() != 0.0 )
        scale_w = pdr.width() / pmr.width();
    qreal scale_h = 0.0;
    if ( pmr.height() != 0.0 )
        scale_h = pdr.height() / pmr.height();

    return QRectF( pdr.left() + ( pr.left() - pmr.left() ) * scale_w,
                   pdr.top()  + ( pr.top() - pmr.top() ) * scale_h,
                   pr.width() * scale_w,
                   pr.height() * scale_h );
}

void EgMmlNode::layoutSymbol()
{
    // default behaves like an mrow

    // now lay them out in a neat row, aligning their origins to my origin
    qreal w = 0.0;
    EgMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() )
    {
        child->setRelOrigin( QPointF( w, 0.0 ) );
        w += child->parentRect().width() + 1.0;
    }
}

void EgMmlNode::paint(
    QPainter *painter, qreal x_scaling, qreal y_scaling )
{    
    if ( !m_my_rect.isValid() )
        return;

    painter->save();

    QRectF d_rect = deviceRect();

    //update node id position data
    m_document->updateRenderingData(m_nodeId, 0, d_rect);

    if ( m_stretched )
    {
        x_scaling *= d_rect.width() / m_my_rect.width();
        y_scaling *= d_rect.height() / m_my_rect.height();
    }

    if ( m_node_type != EgMathMlNodeType::UnknownNode )
    {
        const QColor bg = background();
        if ( bg.isValid() )
            painter->fillRect( d_rect, bg );
        else
            painter->fillRect( d_rect, m_document->backgroundColor() );

        const QColor fg = color();
        if ( fg.isValid() )
            painter->setPen( QPen( fg, 1 ) );
        else
            painter->setPen( QPen( m_document->foregroundColor(), 1 ) );
    }

    EgMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() ) {
            child->paint( painter, x_scaling, y_scaling );
    }

    if ( m_node_type != EgMathMlNodeType::UnknownNode )
        paintSymbol( painter, x_scaling, y_scaling );

    painter->restore();
}

void EgMmlNode::paintSymbol( QPainter *painter, qreal, qreal ) const
{
    QRectF d_rect = deviceRect();
    if ( m_document->drawFrames() && d_rect.isValid() )
    {
        painter->save();

        painter->setPen( QPen( Qt::red, 0 ) );

        painter->drawRect( d_rect );

        QPen pen = painter->pen();
        pen.setStyle( Qt::DotLine );
        painter->setPen( pen );

        const QPointF d_pos = devicePoint( QPointF() );

        painter->drawLine( QPointF( d_rect.left(), d_pos.y() ),
                           QPointF( d_rect.right(), d_pos.y() ) );

        painter->restore();
    }
}

void EgMmlNode::stretch()
{
    EgMmlNode *child = m_first_child;
    for ( ; child != 0; child = child->nextSibling() )
        child->stretch();
}

QString EgMmlTokenNode::text() const
{
    QString result;

    const EgMmlNode *child = firstChild();
    for ( ; child != 0; child = child->nextSibling() )
    {
        if ( child->nodeType() != EgMathMlNodeType::TextNode ) continue;
        if ( !result.isEmpty() )
            result += ' ';
        result += static_cast<const EgMmlTextNode *>( child )->text();
    }

    return result;
}

EgMmlNode *EgMmlMfracNode::numerator() const
{
    Q_ASSERT( firstChild() != 0 );
    return firstChild();
}

EgMmlNode *EgMmlMfracNode::denominator() const
{
    EgMmlNode *node = numerator()->nextSibling();
    Q_ASSERT( node != 0 );
    return node;
}

void EgMmlMfracNode::layoutHook(quint32 childIndex)
{
        if (childIndex == 1) {
                m_document->appendRenderingData(m_nodeId, childIndex, this);
        }
}

QRectF EgMmlMfracNode::symbolRect() const
{
    QRectF num_rect = numerator()->myRect();
    QRectF denom_rect = denominator()->myRect();
    qreal spacing = g_mfrac_spacing * ( num_rect.height() + denom_rect.height() );
    qreal my_width = qMax( num_rect.width(), denom_rect.width() ) + 2.0 * spacing;
    int line_thickness = qCeil( lineThickness() );

    return QRectF( -0.5 * ( my_width + line_thickness ), -0.5 * line_thickness,
                   my_width + line_thickness, line_thickness );
}

void EgMmlMfracNode::layoutSymbol()
{
    EgMmlNode *num = numerator();
    EgMmlNode *denom = denominator();

    QRectF num_rect = num->myRect();
    QRectF denom_rect = denom->myRect();

    qreal spacing = g_mfrac_spacing * ( num_rect.height() + denom_rect.height() );
    int line_thickness = qCeil( lineThickness() );

    num->setRelOrigin( QPointF( -0.5 * num_rect.width(), - spacing - num_rect.bottom() - 0.5 * line_thickness ) );
    denom->setRelOrigin( QPointF( -0.5 * denom_rect.width(), spacing - denom_rect.top() + 0.5 * line_thickness ) );
}

static bool zeroLineThickness( const QString &s )
{
    if ( s.length() == 0 || !s[0].isDigit() )
        return false;

    for ( int i = 0; i < s.length(); ++i )
    {
        QChar c = s.at( i );
        if ( c.isDigit() && c != '0' )
            return false;
    }
    return true;
}

qreal EgMmlMfracNode::lineThickness() const
{
    QString linethickness_str = inheritAttributeFromMrow( "linethickness", QString::number( 0.75 * lineWidth () ) );

    /* InterpretSpacing returns a qreal, which might be 0 even if the thickness
       is > 0, though very very small. That's ok, because we can set it to 1.
       However, we have to run this check if the line thickness really is zero */
    if ( !zeroLineThickness( linethickness_str ) )
    {
        bool ok;
        qreal line_thickness = interpretSpacing( linethickness_str, &ok );
        if ( !ok || !line_thickness )
            line_thickness = 1.0;

        return line_thickness;
    }
    else
    {
        return 0.0;
    }
}

void EgMmlMfracNode::paintSymbol(
    QPainter *painter, qreal x_scaling, qreal y_scaling ) const
{
    EgMmlNode::paintSymbol( painter, x_scaling, y_scaling );

    int line_thickness = qCeil( lineThickness() );

    if ( line_thickness != 0.0 )
    {
        painter->save();

        QPen pen = painter->pen();
        pen.setWidthF( line_thickness );
        painter->setPen( pen );

        QRectF s_rect = symbolRect();
        s_rect.moveTopLeft( devicePoint( s_rect.topLeft() ) );

        QRectF rect(s_rect);
        m_document->updateRenderingData(m_nodeId, 1, rect);

        painter->drawLine( QPointF( s_rect.left() + 0.5 * line_thickness, s_rect.center().y() ),
                           QPointF( s_rect.right() - 0.5 * line_thickness, s_rect.center().y() ) );

        painter->restore();
    }
}

EgMmlNode *EgMmlRootBaseNode::base() const
{
    return firstChild();
}

EgMmlNode *EgMmlRootBaseNode::index() const
{
    EgMmlNode *b = base();
    if ( b == 0 )
        return 0;
    return b->nextSibling();
}

int EgMmlRootBaseNode::scriptlevel( const EgMmlNode *child ) const
{
    int sl = EgMmlNode::scriptlevel();

    EgMmlNode *i = index();
    if ( child != 0 && child == i )
        return sl + 1;
    else
        return sl;
}

QRectF EgMmlRootBaseNode::baseRect() const
{
    EgMmlNode *b = base();
    if ( b == 0 ) { // in case of a sqrt without an element or with an unvisible element, choose to render the rect of a "0"
        QRectF br = QFontMetricsF( font() ).tightBoundingRect( "0" );
        br.translate( 0.0, basePos() );
        return br;
    } else {
        return b->myRect();
    }
}

QRectF EgMmlRootBaseNode::radicalRect() const
{
    return QFontMetricsF( font() ).boundingRect( QChar( g_radical ) );
}

qreal EgMmlRootBaseNode::radicalMargin() const
{
    return g_mroot_base_margin * baseRect().height();
}

qreal EgMmlRootBaseNode::radicalLineWidth() const
{
    return g_mroot_base_line * lineWidth();
}

QRectF EgMmlRootBaseNode::symbolRect() const
{
    QRectF base_rect = baseRect();
    qreal radical_margin = radicalMargin();
    qreal radical_width = radicalRect().width();
    int radical_line_width = qCeil( radicalLineWidth() );

    return QRectF( -radical_width, base_rect.top() - radical_margin - radical_line_width,
                    radical_width + base_rect.width() + radical_margin, base_rect.height() + 2.0 * radical_margin + radical_line_width );
}

void EgMmlRootBaseNode::layoutSymbol()
{
    EgMmlNode *b = base();
    if ( b != 0 )
        b->setRelOrigin( QPointF( 0.0, 0.0 ) );

    EgMmlNode *i = index();
    if ( i != 0 )
    {
        QRectF i_rect = i->myRect();
        i->setRelOrigin( QPointF( -0.6 * radicalRect().width() - i_rect.width(),
                                  -1.1 * i_rect.bottom() ) );
    }
}

void EgMmlRootBaseNode::paintSymbol(
    QPainter *painter, qreal x_scaling, qreal y_scaling ) const
{
    EgMmlNode::paintSymbol( painter, x_scaling, y_scaling );

    painter->save();

    QRectF s_rect = symbolRect();
    s_rect.moveTopLeft( devicePoint( s_rect.topLeft() ) );

    QRectF radical_rect = QFontMetricsF( font() ).boundingRect( QChar( g_radical ) );

    QRectF rect = s_rect;
    rect.adjust(  0.0, qCeil( radicalLineWidth() ),
                 -(rect.width() - radical_rect.width() ), 0.0 );

    painter->translate( rect.bottomLeft() );

    QPointF radical_points[ g_radical_points_size ];

    for ( int i = 0; i < g_radical_points_size; ++i )
    {
        radical_points[ i ].setX( radical_rect.width() * g_radical_points[ i ].x() );
        radical_points[ i ].setY( -rect.height() * g_radical_points[ i ].y() );
    }

    qreal x2 = radical_points[ 2 ].x();
    qreal y2 = radical_points[ 2 ].y();
    qreal x3 = radical_points[ 3 ].x();
    qreal y3 = radical_points[ 3 ].y();

    radical_points[ 4 ].setX( s_rect.width() );
    radical_points[ 5 ].setX( s_rect.width() );

    radical_points[ 3 ].setY( -s_rect.height() );
    radical_points[ 4 ].setY( -s_rect.height() );

    qreal new_y3 = radical_points[ 3 ].y();

    radical_points[ 3 ].setX( x2 + ( x3 - x2 ) * ( new_y3 - y2 ) / ( y3 - y2 ) );

    QBrush brush = painter->brush();
    brush.setColor( painter->pen().color() );
    brush.setStyle( Qt::SolidPattern );
    painter->setBrush( brush );

    painter->setRenderHint( QPainter::Antialiasing, true );

    painter->drawPolygon( radical_points, g_radical_points_size );

    painter->restore();
}

EgMmlTextNode::EgMmlTextNode( const QString &text, EgMmlDocument *document )
    : EgMmlNode( EgMathMlNodeType::TextNode, document, EgMmlAttributeMap() )
{
    m_text = text;
    // Trim whitespace from ends, but keep nbsp and thinsp
    m_text.remove( QRegExp( "^[^\\S\\x00a0\\x2009]+" ) );
    m_text.remove( QRegExp( "[^\\S\\x00a0\\x2009]+$" ) );
}

QString EgMmlTextNode::toStr() const
{
    return EgMmlNode::toStr() + " text=\"" + m_text + "\"";
}

bool EgMmlTextNode::isInvisibleOperator() const
{
    return    m_text == QString( QChar( 0x61, 0x20 ) )  // &ApplyFunction;
           || m_text == QString( QChar( 0x62, 0x20 ) )  // &InvisibleTimes;
           || m_text == QString( QChar( 0x63, 0x20 ) )  // &InvisibleComma;
           || m_text == QString( QChar( 0x64, 0x20 ) ); // Invisible addition
}

void EgMmlTextNode::paintSymbol(
    QPainter *painter, qreal x_scaling, qreal y_scaling ) const
{
    EgMmlNode::paintSymbol( painter, x_scaling, y_scaling );

    if ( isInvisibleOperator() )
        return;

    painter->save();

    QPointF d_pos = devicePoint( QPointF() );
    QPointF s_pos = symbolRect().topLeft();

    painter->translate( d_pos + s_pos );
    painter->scale( x_scaling, y_scaling );
    painter->setFont( font() );

    painter->drawText( QPointF( 0.0, basePos() ) - s_pos, m_text );

    painter->restore();
}

QRectF EgMmlTextNode::symbolRect() const
{
    QRectF br;
    if (isInvisibleOperator()) {
            br = QRectF();
    } else {
            QFontMetricsF metrics = QFontMetricsF( font() );
            br = metrics.tightBoundingRect( m_text );
    }

    br.translate( 0.0, basePos() );
    generateTxtRenderingData(br);

    return br;
}

void EgMmlTextNode::generateTxtRenderingData(QRectF rect) const
{
        if (isInvisibleOperator())
                return;

        qreal previousWidth = 0.0;

        int size = m_text.size();

        if (size <= 0)
                return;
        if (!m_parent)
                return;
        if (m_parent->getNodeId() == 0)
                return;

        int i;
        for(i = 1; i <= size; i++) {
                previousWidth = TxtRenderingDataHelper(rect, m_text.left(i), previousWidth);
        }
}

qreal EgMmlTextNode::TxtRenderingDataHelper(QRectF parentRect, QString text, qreal previousWidth) const
{
        QFontMetricsF metrics = QFontMetricsF(font());
        quint32 parentId = 0;
        EgMathMlNodeType nodeType;
        qreal newWidth;
        if (m_parent) {
                parentId = m_parent->getNodeId();
                nodeType = m_parent->nodeType();
        } else {
                return 0.0;
        }

        EgRendAdjustBits adjBits = EgRendAdjustBits::Nothing;
        if (    nodeType == EgMathMlNodeType::MoNode
             || nodeType == EgMathMlNodeType::MpaddedNode) {
                adjBits = EgRendAdjustBits::translateLspace;
        }

        quint64 i = text.size();
        m_document->appendRenderingData(parentId, i, m_parent, adjBits | EgRendAdjustBits::translateTxt);

        QRectF newRect = QRectF(QPointF(0.0, 0.0), parentRect.size());;
        newRect.translate(previousWidth, 0.0);
        newRect.setWidth(metrics.width(text.right(1)));
        newRect.setHeight(metrics.boundingRect('X').height());

        if (newRect.width() + previousWidth > parentRect.width()) {
                newWidth = qMin(newRect.width() + previousWidth, parentRect.width());
                newRect.setWidth(parentRect.width() - previousWidth);
        } else {
                newWidth = newRect.width() + previousWidth;
        }

        newRect.moveBottom(basePos() - parentRect.top());

        //width - previousWidth
        m_document->updateRenderingData(parentId, i, newRect);

        return newWidth;
}

EgMmlNode *EgMmlSubsupBaseNode::base() const
{
    Q_ASSERT( firstChild() != 0 );
    return firstChild();
}

EgMmlNode *EgMmlSubsupBaseNode::sscript() const
{
    EgMmlNode *s = base()->nextSibling();
    Q_ASSERT( s != 0 );
    return s;
}

int EgMmlSubsupBaseNode::scriptlevel( const EgMmlNode *child ) const
{
    int sl = EgMmlNode::scriptlevel();

    EgMmlNode *s = sscript();
    if ( child != 0 && child == s )
        return sl + 1;
    else
        return sl;
}

void EgMmlMsupNode::layoutSymbol()
{
    EgMmlNode *b = base();
    EgMmlNode *s = sscript();

    b->setRelOrigin( QPointF( -b->myRect().width(), 0.0 ) );
    s->setRelOrigin( QPointF( interpretSpacing( g_subsup_spacing, 0 ),
                              -g_sup_shift_multiplier * s->myRect().height() ) );
}

void EgMmlMsubNode::layoutSymbol()
{
    EgMmlNode *b = base();
    EgMmlNode *s = sscript();

    b->setRelOrigin( QPointF( -b->myRect().width(), 0.0 ) );
    s->setRelOrigin( QPointF( interpretSpacing( g_subsup_spacing, 0 ),
                              s->myRect().height() ) );
}

EgMmlNode *EgMmlMsubsupNode::base() const
{
    Q_ASSERT( firstChild() != 0 );
    return firstChild();
}

EgMmlNode *EgMmlMsubsupNode::subscript() const
{
    EgMmlNode *sub = base()->nextSibling();
    Q_ASSERT( sub != 0 );
    return sub;
}

EgMmlNode *EgMmlMsubsupNode::superscript() const
{
    EgMmlNode *sup = subscript()->nextSibling();
    Q_ASSERT( sup != 0 );
    return sup;
}

void EgMmlMsubsupNode::layoutSymbol()
{
    EgMmlNode *b = base();
    EgMmlNode *sub = subscript();
    EgMmlNode *sup = superscript();

    b->setRelOrigin( QPointF( -b->myRect().width(), 0.0 ) );

    QRectF sub_rect = sub->myRect();
    QRectF sup_rect = sup->myRect();
    qreal subsup_spacing = interpretSpacing( g_subsup_spacing, 0 );
    qreal shift = 0.0;

    sub_rect.moveTo( QPointF( 0.0, sub->myRect().height() ) );
    sup_rect.moveTo( QPointF( 0.0, -g_sup_shift_multiplier * sup->myRect().height() ) );

    qreal subsup_diff = sub_rect.top() - sup_rect.bottom();

    if ( subsup_diff < subsup_spacing )
        shift = 0.5 * ( subsup_spacing - subsup_diff );

    sub->setRelOrigin( QPointF( subsup_spacing, sub_rect.top() + shift ) );
    sup->setRelOrigin( QPointF( subsup_spacing, sup_rect.top() - shift ) );
}

int EgMmlMsubsupNode::scriptlevel( const EgMmlNode *child ) const
{
    int sl = EgMmlNode::scriptlevel();

    EgMmlNode *sub = subscript();
    EgMmlNode *sup = superscript();

    if ( child != 0 && ( child == sup || child == sub ) )
        return sl + 1;
    else
        return sl;
}

QString EgMmlMoNode::toStr() const
{
    return EgMmlNode::toStr() + QString( " form=%1" ).arg( ( int )form() );
}

void EgMmlMoNode::layoutSymbol()
{
    if ( firstChild() == 0 )
        return;

    firstChild()->setRelOrigin( QPointF( 0.0, 0.0 ) );

    if ( m_oper_spec == 0 )
        m_oper_spec = mmlFindOperSpec( text(), form() );
}

EgMmlMoNode::EgMmlMoNode( EgMmlDocument *document,
                            const EgMmlAttributeMap &attribute_map )
    : EgMmlTokenNode( EgMathMlNodeType::MoNode, document, attribute_map )
{
    m_oper_spec = 0;
}

QString EgMmlMoNode::dictionaryAttribute( const QString &name ) const
{
    const EgMmlNode *p = this;
    for ( ; p != 0; p = p->parent() )
    {
        if ( p == this || p->nodeType() == EgMathMlNodeType::MstyleNode )
        {
            QString expl_attr = p->explicitAttribute( name );
            if ( !expl_attr.isNull() )
                return expl_attr;
        }
    }

    return mmlDictAttribute( name, m_oper_spec );
}

EgMml::FormType EgMmlMoNode::form() const
{
    QString value_str = inheritAttributeFromMrow( "form" );
    if ( !value_str.isNull() )
    {
        bool ok;
        FormType value = mmlInterpretForm( value_str, &ok );
        if ( ok )
            return value;
        else
            qWarning() << "Could not convert " << value_str << " to form";
    }

    // Default heuristic.
    if ( firstSibling() == ( EgMmlNode* )this && lastSibling() != ( EgMmlNode* )this )
        return PrefixForm;
    else if ( lastSibling() == ( EgMmlNode* )this && firstSibling() != ( EgMmlNode* )this )
        return PostfixForm;
    else
        return InfixForm;
}

void EgMmlMoNode::stretch()
{
    if ( parent() == 0 )
        return;

    if ( m_oper_spec == 0 )
        return;

    if ( m_oper_spec->stretch_dir == EgMmlOperSpec::HStretch
            && parent()->nodeType() == EgMathMlNodeType::MrowNode
            && ( previousSibling() != 0 || nextSibling() != 0) )
        return;

    QRectF pmr = parent()->myRect();
    QRectF pr = parentRect();

    switch ( m_oper_spec->stretch_dir )
    {
        case EgMmlOperSpec::VStretch:
            stretchTo( QRectF( pr.left(), pmr.top(), pr.width(), pmr.height() ) );
            break;
        case EgMmlOperSpec::HStretch:
            stretchTo( QRectF( pmr.left(), pr.top(), pmr.width(), pr.height() ) );
            break;
        case EgMmlOperSpec::HVStretch:
            stretchTo( pmr );
            break;
        case EgMmlOperSpec::NoStretch:
            break;
    }
}

qreal EgMmlMoNode::lspace() const
{
    Q_ASSERT( m_oper_spec != 0 );
    if ( parent() == 0
            || ( parent()->nodeType() != EgMathMlNodeType::MrowNode
                 && parent()->nodeType() != EgMathMlNodeType::MfencedNode
                 && parent()->nodeType() != EgMathMlNodeType::UnknownNode )
            || previousSibling() == 0
            || ( previousSibling() == 0 && nextSibling() == 0 ) )
        return 0.0;
    else
        return interpretSpacing( dictionaryAttribute( "lspace" ), 0 );
}

qreal EgMmlMoNode::rspace() const
{
    Q_ASSERT( m_oper_spec != 0 );
    if ( parent() == 0
            || ( parent()->nodeType() != EgMathMlNodeType::MrowNode
                 && parent()->nodeType() != EgMathMlNodeType::MfencedNode
                 && parent()->nodeType() != EgMathMlNodeType::UnknownNode )
            || nextSibling() == 0
            || ( previousSibling() == 0 && nextSibling() == 0 ) )
        return 0.0;
    else
        return interpretSpacing( dictionaryAttribute( "rspace" ), 0 );
}

QRectF EgMmlMoNode::symbolRect() const
{
    if ( firstChild() == 0 )
        return QRectF( 0.0, 0.0, 0.0, 0.0 );

    QRectF cmr = firstChild()->myRect();

    return QRectF( -lspace(), cmr.top(),
                   cmr.width() + lspace() + rspace(), cmr.height() );
}

qreal EgMmlMtableNode::rowspacing() const
{
    QString value = explicitAttribute( "rowspacing" );
    if ( value.isNull() )
        return ex();
    bool ok;
    qreal spacing = interpretSpacing( value, &ok );

    if ( ok )
        return spacing;
    else
        return ex();
}

qreal EgMmlMtableNode::columnspacing() const
{
    QString value = explicitAttribute( "columnspacing" );
    if ( value.isNull() )
        return 0.8 * em();
    bool ok;
    qreal spacing = interpretSpacing( value, &ok );

    if ( ok )
        return spacing;
    else
        return 0.8 * em();
}

qreal EgMmlMtableNode::CellSizeData::colWidthSum() const
{
    qreal w = 0.0;
    for ( int i = 0; i < col_widths.count(); ++i )
        w += col_widths[i];
    return w;
}

qreal EgMmlMtableNode::CellSizeData::rowHeightSum() const
{
    qreal h = 0.0;
    for ( int i = 0; i < row_heights.count(); ++i )
        h += row_heights[i];
    return h;
}

void EgMmlMtableNode::CellSizeData::init( const EgMmlNode *first_row )
{
    col_widths.clear();
    row_heights.clear();

    const EgMmlNode *mtr = first_row;
    for ( ; mtr != 0; mtr = mtr->nextSibling() )
    {
        Q_ASSERT( mtr->nodeType() == EgMathMlNodeType::MtrNode );

        int col_cnt = 0;
        const EgMmlNode *mtd = mtr->firstChild();
        for ( ; mtd != 0; mtd = mtd->nextSibling(), ++col_cnt )
        {
            Q_ASSERT( mtd->nodeType() == EgMathMlNodeType::MtdNode );

            QRectF mtdmr = mtd->myRect();

            if ( col_cnt == col_widths.count() )
                col_widths.append( mtdmr.width() );
            else
                col_widths[col_cnt] = qMax( col_widths[col_cnt], mtdmr.width() );
        }

        row_heights.append( mtr->myRect().height() );
    }
}

static qreal mmlQstringToQreal( const QString &string, bool *ok )
{
    static const int SizeOfDouble = sizeof( double );

    if ( sizeof( qreal ) == SizeOfDouble )
        return string.toDouble( ok );
    else
        return string.toFloat( ok );
}

void EgMmlMtableNode::layoutSymbol()
{
    // Obtain natural widths of columns
    m_cell_size_data.init( firstChild() );

    qreal col_spc = columnspacing();
    qreal row_spc = rowspacing();
    qreal frame_spc_hor = framespacing_hor();
    QString columnwidth_attr = explicitAttribute( "columnwidth", "auto" );

    // Is table width set by user? If so, set col_width_sum and never ever change it.
    qreal col_width_sum = m_cell_size_data.colWidthSum();
    bool width_set_by_user = false;
    QString width_str = explicitAttribute( "width", "auto" );
    if ( width_str != "auto" )
    {
        bool ok;

        qreal w = interpretSpacing( width_str, &ok );
        if ( ok )
        {
            col_width_sum = w
                            - col_spc * ( m_cell_size_data.numCols() - 1 )
                            - frame_spc_hor * 2.0;
            width_set_by_user = true;
        }
    }

    // Find out what kind of columns we are dealing with and set the widths of
    // statically sized columns.
    qreal fixed_width_sum = 0.0;          // sum of widths of statically sized set columns
    qreal auto_width_sum = 0.0;           // sum of natural widths of auto sized columns
    qreal relative_width_sum = 0.0;       // sum of natural widths of relatively sized columns
    qreal relative_fraction_sum = 0.0;    // total fraction of width taken by relatively
    // sized columns
    int i;
    for ( i = 0; i < m_cell_size_data.numCols(); ++i )
    {
        QString value = mmlInterpretListAttr( columnwidth_attr, i, "auto" );

        // Is it an auto sized column?
        if ( value == "auto" || value == "fit" )
        {
            auto_width_sum += m_cell_size_data.col_widths[i];
            continue;
        }

        // Is it a statically sized column?
        bool ok;
        qreal w = interpretSpacing( value, &ok );
        if ( ok )
        {
            // Yup, sets its width to the user specified value
            m_cell_size_data.col_widths[i] = w;
            fixed_width_sum += w;
            continue;
        }

        // Is it a relatively sized column?
        if ( value.endsWith( "%" ) )
        {
            value.truncate( value.length() - 1 );
            qreal factor = mmlQstringToQreal( value, &ok );
            if ( ok && !value.isEmpty() )
            {
                factor *= 0.01;
                relative_width_sum += m_cell_size_data.col_widths[i];
                relative_fraction_sum += factor;
                if ( !width_set_by_user )
                {
                    // If the table width was not set by the user, we are free to increase
                    // it so that the width of this column will be >= than its natural width
                    qreal min_col_width_sum = m_cell_size_data.col_widths[i] / factor;
                    if ( min_col_width_sum > col_width_sum )
                        col_width_sum = min_col_width_sum;
                }
                continue;
            }
            else
                qWarning() << "EgMmlMtableNode::layoutSymbol(): could not parse value " << value << "%%";
        }

        // Relatively sized column, but we failed to parse the factor. Treat is like an auto
        // column.
        auto_width_sum += m_cell_size_data.col_widths[i];
    }

    // Work out how much space remains for the auto olumns, after allocating
    // the statically sized and the relatively sized columns.
    qreal required_auto_width_sum = col_width_sum
                                    - relative_fraction_sum * col_width_sum
                                    - fixed_width_sum;

    if ( !width_set_by_user && required_auto_width_sum < auto_width_sum )
    {
#if 1
        if ( relative_fraction_sum < 1.0 )
            col_width_sum = ( fixed_width_sum + auto_width_sum ) / ( 1.0 - relative_fraction_sum );
        else
            col_width_sum = fixed_width_sum + auto_width_sum + relative_width_sum;
#endif
        required_auto_width_sum = auto_width_sum;
    }

    // Ratio by which we have to shring/grow all auto sized columns to make it all fit
    qreal auto_width_scale = 1.0;
    if ( auto_width_sum > 0.0 )
        auto_width_scale = required_auto_width_sum / auto_width_sum;

    // Set correct sizes for the auto sized and the relatively sized columns.
    for ( i = 0; i < m_cell_size_data.numCols(); ++i )
    {
        QString value = mmlInterpretListAttr( columnwidth_attr, i, "auto" );

        // Is it a relatively sized column?
        if ( value.endsWith( "%" ) )
        {
            bool ok;
            qreal w = mmlInterpretPercentSpacing( value, col_width_sum, &ok );
            if ( ok )
            {
                m_cell_size_data.col_widths[i] = w;
            }
            else
            {
                // We're treating parsing errors here as auto sized columns
                m_cell_size_data.col_widths[i] = auto_width_scale * m_cell_size_data.col_widths[i];
            }
        }
        // Is it an auto sized column?
        else if ( value == "auto" )
        {
            m_cell_size_data.col_widths[i] = auto_width_scale * m_cell_size_data.col_widths[i];
        }
    }

    m_content_width = m_cell_size_data.colWidthSum()
                      + col_spc * ( m_cell_size_data.numCols() - 1 );
    m_content_height = m_cell_size_data.rowHeightSum()
                       + row_spc * ( m_cell_size_data.numRows() - 1 );

    qreal bottom = -0.5 * m_content_height;
    EgMmlNode *child = firstChild();
    for ( ; child != 0; child = child->nextSibling() )
    {
        Q_ASSERT( child->nodeType() == EgMathMlNodeType::MtrNode );
        EgMmlMtrNode *row = ( EgMmlMtrNode* ) child;

        row->layoutCells( m_cell_size_data.col_widths, col_spc );
        QRectF rmr = row->myRect();
        row->setRelOrigin( QPointF( 0.0, bottom - rmr.top() ) );
        bottom += rmr.height() + row_spc;
    }
}

QRectF EgMmlMtableNode::symbolRect() const
{
    qreal frame_spc_hor = framespacing_hor();
    qreal frame_spc_ver = framespacing_ver();

    return QRectF( -frame_spc_hor,
                   -0.5 * m_content_height - frame_spc_ver,
                   m_content_width + 2.0 * frame_spc_hor,
                   m_content_height + 2.0 * frame_spc_ver );
}

EgMml::FrameType EgMmlMtableNode::frame() const
{
    QString value = explicitAttribute( "frame", "none" );
    return mmlInterpretFrameType( value, 0, 0 );
}

EgMml::FrameType EgMmlMtableNode::columnlines( int idx ) const
{
    QString value = explicitAttribute( "columnlines", "none" );
    return mmlInterpretFrameType( value, idx, 0 );
}

EgMml::FrameType EgMmlMtableNode::rowlines( int idx ) const
{
    QString value = explicitAttribute( "rowlines", "none" );
    return mmlInterpretFrameType( value, idx, 0 );
}

void EgMmlMtableNode::paintSymbol(
    QPainter *painter, qreal x_scaling, qreal y_scaling ) const
{
    EgMmlNode::paintSymbol( painter, x_scaling, y_scaling );

    painter->save();

    painter->translate( devicePoint( QPointF() ) );

    QPen pen = painter->pen();

    FrameType frame_type = frame();
    if ( frame_type != FrameNone )
    {
        if ( frame_type == FrameDashed )
            pen.setStyle( Qt::DashLine );
        else
            pen.setStyle( Qt::SolidLine );
        painter->setPen( pen );
        painter->drawRect( myRect() );
    }

    qreal col_spc = columnspacing();
    qreal row_spc = rowspacing();

    qreal col_offset = 0.0;
    int i;
    for ( i = 0; i < m_cell_size_data.numCols() - 1; ++i )
    {
        FrameType frame_type = columnlines( i );
        col_offset += m_cell_size_data.col_widths[i];

        if ( frame_type != FrameNone )
        {
            if ( frame_type == FrameDashed )
                pen.setStyle( Qt::DashLine );
            else if ( frame_type == FrameSolid )
                pen.setStyle( Qt::SolidLine );

            painter->setPen( pen );
            qreal x = col_offset + 0.5 * col_spc;
            painter->drawLine( QPointF( x, -0.5 * m_content_height ),
                               QPointF( x,  0.5 * m_content_height ) );
        }
        col_offset += col_spc;
    }

    qreal row_offset = 0.0;
    for ( i = 0; i < m_cell_size_data.numRows() - 1; ++i )
    {
        FrameType frame_type = rowlines( i );
        row_offset += m_cell_size_data.row_heights[i];

        if ( frame_type != FrameNone )
        {
            if ( frame_type == FrameDashed )
                pen.setStyle( Qt::DashLine );
            else if ( frame_type == FrameSolid )
                pen.setStyle( Qt::SolidLine );

            painter->setPen( pen );
            qreal y = row_offset + 0.5 * ( row_spc - m_content_height );
            painter->drawLine( QPointF( 0, y ),
                               QPointF( m_content_width, y ) );
        }
        row_offset += row_spc;
    }

    painter->restore();
}

qreal EgMmlMtableNode::framespacing_ver() const
{
    if ( frame() == FrameNone )
        return 0.2 * em();

    QString value = explicitAttribute( "framespacing", "0.4em 0.5ex" );

    bool ok;
    FrameSpacing fs = mmlInterpretFrameSpacing( value, em(), ex(), &ok, baseFontPixelSize() );
    if ( ok )
        return fs.m_ver;
    else
        return 0.5 * ex();
}

qreal EgMmlMtableNode::framespacing_hor() const
{
    if ( frame() == FrameNone )
        return 0.2 * em();

    QString value = explicitAttribute( "framespacing", "0.4em 0.5ex" );

    bool ok;
    FrameSpacing fs = mmlInterpretFrameSpacing( value, em(), ex(), &ok, baseFontPixelSize() );
    if ( ok )
        return fs.m_hor;
    else
        return 0.4 * em();
}

void EgMmlMtrNode::layoutCells( const QList<qreal> &col_widths,
                                 qreal col_spc )
{
    const QRectF mr = myRect();

    EgMmlNode *child = firstChild();
    qreal col_offset = 0.0;
    int colnum = 0;
    for ( ; child != 0; child = child->nextSibling(), ++colnum )
    {
        Q_ASSERT( child->nodeType() == EgMathMlNodeType::MtdNode );
        EgMmlMtdNode *mtd = ( EgMmlMtdNode* ) child;

        QRectF rect = QRectF( 0.0, mr.top(), col_widths[colnum], mr.height() );
        mtd->setMyRect( rect );
        mtd->setRelOrigin( QPointF( col_offset, 0.0 ) );
        col_offset += col_widths[colnum] + col_spc;
    }

    updateMyRect();
}

int EgMmlMtdNode::scriptlevel( const EgMmlNode *child ) const
{
    int sl = EgMmlNode::scriptlevel();
    if ( child != 0 && child == firstChild() )
        return sl + m_scriptlevel_adjust;
    else
        return sl;
}

void EgMmlMtdNode::setMyRect( const QRectF &rect )
{
    EgMmlNode::setMyRect( rect );
    EgMmlNode *child = firstChild();
    if ( child == 0 )
        return;

    if ( rect.width() < child->myRect().width() )
    {
        while ( rect.width() < child->myRect().width()
                && static_cast<qreal>(child->font().pixelSize()) > g_min_font_pixel_size_calc )
        {
            qWarning() << "EgMmlMtdNode::setMyRect(): rect.width()=" << rect.width()
                       << ", child->myRect().width=" << child->myRect().width()
                       << " sl=" << m_scriptlevel_adjust;

            ++m_scriptlevel_adjust;
            child->layout();
        }

        qWarning() << "EgMmlMtdNode::setMyRect(): rect.width()=" << rect.width()
                   << ", child->myRect().width=" << child->myRect().width()
                   << " sl=" << m_scriptlevel_adjust;
    }

    QRectF mr = myRect();
    QRectF cmr = child->myRect();

    QPointF child_rel_origin;

    switch ( columnalign() )
    {
        case ColAlignLeft:
            child_rel_origin.setX( 0.0 );
            break;
        case ColAlignCenter:
            child_rel_origin.setX( mr.left() + 0.5 * ( mr.width() - cmr.width() ) );
            break;
        case ColAlignRight:
            child_rel_origin.setX( mr.right() - cmr.width() );
            break;
    }

    switch ( rowalign() )
    {
        case RowAlignTop:
            child_rel_origin.setY( mr.top() - cmr.top() );
            break;
        case RowAlignCenter:
        case RowAlignBaseline:
            child_rel_origin.setY( mr.top() - cmr.top() + 0.5 * ( mr.height() - cmr.height() ) );
            break;
        case RowAlignBottom:
            child_rel_origin.setY( mr.bottom() - cmr.bottom() );
            break;
        case RowAlignAxis:
            child_rel_origin.setY( 0.0 );
            break;
    }

    child->setRelOrigin( child_rel_origin );
}

int EgMmlMtdNode::colNum() const
{
    EgMmlNode *syb = previousSibling();

    int i = 0;
    for ( ; syb != 0; syb = syb->previousSibling() )
        ++i;

    return i;
}

int EgMmlMtdNode::rowNum() const
{
    EgMmlNode *row = parent()->previousSibling();

    int i = 0;
    for ( ; row != 0; row = row->previousSibling() )
        ++i;

    return i;
}

EgMmlMtdNode::ColAlign EgMmlMtdNode::columnalign()
{
    QString val = explicitAttribute( "columnalign" );
    if ( !val.isNull() )
        return mmlInterpretColAlign( val, 0, 0 );

    EgMmlNode *node = parent(); // <mtr>
    if ( node == 0 )
        return ColAlignCenter;

    int colnum = colNum();
    val = node->explicitAttribute( "columnalign" );
    if ( !val.isNull() )
        return mmlInterpretColAlign( val, colnum, 0 );

    node = node->parent(); // <mtable>
    if ( node == 0 )
        return ColAlignCenter;

    val = node->explicitAttribute( "columnalign" );
    if ( !val.isNull() )
        return mmlInterpretColAlign( val, colnum, 0 );

    return ColAlignCenter;
}

EgMmlMtdNode::RowAlign EgMmlMtdNode::rowalign()
{
    QString val = explicitAttribute( "rowalign" );
    if ( !val.isNull() )
        return mmlInterpretRowAlign( val, 0, 0 );

    EgMmlNode *node = parent(); // <mtr>
    if ( node == 0 )
        return RowAlignAxis;

    int rownum = rowNum();
    val = node->explicitAttribute( "rowalign" );
    if ( !val.isNull() )
        return mmlInterpretRowAlign( val, rownum, 0 );

    node = node->parent(); // <mtable>
    if ( node == 0 )
        return RowAlignAxis;

    val = node->explicitAttribute( "rowalign" );
    if ( !val.isNull() )
        return mmlInterpretRowAlign( val, rownum, 0 );

    return RowAlignAxis;
}

void EgMmlMoverNode::layoutSymbol()
{
    EgMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    EgMmlNode *over = base->nextSibling();
    Q_ASSERT( over != 0 );

    QRectF base_rect = base->myRect();
    QRectF over_rect = over->myRect();

    qreal spacing = explicitAttribute( "accent" ) == "true" ? 0.0 : g_mfrac_spacing * ( over_rect.height() + base_rect.height() );
    QString align_value = explicitAttribute( "align" );
    qreal over_rel_factor = align_value == "left" ? 1.0 : align_value == "right" ? 0.0 : 0.5;

    base->setRelOrigin( QPointF( -0.5 * base_rect.width(), 0.0 ) );
    over->setRelOrigin( QPointF( -over_rel_factor * over_rect.width(),
                                 base_rect.top() - spacing - over_rect.bottom() ) );
}

int EgMmlMoverNode::scriptlevel( const EgMmlNode *node ) const
{
    EgMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    EgMmlNode *over = base->nextSibling();
    Q_ASSERT( over != 0 );

    int sl = EgMmlNode::scriptlevel();
    if ( node != 0 && node == over )
        return sl + 1;
    else
        return sl;
}

void EgMmlMunderNode::layoutSymbol()
{
    EgMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    EgMmlNode *under = base->nextSibling();
    Q_ASSERT( under != 0 );

    QRectF base_rect = base->myRect();
    QRectF under_rect = under->myRect();

    qreal spacing = explicitAttribute( "accentunder" ) == "true" ? 0.0 : g_mfrac_spacing * ( under_rect.height() + base_rect.height() );
    QString align_value = explicitAttribute( "align" );
    qreal under_rel_factor = align_value == "left" ? 1.0 : align_value == "right" ? 0.0 : 0.5;

    base->setRelOrigin( QPointF( -0.5 * base_rect.width(), 0.0 ) );
    under->setRelOrigin( QPointF( -under_rel_factor * under_rect.width(), base_rect.bottom() + spacing - under_rect.top() ) );
}

int EgMmlMunderNode::scriptlevel( const EgMmlNode *node ) const
{
    EgMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    EgMmlNode *under = base->nextSibling();
    Q_ASSERT( under != 0 );

    int sl = EgMmlNode::scriptlevel();
    if ( node != 0 && node == under )
        return sl + 1;
    else
        return sl;
}

void EgMmlMunderoverNode::layoutSymbol()
{
    EgMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    EgMmlNode *under = base->nextSibling();
    Q_ASSERT( under != 0 );
    EgMmlNode *over = under->nextSibling();
    Q_ASSERT( over != 0 );

    QRectF base_rect = base->myRect();
    QRectF under_rect = under->myRect();
    QRectF over_rect = over->myRect();

    qreal over_spacing = explicitAttribute( "accent" ) == "true" ? 0.0 : g_mfrac_spacing * ( base_rect.height() + under_rect.height() + over_rect.height() );
    qreal under_spacing = explicitAttribute( "accentunder" ) == "true" ? 0.0 : g_mfrac_spacing * ( base_rect.height() + under_rect.height() + over_rect.height() );
    QString align_value = explicitAttribute( "align" );
    qreal underover_rel_factor = align_value == "left" ? 1.0 : align_value == "right" ? 0.0 : 0.5;

    base->setRelOrigin( QPointF( -0.5 * base_rect.width(), 0.0 ) );
    under->setRelOrigin( QPointF( -underover_rel_factor * under_rect.width(), base_rect.bottom() + under_spacing - under_rect.top() ) );
    over->setRelOrigin( QPointF( -underover_rel_factor * over_rect.width(), base_rect.top() - over_spacing - under_rect.bottom() ) );
}

int EgMmlMunderoverNode::scriptlevel( const EgMmlNode *node ) const
{
    EgMmlNode *base = firstChild();
    Q_ASSERT( base != 0 );
    EgMmlNode *under = base->nextSibling();
    Q_ASSERT( under != 0 );
    EgMmlNode *over = under->nextSibling();
    Q_ASSERT( over != 0 );

    int sl = EgMmlNode::scriptlevel();
    if ( node != 0 && ( node == under || node == over ) )
        return sl + 1;
    else
        return sl;
}

qreal EgMmlSpacingNode::interpretSpacing( QString value, qreal base_value,
                                           bool *ok ) const
{
    if ( ok != 0 )
        *ok = false;

    value.replace( ' ', "" );

    QString sign, factor_str, pseudo_unit;
    bool percent = false;

    // extract the sign
    int idx = 0;
    if ( idx < value.length() && ( value.at( idx ) == '+' || value.at( idx ) == '-' ) )
        sign = value.at( idx++ );

    // extract the factor
    while ( idx < value.length() && ( value.at( idx ).isDigit() || value.at( idx ) == '.' ) )
        factor_str.append( value.at( idx++ ) );

    if ( factor_str == "" )
        factor_str = "1.0";

    // extract the % sign
    if ( idx < value.length() && value.at( idx ) == '%' )
    {
        percent = true;
        ++idx;
    }

    // extract the pseudo-unit
    pseudo_unit = value.mid( idx );

    bool qreal_ok;
    qreal factor = mmlQstringToQreal( factor_str, &qreal_ok );
    if ( !qreal_ok || factor < 0.0 )
    {
        qWarning( "EgMmlSpacingNode::interpretSpacing(): could not parse \"%s\"", qPrintable( value ) );
        return 0.0;
    }

    if ( percent )
        factor *= 0.01;

    QRectF cr;
    if ( firstChild() == 0 )
        cr = QRectF( 0.0, 0.0, 0.0, 0.0 );
    else
        cr = firstChild()->myRect();

    qreal unit_size;

    if ( pseudo_unit.isEmpty() )
        unit_size = base_value;
    else if ( pseudo_unit == "width" )
        unit_size = cr.width();
    else if ( pseudo_unit == "height" )
        unit_size = -cr.top();
    else if ( pseudo_unit == "depth" )
        unit_size = cr.bottom();
    else
    {
        bool unit_ok;

        if (    pseudo_unit == "em" || pseudo_unit == "ex"
             || pseudo_unit == "cm" || pseudo_unit == "mm"
             || pseudo_unit == "in" || pseudo_unit == "px" )
            unit_size = EgMmlNode::interpretSpacing( "1" + pseudo_unit, &unit_ok );
        else
            unit_size = EgMmlNode::interpretSpacing( pseudo_unit, &unit_ok );

        if ( !unit_ok )
        {
            qWarning( "EgMmlSpacingNode::interpretSpacing(): could not parse \"%s\"", qPrintable( value ) );
            return 0.0;
        }
    }

    if ( ok != 0 )
        *ok = true;

    if ( sign.isNull() )
        return factor * unit_size;
    else if ( sign == "+" )
        return base_value + factor * unit_size;
    else // sign == "-"
        return base_value - factor * unit_size;
}

qreal EgMmlSpacingNode::width() const
{
    qreal child_width = 0.0;
    if ( firstChild() != 0 )
        child_width = firstChild()->myRect().width();

    QString value = explicitAttribute( "width" );
    if ( value.isNull() )
        return child_width;

    bool ok;
    qreal w = interpretSpacing( value, child_width, &ok );
    if ( ok )
        return w;

    return child_width;
}

qreal EgMmlSpacingNode::height() const
{
    QRectF cr;
    if ( firstChild() == 0 )
        cr = QRectF( 0.0, 0.0, 0.0, 0.0 );
    else
        cr = firstChild()->myRect();

    QString value = explicitAttribute( "height" );
    if ( value.isNull() )
        return -cr.top();

    bool ok;
    qreal h = interpretSpacing( value, -cr.top(), &ok );
    if ( ok )
        return h;

    return -cr.top();
}

qreal EgMmlSpacingNode::depth() const
{
    QRectF cr;
    if ( firstChild() == 0 )
        cr = QRectF( 0.0, 0.0, 0.0, 0.0 );
    else
        cr = firstChild()->myRect();

    QString value = explicitAttribute( "depth" );
    if ( value.isNull() )
        return cr.bottom();

    bool ok;
    qreal h = interpretSpacing( value, cr.bottom(), &ok );
    if ( ok )
        return h;

    return cr.bottom();
}

void EgMmlSpacingNode::layoutSymbol()
{
    if ( firstChild() == 0 )
        return;

    firstChild()->setRelOrigin( QPointF( 0.0, 0.0 ) );
}


QRectF EgMmlSpacingNode::symbolRect() const
{
    return QRectF( 0.0, -height(), width(), height() + depth() );
}

qreal EgMmlMpaddedNode::lspace() const
{
    QString value = explicitAttribute( "lspace" );

    if ( value.isNull() )
        return 0.0;

    bool ok;
    qreal lspace = interpretSpacing( value, 0.0, &ok );

    if ( ok )
        return lspace;

    return 0.0;
}

QRectF EgMmlMpaddedNode::symbolRect() const
{
    return QRectF( -lspace(), -height(), lspace() + width(), height() + depth() );
}

// *******************************************************************
// Static helper functions
// *******************************************************************

static qreal mmlInterpretSpacing(QString value, qreal em, qreal ex, bool *ok, qreal fontsize, qreal MmToPixFactor)
{
    if ( ok != 0 )
        *ok = true;

    if ( value == "thin" )
            return fontsize * 0.0666;

    if ( value == "medium" )
        return fontsize * 0.1333;

    if ( value == "thick" )
        return fontsize * 0.2;

    struct HSpacingValue
    {
        QString name;
        qreal factor;
    };

    static const HSpacingValue g_h_spacing_data[] =
    {
        { "veryverythinmathspace",  0.0555556 },
        { "verythinmathspace",      0.111111  },
        { "thinmathspace",          0.166667  },
        { "mediummathspace",        0.222222  },
        { "thickmathspace",         0.277778  },
        { "verythickmathspace",     0.333333  },
        { "veryverythickmathspace", 0.388889  },
        { 0,                        0.0       }
    };

    const HSpacingValue *v = g_h_spacing_data;
    for ( ; v->name != 0; ++v )
    {
        if ( value == v->name )
            return em * v->factor;
    }

    if ( value.endsWith( "em" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
            return em * factor;
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%sem\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "ex" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
            return ex * factor;
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%sex\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "cm" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
        {
            return factor * 10.0 * MmToPixFactor;
        }
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%scm\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "mm" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
        {
            return factor * MmToPixFactor;
        }
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%smm\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "in" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        qreal factor = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && factor >= 0.0 )
        {
            return factor * 10.0 * MmToPixFactor * 2.54;
        }
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%sin\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    if ( value.endsWith( "px" ) )
    {
        value.truncate( value.length() - 2 );
        bool qreal_ok;
        int i = mmlQstringToQreal( value, &qreal_ok );
        if ( qreal_ok && i >= 0 )
            return i;
        else
        {
            qWarning( "interpretSpacing(): could not parse \"%spx\"", qPrintable( value ) );
            if ( ok != 0 )
                *ok = false;
            return 0.0;
        }
    }

    bool qreal_ok;
    int i = mmlQstringToQreal( value, &qreal_ok );
    if ( qreal_ok && i >= 0 )
        return i;

    qWarning( "interpretSpacing(): could not parse \"%s\"", qPrintable( value ) );
    if ( ok != 0 )
        *ok = false;
    return 0.0;
}

static qreal mmlInterpretPercentSpacing(
    QString value, qreal base, bool *ok )
{
    if ( !value.endsWith( "%" ) )
    {
        if ( ok != 0 )
            *ok = false;
        return 0.0;
    }

    value.truncate( value.length() - 1 );
    bool qreal_ok;
    qreal factor = mmlQstringToQreal( value, &qreal_ok );
    if ( qreal_ok && factor >= 0.0 )
    {
        if ( ok != 0 )
            *ok = true;
        return 0.01 * base * factor;
    }

    qWarning( "interpretPercentSpacing(): could not parse \"%s%%\"", qPrintable( value ) );
    if ( ok != 0 )
        *ok = false;
    return 0.0;
}

static qreal mmlInterpretPointSize( QString value, bool *ok )
{
    if ( !value.endsWith( "pt" ) )
    {
        if ( ok != 0 )
            *ok = false;
        return 0;
    }

    value.truncate( value.length() - 2 );
    bool qreal_ok;
    qreal pt_size = mmlQstringToQreal( value, &qreal_ok );
    if ( qreal_ok && pt_size > 0.0 )
    {
        if ( ok != 0 )
            *ok = true;
        return pt_size * EgMmlDocument::MmToPixelFactor() * 0.35275;  //now we have the pixel factor
    }

    qWarning( "interpretPointSize(): could not parse \"%spt\"", qPrintable( value ) );
    if ( ok != 0 )
        *ok = false;
    return 0.0;
}

static qreal mmlInterpretPixelSize( QString value, bool *ok )
{
    if ( !value.endsWith( "px" ) )
    {
        if ( ok != 0 )
            *ok = false;
        return 0;
    }

    value.truncate( value.length() - 2 );
    bool int_ok;
    qreal px_size = mmlQstringToQreal( value, &int_ok );
    if ( int_ok && px_size > 0.0 )
    {
        if ( ok != 0 )
            *ok = true;
        return px_size;
    }

    qWarning( "interpretPixelSize(): could not parse \"%spx\"", qPrintable( value ) );
    if ( ok != 0 )
        *ok = false;
    return 0.0;
}

static const EgMmlNodeSpec *mmlFindNodeSpec( EgMathMlNodeType type )
{
    const EgMmlNodeSpec *spec = g_node_spec_data;
    for ( ; spec->type != EgMathMlNodeType::NoNode; ++spec )
    {
        if ( type == spec->type ) return spec;
    }
    return 0;
}

static const EgMmlNodeSpec *mmlFindNodeSpec( const QString &tag )
{
    const EgMmlNodeSpec *spec = g_node_spec_data;
    for ( ; spec->type != EgMathMlNodeType::NoNode; ++spec )
    {
        if ( tag == spec->tag ) return spec;
    }
    return 0;
}

static bool mmlCheckChildType( EgMathMlNodeType parent_type,
                               EgMathMlNodeType child_type,
                               QString *error_str )
{
    if ( parent_type == EgMathMlNodeType::UnknownNode || child_type == EgMathMlNodeType::UnknownNode )
        return true;

    const EgMmlNodeSpec *child_spec = mmlFindNodeSpec( child_type );
    const EgMmlNodeSpec *parent_spec = mmlFindNodeSpec( parent_type );

    Q_ASSERT( parent_spec != 0 );
    Q_ASSERT( child_spec != 0 );

    QString allowed_child_types( parent_spec->child_types );
    // null list means any child type is valid
    if ( allowed_child_types.isNull() )
        return true;

    QString child_type_str = QString( " " ) + child_spec->type_str + " ";
    if ( !allowed_child_types.contains( child_type_str ) )
    {
        if ( error_str != 0 )
            *error_str = QString( "illegal child " )
                         + child_spec->type_str
                         + " for parent "
                         + parent_spec->type_str;
        return false;
    }

    return true;
}

static bool mmlCheckAttributes( EgMathMlNodeType child_type,
                                const EgMmlAttributeMap &attr,
                                QString *error_str )
{
    const EgMmlNodeSpec *spec = mmlFindNodeSpec( child_type );
    Q_ASSERT( spec != 0 );

    QString allowed_attr( spec->attributes );
    // empty list means any attr is valid
    if ( allowed_attr.isEmpty() )
        return true;

    EgMmlAttributeMap::const_iterator it = attr.begin(), end = attr.end();
    for ( ; it != end; ++it )
    {
        QString name = it.key();

        if ( name.indexOf( ':' ) != -1 )
            continue;

        QString padded_name = " " + name + " ";
        if ( !allowed_attr.contains( padded_name ) )
        {
            if ( error_str != 0 )
                *error_str = QString( "illegal attribute " )
                             + name
                             + " in "
                             + spec->type_str;
            return false;
        }
    }

    return true;
}

static int attributeIndex( const QString &name )
{
    for ( int i = 0; i < g_oper_spec_rows; ++i )
    {
        if ( name == g_oper_spec_names[i] )
            return i;
    }
    return -1;
}

struct OperSpecSearchResult
{
    OperSpecSearchResult() { prefix_form = infix_form = postfix_form = 0; }

    const EgMmlOperSpec *prefix_form,
                         *infix_form,
                         *postfix_form;

    const EgMmlOperSpec *&getForm( EgMml::FormType form );
    bool haveForm( const EgMml::FormType &form ) { return getForm( form ) != 0; }
    void addForm( const EgMmlOperSpec *spec ) { getForm( spec->form ) = spec; }
};

const EgMmlOperSpec *&OperSpecSearchResult::getForm( EgMml::FormType form )
{
    switch ( form )
    {
        case EgMml::PrefixForm:
            return prefix_form;
        case EgMml::InfixForm:
            return infix_form;
        case EgMml::PostfixForm:
            return postfix_form;
    }
    return postfix_form; // just to avoid warning
}

/*
    Searches g_oper_spec_data and returns any instance of operator name. There may
    be more instances, but since the list is sorted, they will be next to each other.
*/
static const EgMmlOperSpec *searchOperSpecData( const QString &name )
{
    // binary search
    // establish invariant g_oper_spec_data[begin].name < name < g_oper_spec_data[end].name

    int cmp = name.compare( g_oper_spec_data[0].name );
    if ( cmp < 0 )
        return 0;

    if ( cmp == 0 )
        return g_oper_spec_data;

    int begin = 0;
    int end = g_oper_spec_count;

    // invariant holds
    while ( end - begin > 1 )
    {
        int mid = 0.5 * ( begin + end );

        const EgMmlOperSpec *spec = g_oper_spec_data + mid;
        int cmp = name.compare( spec->name );
        if ( cmp < 0 )
            end = mid;
        else if ( cmp > 0 )
            begin = mid;
        else
            return spec;
    }

    return 0;
}

/*
    This searches g_oper_spec_data until at least one name in name_list is found with FormType form,
    or until name_list is exhausted. The idea here is that if we don't find the operator in the
    specified form, we still want to use some other available form of that operator.
*/
static OperSpecSearchResult _mmlFindOperSpec( const QStringList &name_list,
                                              EgMml::FormType form )
{
    OperSpecSearchResult result;

    QStringList::const_iterator it = name_list.begin();
    for ( ; it != name_list.end(); ++it )
    {
        const QString &name = *it;

        const EgMmlOperSpec *spec = searchOperSpecData( name );

        if ( spec == 0 )
            continue;

        // backtrack to the first instance of name
        while ( spec > g_oper_spec_data && ( spec - 1 )->name.compare( name ) == 0 )
            --spec;

        // iterate over instances of name until the instances are exhausted or until we
        // find an instance in the specified form.
        do
        {
            result.addForm( spec++ );
            if ( result.haveForm( form ) )
                break;
        }
        while ( spec->name.compare( name ) == 0 );

        if ( result.haveForm( form ) )
            break;
    }

    return result;
}

/*
    text is a string between <mo> and </mo>. It can be a character ('+'), an
    entity reference ("&infin;") or a character reference ("&#x0221E"). Our
    job is to find an operator spec in the operator dictionary (g_oper_spec_data)
    that matches text. Things are further complicated by the fact, that many
    operators come in several forms (prefix, infix, postfix).

    If available, this function returns an operator spec matching text in the specified
    form. If such operator is not available, returns an operator spec that matches
    text, but of some other form in the preference order specified by the MathML spec.
    If that's not available either, returns the default operator spec.
*/
static const EgMmlOperSpec *mmlFindOperSpec( const QString &text,
                                              EgMml::FormType form )
{
    QStringList name_list;
    name_list.append( text );

    // First, just try to find text in the operator dictionary.
    OperSpecSearchResult result = _mmlFindOperSpec( name_list, form );

    if ( !result.haveForm( form ) )
    {
        // Try to find other names for the operator represented by text.

        QList<QString> nameList;
        nameList = mmlEntityTable.search(text);
        foreach (const QString &name, nameList) {
            name_list.append( '&' + QString( name ) + ';' );
        }

        result = _mmlFindOperSpec( name_list, form );
    }

    const EgMmlOperSpec *spec = result.getForm( form );
    if ( spec != 0 )
        return spec;

    spec = result.getForm( EgMml::InfixForm );
    if ( spec != 0 )
        return spec;

    spec = result.getForm( EgMml::PostfixForm );
    if ( spec != 0 )
        return spec;

    spec = result.getForm( EgMml::PrefixForm );
    if ( spec != 0 )
        return spec;

    return &g_oper_spec_defaults;
}

static QString mmlDictAttribute( const QString &name, const EgMmlOperSpec *spec )
{
    int i = attributeIndex( name );
    if ( i == -1 )
        return QString::null;
    else
        return spec->attributes[i];
}

static int mmlInterpretMathVariant( const QString &value, bool *ok )
{
    struct MathVariantValue
    {
        QString value;
        int mv;
    };

    static const MathVariantValue g_mv_data[] =
    {
        { "normal",                 EgMml::NormalMV                                        },
        { "bold",                   EgMml::BoldMV                                          },
        { "italic",                 EgMml::ItalicMV                                        },
        { "bold-italic",            EgMml::BoldMV | EgMml::ItalicMV                       },
        { "double-struck",          EgMml::DoubleStruckMV                                  },
        { "bold-fraktur",           EgMml::BoldMV | EgMml::FrakturMV                      },
        { "script",                 EgMml::ScriptMV                                        },
        { "bold-script",            EgMml::BoldMV | EgMml::ScriptMV                       },
        { "fraktur",                EgMml::FrakturMV                                       },
        { "sans-serif",             EgMml::SansSerifMV                                     },
        { "bold-sans-serif",        EgMml::BoldMV | EgMml::SansSerifMV                    },
        { "sans-serif-italic",      EgMml::SansSerifMV | EgMml::ItalicMV                  },
        { "sans-serif-bold-italic", EgMml::SansSerifMV | EgMml::ItalicMV | EgMml::BoldMV },
        { "monospace",              EgMml::MonospaceMV                                     },
        { 0,                        0                                                       }
    };

    const MathVariantValue *v = g_mv_data;
    for ( ; v->value != 0; ++v )
    {
        if ( value == v->value )
        {
            if ( ok != 0 )
                *ok = true;
            return v->mv;
        }
    }

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretMathVariant(): could not parse value: \"%s\"", qPrintable( value ) );

    return EgMml::NormalMV;
}

static EgMml::FormType mmlInterpretForm( const QString &value, bool *ok )
{
    if ( ok != 0 )
        *ok = true;

    if ( value == "prefix" )
        return EgMml::PrefixForm;
    if ( value == "infix" )
        return EgMml::InfixForm;
    if ( value == "postfix" )
        return EgMml::PostfixForm;

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretForm(): could not parse value \"%s\"", qPrintable( value ) );
    return EgMml::InfixForm;
}

static EgMml::ColAlign mmlInterpretColAlign(
    const QString &value_list, int colnum, bool *ok )
{
    QString value = mmlInterpretListAttr( value_list, colnum, "center" );

    if ( ok != 0 )
        *ok = true;

    if ( value == "left" )
        return EgMml::ColAlignLeft;
    if ( value == "right" )
        return EgMml::ColAlignRight;
    if ( value == "center" )
        return EgMml::ColAlignCenter;

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretColAlign(): could not parse value \"%s\"", qPrintable( value ) );
    return EgMml::ColAlignCenter;
}

static EgMml::RowAlign mmlInterpretRowAlign(
    const QString &value_list, int rownum, bool *ok )
{
    QString value = mmlInterpretListAttr( value_list, rownum, "axis" );

    if ( ok != 0 )
        *ok = true;

    if ( value == "top" )
        return EgMml::RowAlignTop;
    if ( value == "center" )
        return EgMml::RowAlignCenter;
    if ( value == "bottom" )
        return EgMml::RowAlignBottom;
    if ( value == "baseline" )
        return EgMml::RowAlignBaseline;
    if ( value == "axis" )
        return EgMml::RowAlignAxis;

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretRowAlign(): could not parse value \"%s\"", qPrintable( value ) );
    return EgMml::RowAlignAxis;
}

static QString mmlInterpretListAttr(
    const QString &value_list, int idx, const QString &def )
{
    QStringList l = value_list.split( ' ' );

    if ( l.count() == 0 )
        return def;

    if ( l.count() <= idx )
        return l[l.count() - 1];
    else
        return l[idx];
}

static EgMml::FrameType mmlInterpretFrameType(
    const QString &value_list, int idx, bool *ok )
{
    if ( ok != 0 )
        *ok = true;

    QString value = mmlInterpretListAttr( value_list, idx, "none" );

    if ( value == "none" )
        return EgMml::FrameNone;
    if ( value == "solid" )
        return EgMml::FrameSolid;
    if ( value == "dashed" )
        return EgMml::FrameDashed;

    if ( ok != 0 )
        *ok = false;

    qWarning( "interpretFrameType(): could not parse value \"%s\"", qPrintable( value ) );
    return EgMml::FrameNone;
}

static EgMml::FrameSpacing mmlInterpretFrameSpacing(const QString &value_list, qreal em, qreal ex, bool *ok , const qreal fontsize)
{
    EgMml::FrameSpacing fs;

    QStringList l = value_list.split( ' ' );
    if ( l.count() != 2 )
    {
        qWarning( "interpretFrameSpacing: could not parse value \"%s\"", qPrintable( value_list ) );
        if ( ok != 0 )
            *ok = false;
        return EgMml::FrameSpacing( 0.4 * em, 0.5 * ex );
    }

    bool hor_ok, ver_ok;
    fs.m_hor = mmlInterpretSpacing( l[0], em, ex, &hor_ok, fontsize, EgMmlDocument::MmToPixelFactor() );
    fs.m_ver = mmlInterpretSpacing( l[1], em, ex, &ver_ok, fontsize, EgMmlDocument::MmToPixelFactor() );

    if ( ok != 0 )
        *ok = hor_ok && ver_ok;

    return fs;
}

static QFont mmlInterpretDepreciatedFontAttr(
    const EgMmlAttributeMap &font_attr, QFont &fn, qreal em, qreal ex, const qreal fontsize )
{
    if ( font_attr.contains( "fontsize" ) )
    {
        QString value = font_attr["fontsize"];

        for ( ;; )
        {
            bool ok;
            qreal pxsize = mmlInterpretPointSize( value, &ok );
            if ( ok )
            {
                fn.setPixelSize( static_cast<int>(pxsize) );
                break;
            }

            pxsize = mmlInterpretPercentSpacing( value, fn.pixelSize(), &ok );
            if ( ok )
            {
                fn.setPixelSize( static_cast<int>(pxsize) );
                break;
            }

            qreal size = mmlInterpretSpacing( value, em, ex, &ok, fontsize, EgMmlDocument::MmToPixelFactor() );
            if ( ok )
            {
#if 1
                fn.setPixelSize( size );
#endif
                break;
            }

            break;
        }
    }

    if ( font_attr.contains( "fontweight" ) )
    {
        QString value = font_attr["fontweight"];
        if ( value == "normal" )
            fn.setBold( false );
        else if ( value == "bold" )
            fn.setBold( true );
        else
            qWarning( "interpretDepreciatedFontAttr(): could not parse fontweight \"%s\"", qPrintable( value ) );
    }

    if ( font_attr.contains( "fontstyle" ) )
    {
        QString value = font_attr["fontstyle"];
        if ( value == "normal" )
            fn.setItalic( false );
        else if ( value == "italic" )
            fn.setItalic( true );
        else
            qWarning( "interpretDepreciatedFontAttr(): could not parse fontstyle \"%s\"", qPrintable( value ) );
    }

    if ( font_attr.contains( "fontfamily" ) )
    {
        QString value = font_attr["fontfamily"];
        fn.setFamily( value );
    }

    return fn;
}

static QFont mmlInterpretMathSize(const QString &value, QFont &fn, qreal em, qreal ex, bool *ok, const qreal fontsize)
{
    if ( ok != 0 )
        *ok = true;

    if ( value == "small" )
    {
            fn.setPixelSize(static_cast<int>(static_cast<qreal>(fn.pixelSize()) * 0.7));

            return fn;
    }

    if ( value == "normal" )
        return fn;

    if ( value == "big" )
    {
        fn.setPixelSize(static_cast<int>(static_cast<qreal>(fn.pixelSize()) * 1.5));

        return fn;
    }

    bool size_ok;

    qreal pxsize = mmlInterpretPointSize( value, &size_ok );
    if ( size_ok )
    {
        fn.setPixelSize( static_cast<int>(pxsize) );
        return fn;
    }

    pxsize = mmlInterpretPixelSize( value, &size_ok );
    if ( size_ok )
    {
        fn.setPixelSize( static_cast<int>(pxsize) );
        return fn;
    }

    qreal size = mmlInterpretSpacing( value, em, ex, &size_ok, fontsize, EgMmlDocument::MmToPixelFactor()  );
    if ( size_ok )
    {
        fn.setPixelSize( static_cast<int>(size));

        return fn;
    }

    if ( ok != 0 )
        *ok = false;
    qWarning( "interpretMathSize(): could not parse mathsize \"%s\"", qPrintable( value ) );
    return fn;
}

/*!
    \class EgMathMLDocument

    \brief The EgMathMLDocument class renders mathematical formulas written in MathML 2.0.
*/

/*!
  Constructs an empty MML document.
*/
EgMathMLDocument::EgMathMLDocument() : m_size(QSizeF(0.0, 0.0))
{
        m_doc = new EgMmlDocument(this);
}

/*!
  Destroys the MML document.
*/
EgMathMLDocument::~EgMathMLDocument()
{
        delete m_doc;
}

/*!
    Clears the contents of this MML document.
*/
void EgMathMLDocument::clear()
{
        m_doc->clear();
        m_size = QSizeF(0.0, 0.0);
}

/*!
    Sets the MathML expression to be rendered. The expression is given
    in the string \a text. If the expression is successfully parsed,
    this method returns true; otherwise it returns false. If an error
    occured \a errorMsg is set to a diagnostic message, while \a
    errorLine and \a errorColumn contain the location of the error.
    Any of \a errorMsg, \a errorLine and \a errorColumn may be 0,
    in which case they are not set.

    \a text should contain MathML 2.0 presentation markup elements enclosed
    in a <math> element.
*/
bool EgMathMLDocument::setContent( const QString &text, QString *errorMsg,
                                    int *errorLine, int *errorColumn )
{
        m_size = QSizeF(0.0, 0.0);
        return m_doc->setContent( text, errorMsg, errorLine, errorColumn );
}

/*!
  Renders this MML document with the painter \a painter at position \a pos.
*/
void EgMathMLDocument::paint( QPainter *painter, const QPointF &pos ) const
{
        m_doc->paint( painter, pos );
        if (!m_doc->renderingComplete()) {
                m_doc->doPostProcessing();
        }
        //clear temporary rendering infos, since they are not needed anymore
        m_doc->optimizeSize();
        m_doc->setRenderingComplete();
}

/*!
    Returns the size of this MML document, as rendered, in pixels.
*/
QSizeF EgMathMLDocument::size()
{
        if (m_size.isNull())
                m_size = m_doc->size();

        return m_size;
}

/*!
    Returns the name of the font used to render the font \a type.

    \sa setFontName()  setBaseFontPixelSize() baseFontPixelSize() EgMathMLDocument::MmlFont
*/
QString EgMathMLDocument::fontName( EgMathMLDocument::MmlFont type ) const
{
        return m_doc->fontName( type );
}

/*!
    Sets the name of the font used to render the font \a type to \a name.

    \sa fontName() setBaseFontPixelSize() baseFontPixelSize() EgMathMLDocument::MmlFont
*/
void EgMathMLDocument::setFontName( EgMathMLDocument::MmlFont type,
                                     const QString &name )
{
        m_size = QSizeF(0.0, 0.0);

        if ( name == m_doc->fontName( type ) )
        return;

        m_doc->setFontName( type, name );
        m_doc->layout();
}

/*!
    Returns the point size of the font used to render expressions
    which scriptlevel is 0.

    \sa setBaseFontPixelSize() fontName() setFontName()
*/
qreal EgMathMLDocument::baseFontPixelSize()
{
        return m_doc->baseFontPixelSize();
}

/*!
    Sets the point \a size of the font used to render expressions
    which scriptlevel is 0.

    \sa baseFontPixelSize() fontName() setFontName()
*/
void EgMathMLDocument::setBaseFontPixelSize( qreal size )
{
        m_size = QSizeF(0.0, 0.0);

        if ( size < g_min_font_pixel_size_calc )
        size = g_min_font_pixel_size_calc;

        if ( size == m_doc->baseFontPixelSize() )
        return;

        m_doc->setBaseFontPixelSize( size );
        m_doc->layout();
}

/*!
    Returns the color used to render expressions.
*/
QColor EgMathMLDocument::foregroundColor() const
{
        return m_doc->foregroundColor();
}

/*!
    Sets the color used to render expressions.
*/
void EgMathMLDocument::setForegroundColor( const QColor &color )
{
        if ( color == m_doc->foregroundColor() )
        return;

        m_doc->setForegroundColor( color );
}

/*!
    Returns the color used to render the background of expressions.
*/
QColor EgMathMLDocument::backgroundColor() const
{
        return m_doc->backgroundColor();
}

/*!
    Sets the color used to render the background of expressions.
*/
void EgMathMLDocument::setBackgroundColor( const QColor &color )
{
        if ( color == m_doc->backgroundColor() )
        return;

        m_doc->setBackgroundColor( color );
}

/*!
    Returns whether frames are to be drawn.
*/
bool EgMathMLDocument::drawFrames() const
{
        return m_doc->drawFrames();
}

/*!
    Specifies whether frames are to be drawn.
*/
void EgMathMLDocument::setDrawFrames( bool drawFrames )
{
        if ( drawFrames == m_doc->drawFrames() )
        return;

        m_doc->setDrawFrames( drawFrames );
}

QVector<EgRenderingPosition> EgMathMLDocument::getRenderingPositions(void)
{
        return m_doc->getRenderingPositions();
}
