#-------------------------------------------------
#
# Project created by QtCreator 2016-12-28T14:30:16
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = TCPClient
TEMPLATE = app


SOURCES += main.cpp \
    qcustomplot.cpp \
    tcpclient.cpp

HEADERS  += \
    circularbuffer.h \
    qcustomplot.h \
    tcpclient.h
