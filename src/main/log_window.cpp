#include "log_window.h"
#include <QHeaderView>
#include <algorithm>

using namespace cannabus;

LogWindow::LogWindow(QWidget *parent) : QTableWidget(parent)
{    
    makeHeader();

    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->hide();

    setDefaultMessageFilterSettings();
}

void LogWindow::makeHeader()
{
    QStringList logWindowHeader = {"No.", "Time", "Msg Type", "Address", "F-Code", "DLC", "Data", "Info"};

   setColumnCount(logWindowHeader.count());
   setHorizontalHeaderLabels(logWindowHeader);

   resizeColumnsToContents();
   setColumnWidth(COUNT, 50);
   setColumnWidth(TIME, 80);
   setColumnWidth(MSG_TYPE, 75);
   setColumnWidth(SLAVE_ADDRESS, 80);
   setColumnWidth(F_CODE, 65);
   setColumnWidth(DATA_SIZE, 40);
   setColumnWidth(DATA, 195);

   horizontalHeader()->setStyleSheet("QHeaderView::section { background-color: beige; font-family: \"Courier New\"; font-size: 8pt }");
}

void LogWindow::clearLog()
{    
    m_numberFramesReceived = 0;

    clear();
    setRowCount(0);
    makeHeader();
}

void LogWindow::processDataFrame(const QCanBusFrame &frame)
{
    m_numberFramesReceived++;

    if (mustDataFrameBeProcessed(frame) == false)
    {
        return;
    }

    m_currentRow = rowCount();
    insertRow(m_currentRow);

    const uint32_t frameId = frame.frameId();
    const uint32_t slaveAddress = getAddressFromId(frameId);
    const IdMsgTypes msgType = getMsgTypeFromId(frameId);
    const IdFCode fCode = getFCodeFromId(frameId);

    setCount();
    setTime(frame.timeStamp().seconds(), frame.timeStamp().microSeconds());
    setMsgType(msgType);
    setSlaveAddress(slaveAddress);
    setFCode(fCode);
    setDataSize(frame.payload().size());
    setData(frame.payload());
    setMsgInfo(msgType, fCode, frame.payload().size());

    scrollToBottom();
}

void LogWindow::processErrorFrame(const QCanBusFrame &frame, const QString errorInfo)
{
    m_numberFramesReceived++;

    m_currentRow = rowCount();
    insertRow(m_currentRow);

    setCount();
    setTime(frame.timeStamp().seconds(), frame.timeStamp().microSeconds());
    setMsgInfo(errorInfo);
}

void LogWindow::setCount()
{
    // Выводим номер принятого кадра с шириной поля в 6 символов
    // в формате '   123'
    m_count = tr("%1").arg(m_numberFramesReceived, 6, 10, QLatin1Char(' '));

    auto item = new QTableWidgetItem(m_count);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setItem(m_currentRow, COUNT, item);
}

void LogWindow::setTime(const uint64_t seconds, const uint64_t microseconds)
{
    // Выводим время в секундах с шириной поля в 9 символов
    // в формате '  12.3456'
    m_time = tr("%1.%2")
            .arg(seconds, 4, 10, QLatin1Char(' '))
            .arg(microseconds / 100, 4, 10, QLatin1Char('0'));

    auto item = new QTableWidgetItem(m_time);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setItem(m_currentRow, TIME, item);
}

void LogWindow::setMsgType(const IdMsgTypes msgType)
{
    // Выводим тип сообщения в двоичной системе счисления с шириной поля в 4 символа
    // в формате '0b10'
    m_msgType = tr("0b%1").arg((uint32_t)msgType, 2, 2, QLatin1Char('0'));

    auto item = new QTableWidgetItem(m_msgType);
    item->setTextAlignment(Qt::AlignCenter);
    setItem(m_currentRow, MSG_TYPE, item);
}

void LogWindow::setSlaveAddress(const uint32_t slaveAddress)
{
    // Выводим адрес ведомого устройства в десятичной
    // и шестнадцатеричной системах счисления с шириной поля в 9 символов
    // в формате '10 (0x0A)'
    m_slaveAddress = tr("%1 (0x").arg(slaveAddress, 2, 10, QLatin1Char(' ')) +
            tr("%1)").arg(slaveAddress, 2, 16, QLatin1Char('0')).toUpper();

    auto item = new QTableWidgetItem(m_slaveAddress);
    item->setTextAlignment(Qt::AlignCenter);
    setItem(m_currentRow, SLAVE_ADDRESS, item);
}

void LogWindow::setFCode(const IdFCode fCode)
{
    // Выводим F-код в двоичной системе счисления с шириной поля в 5 символов
    // в формате '0b101'
    m_fCode = tr("0b%1").arg((uint32_t)fCode, 3, 2, QLatin1Char('0'));

    auto item = new QTableWidgetItem(m_fCode);
    item->setTextAlignment(Qt::AlignCenter);
    setItem(m_currentRow, F_CODE, item);
}

void LogWindow::setDataSize(const uint32_t dataSize)
{
    // Выводим размер поля данных в байтах с шириной поля в 3 символа
    // в формате '[8]'
    m_dataSize = tr("[%1]").arg(dataSize);

    auto item = new QTableWidgetItem(m_dataSize);
    item->setTextAlignment(Qt::AlignCenter);
    setItem(m_currentRow, DATA_SIZE, item);
}

void LogWindow::setData(const QByteArray data)
{
    // Выводим данные в шестнадцатеричном системе счисления без префиксов
    // с шириной поля данных до 25 символов (сколько получится)
    // в формате '11 22 33 44 55 66 77 88'
    m_data = data.toHex(' ').toUpper();

    auto item = new QTableWidgetItem(m_data);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setItem(m_currentRow, DATA, item);
}

void LogWindow::setMsgInfo(const IdMsgTypes msgType, const IdFCode fCode, const uint32_t dataSize)
{
    // Готовим словесное описание для типа сообщения и его F-кода,
    // а затем выводим информацию о кадре в текстовом формате
    // с шириной поля до 40 символов (сколько получится)
    // в формате '[MSG_TYPE_INFO] F_CODE_INFO'
    if (dataSize == 0)
    {
        m_msgInfo = tr("[Slave's response] Incorrect request");
    }
    else
    {
        QString msgTypeInfo;
        QString fCodeInfo;

        switch (msgType)
        {
            case IdMsgTypes::HIGH_PRIO_MASTER:
            {
                msgTypeInfo = tr("Master's high-prio");
                break;
            }
            case IdMsgTypes::HIGH_PRIO_SLAVE:
            {
                msgTypeInfo = tr("Slave's high-prio");
                break;
            }
            case IdMsgTypes::MASTER:
            {
                msgTypeInfo = tr("Master's request");
                break;
            }
            case IdMsgTypes::SLAVE:
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
            case IdFCode::WRITE_REGS_RANGE:
            {
                fCodeInfo = tr("Writing regs range");
                break;
            }
            case IdFCode::WRITE_REGS_SERIES:
            {
                fCodeInfo = tr("Writing regs series");
                break;
            }
            case IdFCode::READ_REGS_RANGE:
            {
                fCodeInfo = tr("Reading regs range");
                break;
            }
            case IdFCode::READ_REGS_SERIES:
            {
                fCodeInfo = tr("Reading regs series");
                break;
            }
            case IdFCode::DEVICE_SPECIFIC1:
            {
                fCodeInfo = tr("Device-specific (1)");
                break;
            }
            case IdFCode::DEVICE_SPECIFIC2:
            {
                fCodeInfo = tr("Device-specific (2)");
                break;
            }
            case IdFCode::DEVICE_SPECIFIC3:
            {
                fCodeInfo = tr("Device-specific (3)");
                break;
            }
            case IdFCode::DEVICE_SPECIFIC4:
            {
                fCodeInfo = tr("Device-specific (4)");
                break;
            }
            default:
            {
                break;
            }
        }

        m_msgInfo = tr("[%1] %2").arg(msgTypeInfo).arg(fCodeInfo);
    }

    auto item = new QTableWidgetItem(m_msgInfo);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setItem(m_currentRow, MSG_INFO, item);
}

void LogWindow::setMsgInfo(const QString errorInfo)
{
    // Выводим информацию о кадре ошибки в текстовом формате
    // с шириной поля до ? символов (сколько получится)
    // в формате 'ERROR_FRAME_INFO'
    m_msgInfo = errorInfo;

    auto item = new QTableWidgetItem(m_msgInfo);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setItem(m_currentRow, MSG_INFO, item);
}

void LogWindow::setDefaultMessageFilterSettings()
{
    m_filter.fCodeSettings->fill(true);
    m_filter.msgTypeSettings->fill(true);
    m_filter.slaveAddressSettings->fill(true);
}

bool LogWindow::mustDataFrameBeProcessed(const QCanBusFrame &frame)
{
    bool isFiltrated = true;

    const uint32_t frameId = frame.frameId();

    const uint32_t slaveAddress = getAddressFromId(frameId);
    isFiltrated &= isSlaveAddressFiltrated(slaveAddress);

    const IdMsgTypes msgType = getMsgTypeFromId(frameId);
    isFiltrated &= isMsgTypeFiltrated(msgType);

    const IdFCode fCode = getFCodeFromId(frameId);
    isFiltrated &= isFCodeFiltrated(fCode);

    return isFiltrated;
}

void LogWindow::setSlaveAddressFiltrated(const uint32_t slaveAddress, const bool isFiltrated)
{
    m_filter.slaveAddressSettings->insert(slaveAddress, isFiltrated);
}

bool LogWindow::isSlaveAddressFiltrated(const uint32_t slaveAddress)
{
    return m_filter.slaveAddressSettings->value(slaveAddress);
}

void LogWindow::setMsgTypeFiltrated(const IdMsgTypes msgType, const bool isFiltrated)
{
    m_filter.msgTypeSettings->insert((uint32_t)msgType, isFiltrated);
}

bool LogWindow::isMsgTypeFiltrated(const IdMsgTypes msgType)
{
    return m_filter.msgTypeSettings->value((uint32_t)msgType);
}

void LogWindow::setFCodeFiltrated(const IdFCode fCode, const bool isFiltrated)
{
    m_filter.fCodeSettings->insert((uint32_t)fCode, isFiltrated);
}

bool LogWindow::isFCodeFiltrated(const IdFCode fCode)
{
    return m_filter.fCodeSettings->value((uint32_t)fCode);
}
