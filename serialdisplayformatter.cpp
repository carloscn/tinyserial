#include "serialdisplayformatter.h"

#include "serialcodec.h"

QString formatSerialDisplayData(const QByteArray &data, bool ascii,
                                SerialDirection direction, bool showDirection,
                                bool showTimestamp, const QTime &time)
{
    return formatSerialDisplayText(ascii ? QString::fromLatin1(data) : SerialCodec::encodeHex(data),
                                   direction, showDirection, showTimestamp, time);
}

QString formatSerialDisplayText(const QString &text, SerialDirection direction,
                                bool showDirection, bool showTimestamp, const QTime &time)
{
    QString result = text;
    if (showDirection)
        result.prepend(direction == SerialDirection::Receive
                           ? QStringLiteral("rx -> ") : QStringLiteral("tx -> "));
    if (showTimestamp)
        result.prepend(QStringLiteral("<%1>").arg(time.toString(QStringLiteral("hh:mm:ss.zzz"))));
    return result;
}
