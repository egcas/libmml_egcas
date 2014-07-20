TEMPLATE = lib
TARGET   = egcas

CONFIG += qt 
CONFIG += warn_on
CONFIG += c++11
CONFIG += sharedlib

#remove for production
#CONFIG += debug

QT += xml
QT += widgets

MOC_DIR      = moc
OBJECTS_DIR  = obj

# DEFINES += MML_TEST

HEADERS = \
	eg_mml_document.h \
        eg_mml_entity_table.h

SOURCES = \
	eg_mml_document.cpp \
        eg_mml_entity_table.cpp

        
headers.path = /usr/local/include/libegcas/
headers.files = eg_mml_document.h \
		eg_mml_entity_table.h
INSTALLS += headers 

target.path = /usr/local/lib
INSTALLS += target
