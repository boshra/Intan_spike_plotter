#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "qcustomplot.h"
#include "circularbuffer.h"
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
    QLabel *commandHostLabel;
    QLineEdit *commandHost;
    QLabel *waveformHostLabel;
    QLineEdit *waveformHost;
    QLabel *commandPortLabel;
    QSpinBox *commandPort;
    QLabel *waveformPortLabel;
    QSpinBox *waveformPort;

    QPushButton *commandConnectButton;
    QPushButton *commandDisconnectButton;
    QPushButton *waveformConnectButton;
    QPushButton *waveformDisconnectButton;

    QLabel *routineLabel;
    QPushButton *startRoutineButton;
    QLabel *messageLabel;
    QTextEdit *messages;
    QLabel *commandLabel;
    QTextEdit *commandsTextEdit;
    QPushButton *sendCommandButton;

    QLabel *timestampLabel;
    QLabel *timestampText;

    QLabel *waveformLabel;
    QLabel *waveformText;

    qint64 beginTime;
    qint64 endTime;

    QByteArray waveformInputBuffer;

    QVector<QVector <float>> waveform10blocks;

    //Channel X 20 waveforms X times
    QVector<CircularBuffer> spikewaveforms;

    QVector<QCustomPlot*> customplots;
    QVector<QCPGraph*> graphs;

    int waveformBytesPerFrame;
    int waveformBytesPerBlock;
    int allBytesPerFrame;
    int blocksPerRead;
    int waveformBytes10Blocks;
    int channels;

    int totalWaveformDataBlocksProcessed;

    int spikethreshold = -30;
    int totalSpikesPerChannel = 20;
    int samplingRate = 30000;

    void processWaveformChunk();

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public slots:
    void connectCommandToHost();
    void connectWaveformToHost();

    void disconnectCommandFromHost();
    void disconnectWaveformFromHost();

    void startRoutineSlot();

    void commandConnected();
    void commandDisconnected();

    void waveformConnected();
    void waveformDisconnected();

    void readCommandServer();
    void sendCommand();

    void readWaveform();

    void updatePlot(QVector<double> waveform, QVector<double> timestamps, int channel);

};

#endif // TCPCLIENT_H
