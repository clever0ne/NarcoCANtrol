#include "main_window.h"
#include "ui_main_window.h"
#include "settings_dialog.h"
#include "bitrate.h"
#include "../cannabus_library/cannabus_common.h"

#include <QCanBus>
#include <QCanBusFrame>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_busStatusTimer(new QTimer(this)),
    m_logWindowUpdateTimer(new QTimer(this))
{
    m_ui->setupUi(this);

    m_settingsDialog = new SettingsDialog;

    m_status = new QLabel;
    m_ui->statusBar->addPermanentWidget(m_status);

    m_written = new QLabel;
    m_ui->statusBar->addWidget(m_written);

    initActionsConnections();
}

MainWindow::~MainWindow()
{
    delete m_settingsDialog;
    delete m_ui;
}

void MainWindow::initActionsConnections()
{
    m_ui->actionDisconnect->setEnabled(false);

    connect(m_ui->actionSettings, &QAction::triggered,
        m_settingsDialog, &QDialog::show);
    connect(m_ui->actionConnect, &QAction::triggered,
    this, &MainWindow::connectDevice);
    connect(m_ui->actionDisconnect, &QAction::triggered,
            this, &MainWindow::disconnectDevice);
    connect(m_ui->actionClearLog, &QAction::triggered,
            m_ui->receivedMessagesLogWindow, &LogWindow::clearLog);
    connect(m_ui->actionQuit, &QAction::triggered,
            this, &QWidget::close);

    connect(m_settingsDialog, &QDialog::accepted, [this] {
        disconnectDevice();
        connectDevice();
    });

    connect(m_busStatusTimer, &QTimer::timeout,
            this, &MainWindow::busStatus);
    connect(m_logWindowUpdateTimer, &QTimer::timeout,
            this, &MainWindow::processFramesReceived);
}

void MainWindow::processError(QCanBusDevice::CanBusError error) const
{
    switch(error)
    {
        case QCanBusDevice::ReadError:
        case QCanBusDevice::WriteError:
        case QCanBusDevice::ConnectionError:
        case QCanBusDevice::ConfigurationError:
        case QCanBusDevice::UnknownError:
        {
            m_status->setText(m_canDevice->errorString());
            break;
        }
        default:
        {
            break;
        }
    }
}

void MainWindow::connectDevice()
{
    const SettingsDialog::Settings settings = m_settingsDialog->settings();

    QString errorString;
    m_canDevice.reset(QCanBus::instance()->createDevice(
                        settings.pluginName,
                        settings.deviceInterfaceName,
                        &errorString));

    if (m_canDevice == nullptr)
    {
        m_status->setText(tr("Error creating device '%1': '%2'")
                        .arg(settings.pluginName)
                        .arg(errorString));
        return;
    }

    m_ui->receivedMessagesLogWindow->clearLog();
    m_numberFramesReceived = 0;

    connect(m_canDevice.get(), &QCanBusDevice::errorOccurred,
            this, &MainWindow::processError);

    for (const SettingsDialog::ConfigurationItem &item : qAsConst(settings.configurations))
    {
        m_canDevice->setConfigurationParameter(item.first, item.second);
    }

    if (m_canDevice->connectDevice() == false)
    {
        m_status->setText(tr("Connection error: %1").arg(m_canDevice->errorString()));

        m_canDevice.reset();
    }
    else
    {
        m_ui->actionConnect->setEnabled(false);
        m_ui->actionDisconnect->setEnabled(true);

        const QVariant bitRate = m_canDevice->configurationParameter(QCanBusDevice::BitRateKey);
        if (bitRate.isValid() != false)
        {
            const bool isCanFdEnabled = m_canDevice->configurationParameter(QCanBusDevice::CanFdKey).toBool();
            const QVariant dataBitRate = m_canDevice->configurationParameter(QCanBusDevice::DataBitRateKey);

            if (isCanFdEnabled != false && dataBitRate.isValid() != false)
            {
                m_status->setText(tr("Plugin '%1': connected to %2 at %3 / %4 with CAN FD")
                                .arg(settings.pluginName)
                                .arg(settings.deviceInterfaceName)
                                .arg(bitRateToString(bitRate.toUInt()))
                                .arg(bitRateToString(dataBitRate.toUInt())));
            }
            else
            {
                m_status->setText(tr("Plugin '%1': connected to %2 at %3")
                                .arg(settings.pluginName)
                                .arg(settings.deviceInterfaceName)
                                .arg(bitRateToString(bitRate.toUInt())));
            }
        }
        else
        {
            m_status->setText(tr("Plugin '%1': connected to %2")
                            .arg(settings.pluginName)
                            .arg(settings.deviceInterfaceName));
        }

        if (m_canDevice->hasBusStatus() != false)
        {
            m_busStatusTimer->start(1000);
            m_logWindowUpdateTimer->start(100);
        }
        else
        {
            m_ui->busStatus->setText(tr("No CAN bus status available"));
        }
    }
}

void MainWindow::disconnectDevice()
{
    if (m_canDevice == nullptr)
    {
        return;
    }

    m_busStatusTimer->stop();
    m_logWindowUpdateTimer->stop();

    processFramesReceived();

    m_canDevice->disconnectDevice();

    m_ui->busStatus->setText(tr("No CAN bus status available."));

    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);

    m_status->setText("Disconnected");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_settingsDialog->close();
    event->accept();
}

void MainWindow::processFramesReceived()
{
    if (m_canDevice == nullptr)
    {
        return;
    }

    while (m_canDevice->framesAvailable() != false)
    {
        const QCanBusFrame frame = m_canDevice->readFrame();

        m_numberFramesReceived++;

        // Обработка кадра ошибки
        if (frame.frameType() == QCanBusFrame::ErrorFrame)
        {
            uint32_t row = m_ui->receivedMessagesLogWindow->rowCount();
            m_ui->receivedMessagesLogWindow->insertRow(row);

            QString count = QString::number(m_numberFramesReceived);
            QTableWidgetItem *countItem = new QTableWidgetItem(count);
            m_ui->receivedMessagesLogWindow->setItem(row, 0, countItem);

            QString time = tr("%1.%2")
                    .arg(frame.timeStamp().seconds(), 4, 10, QLatin1Char(' '))
                    .arg(frame.timeStamp().microSeconds() / 100, 4, 10, QLatin1Char('0'));
            QTableWidgetItem *timeItem = new QTableWidgetItem(time);
            m_ui->receivedMessagesLogWindow->setItem(row, 1, timeItem);

            QString info = m_canDevice->interpretErrorFrame(frame);
            QTableWidgetItem *infoItem = new QTableWidgetItem(info);
            m_ui->receivedMessagesLogWindow->setItem(row, 7, infoItem);

            m_ui->receivedMessagesLogWindow->scrollToBottom();

            continue;
        }        

        // Обработка обычных кадров
        const uint32_t frameId = frame.frameId();

        cannabus::IdMsgTypes msgType = cannabus::getMsgTypeFromId(frameId);
        QString msgTypeInfo;

        cannabus::IdFCode fCode = cannabus::getFCodeFromId(frameId);
        QString fCodeInfo;

        switch (msgType)
        {
            case cannabus::IdMsgTypes::HIGH_PRIO_MASTER:
            {
                msgTypeInfo = tr("Master's high-priority");
                break;
            }
            case cannabus::IdMsgTypes::HIGH_PRIO_SLAVE:
            {
                msgTypeInfo = tr("Slave's high-priority");
                break;
            }
            case cannabus::IdMsgTypes::MASTER:
            {
                msgTypeInfo = tr("Master's request");
                break;
            }
            case cannabus::IdMsgTypes::SLAVE:
            {
                msgTypeInfo = tr("Slave's response");
                break;
            }
            default:
            {
                break;
            }
        }

        switch (fCode)
        {
            case cannabus::IdFCode::WRITE_REGS_RANGE:
            {
                fCodeInfo = tr("Writing register's range");
                break;
            }
            case cannabus::IdFCode::WRITE_REGS_SERIES:
            {
                fCodeInfo = tr("Writing register's series");
                break;
            }
            case cannabus::IdFCode::READ_REGS_RANGE:
            {
                fCodeInfo = tr("Reading register's range");
                break;
            }
            case cannabus::IdFCode::READ_REGS_SERIES:
            {
                fCodeInfo = tr("Reading register's series");
                break;
            }
            case cannabus::IdFCode::DEVICE_SPECIFIC1:
            {
                fCodeInfo = tr("Device-specific function (1)");
                break;
            }
            case cannabus::IdFCode::DEVICE_SPECIFIC2:
            {
                fCodeInfo = tr("Device-specific function (2)");
                break;
            }
            case cannabus::IdFCode::DEVICE_SPECIFIC3:
            {
                fCodeInfo = tr("Device-specific function (3)");
                break;
            }
            case cannabus::IdFCode::DEVICE_SPECIFIC4:
            {
                fCodeInfo = tr("Device-specific function (4)");
                break;
            }
            default:
            {
                break;
            }
        }

        QString count = QString::number(m_numberFramesReceived);
        QString time = tr("%1.%2")
                .arg(frame.timeStamp().seconds(), 4, 10, QLatin1Char(' '))
                .arg(frame.timeStamp().microSeconds() / 100, 4, 10, QLatin1Char('0'));
        QString slaveAddress = QString::number(cannabus::getAddressFromId(frameId));
        QString dataSize = "[" + QString::number(frame.payload().size()) + "]";
        QString data(frame.payload().toHex(' ').toUpper());
        QString info;

        if (dataSize != "[0]")
        {
            info = tr("[%1] %2")
                    .arg(fCodeInfo)
                    .arg(msgTypeInfo);
        }
        else
        {
            info = tr("Error: incorrect request!");
        }

        QStringList frameInfo = {
            count,
            time,
            QString::number((uint32_t)fCode, 2).rightJustified(2, '0'),
            slaveAddress,
            QString::number((uint32_t)msgType, 2).rightJustified(3, '0'),
            dataSize,
            data,
            info
        };

        uint32_t row = m_ui->receivedMessagesLogWindow->rowCount();
        m_ui->receivedMessagesLogWindow->insertRow(row);

        for (uint8_t column = 0; column < m_ui->receivedMessagesLogWindow->columnCount(); column++)
        {
            QTableWidgetItem *item = new QTableWidgetItem(frameInfo.takeFirst());
            m_ui->receivedMessagesLogWindow->setItem(row, column, item);
        }

        m_ui->receivedMessagesLogWindow->scrollToBottom();
    }
}

void MainWindow::busStatus()
{
    if (m_canDevice == nullptr || m_canDevice->hasBusStatus() == false)
    {
        m_ui->busStatus->setText(tr("No CAN bus status available."));
        m_busStatusTimer->stop();
        return;
    }

    switch(m_canDevice->busStatus())
    {
        case QCanBusDevice::CanBusStatus::Good:
        {
            m_ui->busStatus->setText(tr("CAN bus status: Good."));
            break;
        }
        case QCanBusDevice::CanBusStatus::Warning:
        {
            m_ui->busStatus->setText(tr("CAN bus status: Warning."));
            break;
        }
        case QCanBusDevice::CanBusStatus::Error:
        {
            m_ui->busStatus->setText(tr("CAN bus status: Error."));
            break;
        }
        case QCanBusDevice::CanBusStatus::BusOff:
        {
            m_ui->busStatus->setText(tr("CAN bus status: Bus Off."));
            break;
        }
        default:
        {
            m_ui->busStatus->setText(tr("CAN bus status: Unknown."));
            break;
        }
    }
}
