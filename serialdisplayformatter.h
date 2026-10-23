#ifndef SERIALDISPLAYFORMATTER_H
#define SERIALDISPLAYFORMATTER_H

#include <QByteArray>
#include <QTime>

enum class SerialDirection {
    Receive,
    Transmit
};

QString formatSerialDisplayData(const QByteArray &data, bool ascii,
                                SerialDirection direction, bool showDirection,
                                bool showTimestamp, const QTime &time);
QString formatSerialDisplayText(const QString &text, SerialDirection direction,
                                bool showDirection, bool showTimestamp, const QTime &time);

#endif // SERIALDISPLAYFORMATTER_H
