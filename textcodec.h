#ifndef TEXTCODEC_H
#define TEXTCODEC_H

#include <QByteArray>
#include <QString>

enum class TextEncodeError {
    None,
    UnsupportedEncoding,
    UnrepresentableCharacter,
    InvalidEscape
};

QString decodeTextBytes(const QByteArray &bytes, const QString &encoding);
bool encodeTextBytes(const QString &text, const QString &encoding, QByteArray *output,
                     TextEncodeError *error = nullptr);
bool encodeTextWithEscapes(const QString &text, const QString &encoding, QByteArray *output,
                           TextEncodeError *error = nullptr);

#endif // TEXTCODEC_H
