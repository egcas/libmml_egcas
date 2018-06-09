#include "mainwindow.h"

#include <qapplication.h>

int main( int argc, char **argv )
{
    QApplication a( argc, argv );

    MainWindow w;
    w.resize( 1024, 800 );
    w.show();

    return a.exec();
}
