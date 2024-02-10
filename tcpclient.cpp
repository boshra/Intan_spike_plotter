#include <QtGui>
#include <QtNetwork>
#include <QtConcurrent/QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

#include <iostream>
#include <stdio.h>

#include "tcpclient.h"
#include "qcustomplot.h"


TCPClient::TCPClient(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Spike monitor");

    // TCP Socket for commands
    commandSocket = new QTcpSocket(this);

    // TCP Socket for waveform data
    waveformSocket = new QTcpSocket(this);
    connect(waveformSocket, SIGNAL(readyRead()), this, SLOT(readWaveform()));

    waveformLabel = new QLabel("Waveform data blocks received: ");
    waveformLabel->setMaximumHeight(10);
    waveformText = new QLabel;
    waveformText->setMaximumHeight(10);


    // Routine that automatically connects, enables channels, and runs
    startRoutineButton = new QPushButton("Initiate connection", this);
    connect(startRoutineButton, SIGNAL(clicked()), this, SLOT(startRoutineSlot()));

    // Load JSON file
    QFile file("settings.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open settings.json";
        return;
    }

    // Read JSON data
    QByteArray jsonData = file.readAll();
    file.close();

    // Parse JSON document
    QJsonDocument document = QJsonDocument::fromJson(jsonData);
    if (document.isNull()) {
        qDebug() << "Failed to parse settings.json";
        return;
    }

    // Get JSON object
    QJsonObject jsonObject = document.object();

    // Set global parameters from JSON

    channel_settings = jsonObject["channel_settings"].toArray();
    channel_thresholds = jsonObject["channel_thresholds"].toArray();

    waveformHost = jsonObject["waveformhost_ip"].toString();
    commandHost = jsonObject["commandhost_ip"].toString();
    waveformPort = jsonObject["waveformhost_port"].toInt();
    commandPort = jsonObject["commandhost_port"].toInt();

    channels = channel_settings.size();

    spikethresholds.resize(channels);

    // Panel for plots
    QCustomPlot *plot;

    char title[50];

    std::cout << channels << std::endl;

    counters.resize(channels);
    for (int channel=0;channel<channels;++channel){
        plot = new QCustomPlot();

        plot->plotLayout()->insertRow(0);

        std::sprintf(title, "Channel %s", channel_settings[channel].toString().toStdString().c_str());

        spikethresholds[channel] = channel_thresholds[channel].toInt();

        plot->plotLayout()->addElement(0,0, new QCPTextElement(plot,title));
        // Set up real-time data plot
        for(int s=0;s<totalSpikesPerChannel;++s)
            plot->addGraph();

        // Set up axes labels
        plot->xAxis->setLabel("Time");
        plot->yAxis->setLabel("Amplitude (microV)");

        // Set up plot range
        plot->xAxis->setRange(-1, 2);
        plot->yAxis->setRange(-400, 400);

        QCPItemStraightLine *infLine = new QCPItemStraightLine(plot);
        infLine->point1->setCoords(2, spikethresholds[channel]);  // location of point 1 in plot coordinate
        infLine->point2->setCoords(2, spikethresholds[channel]);  // location of point 2 in plot coordinate

        plot->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Maximum);
        plot->setMinimumHeight(130);
        customplots.append(plot);
        // graphs.append(graph);

        counters[channel] = 0;

    }

    QVBoxLayout *mainLayout = new QVBoxLayout;

    QHBoxLayout *waveformRow = new QHBoxLayout;
    waveformRow->addWidget(waveformLabel);
    waveformRow->addWidget(waveformText);

    QHBoxLayout *allplots = new QHBoxLayout;
    QVBoxLayout *plotsRowOne = new QVBoxLayout;
    QVBoxLayout *plotsRowTwo = new QVBoxLayout;
    QVBoxLayout *plotsRowThree = new QVBoxLayout;

    allplots->addLayout(plotsRowOne);
    allplots->addLayout(plotsRowTwo);
    allplots->addLayout(plotsRowThree);


    for (int channel=0;channel<channels;++channel)
        if (channel %3 == 0)
            plotsRowOne->addWidget(customplots[channel]);
        else if (channel %3 == 1)
            plotsRowTwo->addWidget(customplots[channel]);
        else if (channel %3 == 2)
            plotsRowThree->addWidget(customplots[channel]);


    mainLayout->addWidget(startRoutineButton);
    mainLayout->addLayout(allplots);
    mainLayout->addLayout(waveformRow);


    setLayout(mainLayout);

    // Create waveformInputBuffer as a QByteArray to contain waveform data.

    waveformBytesPerFrame = 4 + 2 * channels;
    allBytesPerFrame = waveformBytesPerFrame + 4;
    waveformBytesPerBlock = 128 * waveformBytesPerFrame + 4;
    blocksPerRead = 60;
    waveformBytesMultiBlocks = blocksPerRead * waveformBytesPerBlock;
    waveformInputBuffer.resize(waveformBytesMultiBlocks);

    totalWaveformDataBlocksProcessed = 0;

}

// Slot to update plot with new data
extern void TCPClient::updatePlot(QVector<double> waveform, QVector<double> timestamps, int channel)
{
    // Update plot
    if (channel % 3 == 0)
        mut1.lock();
    else if (channel % 3 == 1)
        mut2.lock();
    else
        mut3.lock();

    customplots[channel]->graph(counters[channel])->setData(timestamps, waveform);
    customplots[channel]->replot(QCustomPlot::RefreshPriority::rpQueuedReplot);
    counters[channel] = (counters[channel] + 1) % totalSpikesPerChannel;

    if (channel % 3 == 0)
        mut1.unlock();
    else if (channel % 3 == 1)
        mut2.unlock();
    else
        mut3.unlock();
}

// Connect TCP Command Socket to the currently specified host address/port
void TCPClient::connectCommandToHost()
{
    commandSocket->connectToHost(QHostAddress(commandHost), commandPort);
}

// Connect TCP Waveform Data Socket to the currently specified host address/port
void TCPClient::connectWaveformToHost()
{
    waveformSocket->connectToHost(QHostAddress(waveformHost), waveformPort);
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
    std::cout << waveformHost.toStdString() << std::endl;
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

    for(int i = 0;i<channels;i++) {
        sprintf(command, "set %s.tcpdataoutputenabledhigh true;", channel_settings[i].toString().toStdString().c_str());
        commandSocket->write(command);
        commandSocket->waitForBytesWritten();
    }

    commandSocket->write("set runmode run;");
    commandSocket->waitForBytesWritten();

    beginTime = QDateTime::currentMSecsSinceEpoch();
}



// Read waveform data in 10-block chunks when it comes in on TCP Waveform Data Sockets
void TCPClient::readWaveform()
{
    if (waveformSocket->bytesAvailable() > waveformBytesMultiBlocks) {
        processWaveformChunk();
    }

    return;
}

// Process 10-data-block chunk of waveform data.
// This processing it minimal; just checks magic numbers, parses timestamp, and counts how many data blocks have been processed.
// If more sophisticated processing of incoming waveform data is desired, this function should be expanded.
void TCPClient::processWaveformChunk()
{
    QFuture<void> future;
    QThreadPool pool;

    std::cout << "Called process chunk " << ((float) clock()) / CLOCKS_PER_SEC << std::endl;
    waveformInputBuffer = waveformSocket->read(waveformBytesMultiBlocks);
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
                }

                float dataSample = (((uint16_t)((uint8_t) waveformInputBuffer[channelShift + 1] << 8) + (uint8_t) waveformInputBuffer[channelShift]) - 32768) * 0.195;
                waveform.append(dataSample);

            }
        }
        int i=30;
        int peak=-1;
        while (i<waveform.size() - 60) {
            if (waveform[i] < spikethresholds[channel]) {
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

                QtConcurrent::run(&pool,this, &TCPClient::updatePlot, spike, timelock, channel);

            }
            else
                i++;
        }
    }

    totalWaveformDataBlocksProcessed += blocksPerRead;
    waveformText->setText(QString::number(totalWaveformDataBlocksProcessed));
}
