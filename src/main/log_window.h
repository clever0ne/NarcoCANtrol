#pragma once

#include <QTableWidget>
#include <QCanBusFrame>
#include <stdint.h>
#include "../cannabus_library/cannabus_common.h"

class LogWindow : public QTableWidget
{
public:
    explicit LogWindow(QWidget *parent = nullptr);
    ~LogWindow();

    void addReceivedMessage(const QCanBusFrame &frame);

public slots:
    void clearLog();

private:
    void makeHeader();
    void setCount();
    void setTime(const uint64_t seconds, const uint64_t microseconds);
    void setMsgType(const cannabus::IdMsgTypes msgType);
    void setSlaveAddress(const uint32_t slaveAddress);
    void setFCode(const cannabus::IdFCode fCode);
    void setDataSize(const uint32_t dataSize);
    void setData(const QByteArray data);
    void setMsgInfo(const cannabus::IdMsgTypes msgType, const cannabus::IdFCode fCode);

    uint64_t m_numberFramesReceived = 0;

    QString m_count;
    QString m_time;
    QString m_msgType;
    QString m_slaveAddress;
    QString m_fCode;
    QString m_dataSize;
    QString m_data;
    QString m_msgInfo;
};
