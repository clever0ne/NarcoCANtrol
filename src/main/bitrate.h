#pragma once

#include <stdint.h>
#include <QString>
#include <QObject>

enum BitRate {
    BITRATE_10000_BPS = 10000,
    BITRATE_20000_BPS = 20000,
    BITRATE_50000_BPS = 50000,
    BITRATE_100000_BPS = 100000,
    BITRATE_125000_BPS = 125000,
    BITRATE_250000_BPS = 250000,
    BITRATE_500000_BPS = 500000,
    BITRATE_800000_BPS = 800000,
    BITRATE_1000000_BPS = 1000000,
    BITRATE_2000000_BPS = 2000000,
    BITRATE_5000000_BPS = 5000000,
    BITRATE_8000000_BPS = 8000000,
};

inline QString bitRateToString(uint32_t bitRate)
{
    QString result = QObject::tr("%1 %2")
            .arg(bitRate / (bitRate < BITRATE_1000000_BPS ? 1000 : 1000000))
            .arg(bitRate < BITRATE_1000000_BPS ? "kbit/s" : "Mbit/s");
    return result;
}
