#include <QtGui>
#include <QtNetwork>

#include <stdio.h>

#include "tcpclient.h"


TCPClient::TCPClient(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Spike monitor");

    // TCP Socket for commands
    commandSocket = new QTcpSocket(this);
    connect(commandSocket, SIGNAL(connected()), this, SLOT(commandConnected()));
    connect(commandSocket, SIGNAL(disconnected()), this, SLOT(commandDisconnected()));
    connect(commandSocket, SIGNAL(readyRead()), this, SLOT(readCommandServer()));
    commandHost = new QLineEdit("127.0.0.1", this);
    commandHostLabel = new QLabel("Host: ");
    commandPort = new QSpinBox(this);
    commandPort->setRange(0,9999);
    commandPort->setValue(5000);
    commandPortLabel = new QLabel("Port: ");
    commandConnectButton = new QPushButton("Connect", this);
    connect(commandConnectButton, SIGNAL(clicked()), this, SLOT(connectCommandToHost()));
    commandDisconnectButton = new QPushButton("Disconnect", this);
    connect(commandDisconnectButton, SIGNAL(clicked()), this, SLOT(disconnectCommandFromHost()));

    // TCP Socket for waveform data
    waveformSocket = new QTcpSocket(this);
    connect(waveformSocket, SIGNAL(connected()), this,  SLOT(waveformConnected()));
    connect(waveformSocket, SIGNAL(disconnected()), this, SLOT(waveformDisconnected()));
    connect(waveformSocket, SIGNAL(readyRead()), this, SLOT(readWaveform()));
    waveformHost = new QLineEdit("127.0.0.1", this);
    waveformHostLabel = new QLabel("Host: ");
    waveformPortLabel = new QLabel("Port: ");
    waveformPort = new QSpinBox(this);
    waveformPort->setRange(0,9999);
    waveformPort->setValue(5001);
    waveformConnectButton = new QPushButton("Connect", this);
    connect(waveformConnectButton, SIGNAL(clicked()), this, SLOT(connectWaveformToHost()));
    waveformDisconnectButton = new QPushButton("Disconnect", this);
    connect(waveformDisconnectButton, SIGNAL(clicked()), this, SLOT(disconnectWaveformFromHost()));


    // Routine that automatically connects, enables channels, and runs
    routineLabel = new QLabel("Routine automatically connects the 2 sockets, enables 32 channels Wide, and runs controller.");
    startRoutineButton = new QPushButton("Initiate connection", this);
    connect(startRoutineButton, SIGNAL(clicked()), this, SLOT(startRoutineSlot()));

    // Panel that contains messages
    messageLabel = new QLabel("Messages:", this);
    messages = new QTextEdit(this);
    messages->setReadOnly(true);

    // Panel that contains commands
    commandLabel = new QLabel("Commands:", this);
    commandsTextEdit = new QTextEdit(this);
    commandsTextEdit->installEventFilter(this);

    // Labels for timestamp, waveform
    timestampLabel = new QLabel("Timestamp: ");
    timestampText = new QLabel;
    waveformLabel = new QLabel("Waveform data blocks received: ");
    waveformText = new QLabel;

    channels = 32;

    // Panel for plots
    QCustomPlot *plot;
    QCPGraph *graph;

    for (int channel=0;channel<channels;++channel){
        plot = new QCustomPlot();

        // Set up real-time data plot
        graph = plot->addGraph();

        // Set up axes labels
        plot->xAxis->setLabel("Time");
        plot->yAxis->setLabel("Amplitude (microV)");

        // Set up plot range
        plot->xAxis->setRange(-1, 2);
        plot->yAxis->setRange(-400, 400);

        customplots.append(plot);
        graphs.append(graph);

    }

    QVBoxLayout *mainLayout = new QVBoxLayout;

    QHBoxLayout *commandHostRow = new QHBoxLayout;
    commandHostRow->addWidget(commandHostLabel);
    commandHostRow->addWidget(commandHost);

    QHBoxLayout *commandPortRow = new QHBoxLayout;
    commandPortRow->addWidget(commandPortLabel);
    commandPortRow->addWidget(commandPort);

    QVBoxLayout *commandColumnLayout = new QVBoxLayout;
    commandColumnLayout->addLayout(commandHostRow);
    commandColumnLayout->addLayout(commandPortRow);
    commandColumnLayout->addWidget(commandConnectButton);
    commandColumnLayout->addWidget(commandDisconnectButton);

    QGroupBox *commandColumn = new QGroupBox;
    commandColumn->setLayout(commandColumnLayout);

    QHBoxLayout *waveformHostRow = new QHBoxLayout;
    waveformHostRow->addWidget(waveformHostLabel);
    waveformHostRow->addWidget(waveformHost);

    QHBoxLayout *waveformPortRow = new QHBoxLayout;
    waveformPortRow->addWidget(waveformPortLabel);
    waveformPortRow->addWidget(waveformPort);

    QVBoxLayout *waveformColumnLayout = new QVBoxLayout;
    waveformColumnLayout->addLayout(waveformHostRow);
    waveformColumnLayout->addLayout(waveformPortRow);
    waveformColumnLayout->addWidget(waveformConnectButton);
    waveformColumnLayout->addWidget(waveformDisconnectButton);

    QGroupBox *waveformColumn = new QGroupBox;
    waveformColumn->setLayout(waveformColumnLayout);

    QHBoxLayout *socketsRow = new QHBoxLayout;
    socketsRow->addWidget(commandColumn);
    socketsRow->addWidget(waveformColumn);

    QHBoxLayout *timestampRow = new QHBoxLayout;
    timestampRow->addWidget(timestampLabel);
    timestampRow->addWidget(timestampText);

    QHBoxLayout *waveformRow = new QHBoxLayout;
    waveformRow->addWidget(waveformLabel);
    waveformRow->addWidget(waveformText);

    QHBoxLayout *plotsRow = new QHBoxLayout;
    for (int channel=0;channel<channels;++channel)
        plotsRow->addWidget(customplots[channel]);


    mainLayout->addLayout(socketsRow);

    mainLayout->addWidget(routineLabel);
    mainLayout->addWidget(startRoutineButton);
    // mainLayout->addWidget(messageLabel);
    // mainLayout->addWidget(messages);
    // mainLayout->addWidget(commandLabel);
    // mainLayout->addWidget(commandsTextEdit);
    mainLayout->addLayout(timestampRow);
    mainLayout->addLayout(waveformRow);
    mainLayout->addLayout(plotsRow);
    // mainLayout->addWidget(sendCommandButton);

    setLayout(mainLayout);

    // Create waveformInputBuffer as a QByteArray to contain waveform data.

    waveformBytesPerFrame = 4 + 2 * channels;
    allBytesPerFrame = waveformBytesPerFrame + 4;
    waveformBytesPerBlock = 128 * waveformBytesPerFrame + 4;
    blocksPerRead = 10;
    waveformBytes10Blocks = blocksPerRead * waveformBytesPerBlock;
    waveformInputBuffer.resize(waveformBytes10Blocks);

    waveform10blocks.resize(channels);

    for  (int i = 0; i < channels; i++) {
        waveform10blocks[i].resize(128*blocksPerRead);
    }
    // timestamps10blocks.append(new QVector<float>(128*blocksPerRead));


    totalWaveformDataBlocksProcessed = 0;

}

// Slot to update plot with new data
void TCPClient::updatePlot(QVector<double> waveform, QVector<double> timestamps, int channel)
{
    // Update plot
    graphs[channel]->setData(timestamps, waveform);
    customplots[channel]->replot();
}

bool TCPClient::eventFilter(QObject *obj, QEvent *event)
{
    // When user presses enter, send command.
    if (obj == commandsTextEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return) {
                sendCommand();
                return true;
            }
        }
    }
    return false;
}

// Connect TCP Command Socket to the currently specified host address/port
void TCPClient::connectCommandToHost()
{
    commandSocket->connectToHost(QHostAddress(commandHost->text()), commandPort->value());
}

// Connect TCP Waveform Data Socket to the currently specified host address/port
void TCPClient::connectWaveformToHost()
{
    waveformSocket->connectToHost(QHostAddress(waveformHost->text()), waveformPort->value());
}

// Disconnect TCP Command Socket from the currently connected host address/port
void TCPClient::disconnectCommandFromHost()
{
    commandSocket->disconnectFromHost();
}

// Disconnect TCP Waveform Data Socket from the currently connected host address/port
void TCPClient::disconnectWaveformFromHost()
{
    waveformSocket->disconnectFromHost();
}

// Run the routine that connects to the command, waveform servers, enables TCP data output on some channels, and starts the controller running
void TCPClient::startRoutineSlot()
{
    // Connect to TCP command server
    connectCommandToHost();

    // Connect to TCP waveform server
    connectWaveformToHost();


    // Wait for 2 connections to be made
    while (commandSocket->state() != QAbstractSocket::ConnectedState ||
           waveformSocket->state() != QAbstractSocket::ConnectedState ) {
        qApp->processEvents();
    }

    // Clear TCP data output to ensure no TCP channels are enabled at the beginning of this routine
    commandSocket->write("execute clearalldataoutputs;");
    commandSocket->waitForBytesWritten();

    // Enable wide output for first 32 channels
    char command[50];

    for(int i = 0;i<32;i++) {
        sprintf(command, "set a-%03d.tcpdataoutputenabledhigh true;", i);
        commandSocket->write(command);
        commandSocket->waitForBytesWritten();
    }

    commandSocket->write("set runmode run;");
    commandSocket->waitForBytesWritten();

    beginTime = QDateTime::currentMSecsSinceEpoch();
}

// Executes when TCP Command Socket is successfully connected
void TCPClient::commandConnected()
{
    messages->append("Command Port Connected");
}

// Executes when TCP Command Socket is disconnected
void TCPClient::commandDisconnected()
{
    messages->append("Command Port Disconnected");
}

// Executes when text is received over TCP Command Socket
void TCPClient::readCommandServer()
{
    QString result;
    result = commandSocket->readAll();
    messages->append(result);
    return;
}

// Sends text over TCP Command Socket
void TCPClient::sendCommand()
{
    if (commandSocket->state() == commandSocket->ConnectedState) {
        commandSocket->write(commandsTextEdit->toPlainText().toLatin1());
        commandSocket->waitForBytesWritten();
    }
}

// Executes when TCP Waveform Data Socket is successfully connected
void TCPClient::waveformConnected()
{
    messages->append("Waveform Port Connected");
}

// Executes when TCP Waveform Data Socket is disconnected
void TCPClient::waveformDisconnected()
{
    messages->append("Waveform Port Disconnected");
}


// Read waveform data in 10-block chunks when it comes in on TCP Waveform Data Sockets
void TCPClient::readWaveform()
{
    if (waveformSocket->bytesAvailable() > waveformBytes10Blocks) {
        processWaveformChunk();
    }

    return;
}

// Process 10-data-block chunk of waveform data.
// This processing it minimal; just checks magic numbers, parses timestamp, and counts how many data blocks have been processed.
// If more sophisticated processing of incoming waveform data is desired, this function should be expanded.
void TCPClient::processWaveformChunk()
{
    waveformInputBuffer = waveformSocket->read(waveformBytes10Blocks);
    QVector<double> timestamps;
    QVector<double> waveform;

    int channelShift;
    int timestampShift;
    int magicnumShift;

    for (int channel = 0; channel < channels; ++channel) {
        waveform.clear();
        for (int block = 0; block < blocksPerRead; ++block) {
            if (channel == 0) {
                magicnumShift = block * (4 + waveformBytesPerFrame * 128);
                uint32_t magicNum = ((uint8_t) waveformInputBuffer[magicnumShift + 3] << 24) +
                                    ((uint8_t) waveformInputBuffer[magicnumShift + 2] << 16) +
                                    ((uint8_t) waveformInputBuffer[magicnumShift + 1] << 8) +
                                    (uint8_t) waveformInputBuffer[magicnumShift];
                if (magicNum != 0x2ef07a08) {
                    qDebug() << "ERROR READING WAVEFORM MAGIC NUMBER... read magicNum: " << magicNum << " block: " << block;
                }
            }
            for (int frame = 0; frame < 128; ++frame) {
                channelShift = (block * (4 + waveformBytesPerFrame * 128)) + 4 + (frame * (4 + 2 * channels)) + 4 + channel*2;
                timestampShift = (block * (4 + waveformBytesPerFrame * 128)) + 4 + (frame * (4 + 2 * channels));

                if (channel == 0) {
                    int32_t timestamp = (int32_t)((uint8_t) waveformInputBuffer[timestampShift + 3] << 24) +
                                            ((uint8_t) waveformInputBuffer[timestampShift + 2] << 16) +
                                            ((uint8_t) waveformInputBuffer[timestampShift + 1] << 8) +
                                            (uint8_t) waveformInputBuffer[timestampShift];

                    timestamps.append((double)timestamp / samplingRate);

                    if (frame == 0 && block == 0) {
                        timestampText->setText(QString::number(timestamp));
                    }
                }

                float dataSample = (((uint16_t)((uint8_t) waveformInputBuffer[channelShift + 1] << 8) + (uint8_t) waveformInputBuffer[channelShift]) - 32768) * 0.195;
                waveform.append(dataSample);

            }
        }
        int i=30;
        int peak=-1;
        while (i<waveform.size() - 60) {
            if (waveform[i] < spikethreshold) {
                peak = i;
                for(int j=i;j<i+60;++j) {
                    if (waveform[j] < waveform[i] && j+60 < waveform.size())
                        peak = j;
                }

                i = peak + 5;
                QVector<double> spike;
                QVector<double> spikestamp;
                QVector<double> timelock;
                for (int k = peak - 30; k <= peak + 60; ++k) {
                    spike.append((double)waveform[k]);
                    spikestamp.append(timestamps[k]);
                    timelock.append((double)(k-peak) / 30.0);

                }
                updatePlot(spike, timelock, channel);
            }
            else
                i++;
        }
    }

    totalWaveformDataBlocksProcessed += 10;
    waveformText->setText(QString::number(totalWaveformDataBlocksProcessed));
}
