TEMPLATE = app
TARGET   = mathmlview

CONFIG += qt 
#CONFIG += no_keywords
CONFIG += warn_on

QT += xml
QT += widgets

LIBS += -legcas
LIBPATH += ../library
INCLUDEPATH += ../library

MOC_DIR      = moc
OBJECTS_DIR  = obj

# DEFINES += MML_TEST

HEADERS = \
	formulaview.h \
	mainwindow.h

SOURCES = \
	formulaview.cpp \
	mainwindow.cpp \
	main.cpp
