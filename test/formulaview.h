#ifndef _FORMULA_VIEW_H_
#define _FORMULA_VIEW_H_

#include <qwidget.h>
#include <QRectF>
#include "../library/eg_mml_document.h"

class QPainter;

#define USE_FORMULA_SIGNAL

class FormulaView: public QWidget
{
    Q_OBJECT

public:
    FormulaView( QWidget *parent = NULL );

    QString formula() const;

public Q_SLOTS:
    void setFormula( const QString & );
    void setFontSize( const qreal & );
    void setTransformation( const bool &transformation );
    void setScale( const bool &scale );
    void setRotation( const qreal & );
    void setDrawFrames( const bool &drawFrames );
    void setColors( const bool &colors );
    void setIdRects( const bool &idRects );

protected:
    virtual void paintEvent( QPaintEvent * );

private:
    void renderFormula( QPainter * ) const;
    void paintIdRects(EgMathMLDocument *doc, QPainter* painter) const;

private:
    QString d_formula;
    qreal d_fontSize;
    bool d_transformation;
    bool d_scale;
    qreal d_rotation;
    bool d_drawFrames;
    bool d_colors;
    bool d_idRects;
};

#endif
