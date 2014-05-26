TEMPLATE = lib
TARGET   = egcas

CONFIG += qt 
CONFIG += warn_on
CONFIG += sharedlib

QT += xml
QT += widgets

MOC_DIR      = moc
OBJECTS_DIR  = obj

# DEFINES += MML_TEST

HEADERS = \
	qwt_mml_document.h \
        qwt_mml_entity_table.h

SOURCES = \
	qwt_mml_document.cpp \
        qwt_mml_entity_table.cpp

        
headers.path = /usr/local/include/libegcas/
headers.files = qwt_mml_document.h \
		qwt_mml_entity_table.h
INSTALLS += headers 

target.path = /usr/local/lib
INSTALLS += target
