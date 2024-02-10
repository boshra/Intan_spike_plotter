#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "qcustomplot.h"
#include <QtWidgets>

class QTcpSocket;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QTextEdit;
class QLabel;

class TCPClient : public QWidget
{
    Q_OBJECT

public:
    TCPClient(QWidget *parent = 0);

private:
    QTcpSocket *commandSocket;
    QTcpSocket *waveformSocket;
    QString waveformHost, commandHost;
    int waveformPort, commandPort;

    QPushButton *startRoutineButton;

    QLabel *waveformLabel;
    QLabel *waveformText;

    qint64 beginTime;
    qint64 endTime;

    QByteArray waveformInputBuffer;

    QVector<QCustomPlot*> customplots;
    QJsonArray channel_settings;
    QJsonArray channel_thresholds;

    QVector<int> spikethresholds;

    QMutex mut1;
    QMutex mut2;
    QMutex mut3;

    int waveformBytesPerFrame;
    int waveformBytesPerBlock;
    int allBytesPerFrame;
    int blocksPerRead;
    int waveformBytesMultiBlocks;
    int channels;

    int totalWaveformDataBlocksProcessed;

    int totalSpikesPerChannel = 20;
    int samplingRate = 30000;
    QVector<int> counters;

    void processWaveformChunk();

public slots:
    void connectCommandToHost();
    void connectWaveformToHost();
    void disconnectCommandFromHost();
    void disconnectWaveformFromHost();
    void startRoutineSlot();
    void readWaveform();
    void updatePlot(QVector<double> waveform, QVector<double> timestamps, int channel);

};

#endif // TCPCLIENT_H
