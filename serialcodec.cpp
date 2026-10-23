// SPDX-License-Identifier: MPL-2.0

#include "serialcodec.h"

#include <QRegularExpression>

bool SerialCodec::decodeHex(const QString &input, QByteArray *output, HexError *error)
{
    const QString normalized = QString(input).remove(QRegularExpression(QStringLiteral("\\s+")));

    if (normalized.isEmpty()) {
        if (error)
            *error = HexError::Empty;
        return false;
    }

    if (normalized.size() % 2 != 0) {
        if (error)
            *error = HexError::IncompleteByte;
        return false;
    }

    static const QRegularExpression validHex(QStringLiteral("^[0-9A-Fa-f]+$"));
    if (!validHex.match(normalized).hasMatch()) {
        if (error)
            *error = HexError::InvalidCharacter;
        return false;
    }

    if (error)
        *error = HexError::None;
    if (output)
        *output = QByteArray::fromHex(normalized.toLatin1());
    return true;
}

QString SerialCodec::encodeHex(const QByteArray &data)
{
    return data.toHex(' ').toUpper();
}
