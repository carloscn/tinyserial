// SPDX-License-Identifier: MPL-2.0

#ifndef SERIALCODEC_H
#define SERIALCODEC_H

#include <QByteArray>
#include <QString>

class SerialCodec
{
public:
    enum class HexError {
        None,
        Empty,
        IncompleteByte,
        InvalidCharacter
    };

    // Decodes a whitespace-separated hexadecimal byte stream.  Each byte must
    // have exactly two hexadecimal digits; malformed input is never guessed.
    static bool decodeHex(const QString &input, QByteArray *output, HexError *error = nullptr);
    static QString encodeHex(const QByteArray &data);
};

#endif // SERIALCODEC_H
