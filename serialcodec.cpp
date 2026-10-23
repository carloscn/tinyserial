// SPDX-License-Identifier: MPL-2.0

#include "serialcodec.h"

#include <QRegularExpression>

bool SerialCodec::decodeHex(const QString &input, QByteArray *output, QString *errorMessage)
{
    const QString normalized = QString(input).remove(QRegularExpression(QStringLiteral("\\s+")));

    if (normalized.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("HEX data is empty.");
        return false;
    }

    if (normalized.size() % 2 != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("HEX data must contain complete bytes (an even number of digits).");
        return false;
    }

    static const QRegularExpression validHex(QStringLiteral("^[0-9A-Fa-f]+$"));
    if (!validHex.match(normalized).hasMatch()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("HEX data may only contain digits A-F and whitespace.");
        return false;
    }

    if (output)
        *output = QByteArray::fromHex(normalized.toLatin1());
    return true;
}

QString SerialCodec::encodeHex(const QByteArray &data)
{
    return data.toHex(' ').toUpper();
}
