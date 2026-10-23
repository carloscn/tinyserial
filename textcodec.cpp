#include "textcodec.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif

namespace {

void setError(TextEncodeError *error, TextEncodeError value)
{
    if (error)
        *error = value;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// Qt before 6.5 has no QString constructor for these, only const char *.
QStringDecoder decoderFor(const QString &encoding)
{
    return encoding == QLatin1String("System")
        ? QStringDecoder(QStringConverter::System)
        : QStringDecoder(encoding.toLatin1().constData());
}

QStringEncoder encoderFor(const QString &encoding)
{
    return encoding == QLatin1String("System")
        ? QStringEncoder(QStringConverter::System)
        : QStringEncoder(encoding.toLatin1().constData());
}
#endif

bool appendEncodedText(const QString &text, const QString &encoding, QByteArray *output,
                       TextEncodeError *error)
{
    if (!output)
        return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringEncoder encoder = encoderFor(encoding);
    const QByteArray encoded = encoder.encode(text);
    if (encoder.hasError()) {
        setError(error, TextEncodeError::UnrepresentableCharacter);
        return false;
    }
    output->append(encoded);
    return true;
#else
    QTextCodec *codec = encoding == QLatin1String("System")
        ? QTextCodec::codecForLocale() : QTextCodec::codecForName(encoding.toLatin1());
    if (!codec) {
        setError(error, TextEncodeError::UnsupportedEncoding);
        return false;
    }
    const QByteArray encoded = codec->fromUnicode(text);
    if (codec->toUnicode(encoded) != text) {
        setError(error, TextEncodeError::UnrepresentableCharacter);
        return false;
    }
    output->append(encoded);
    return true;
#endif
}

bool appendEscape(QChar character, const QString &text, int *index, QByteArray *output)
{
    switch (character.unicode()) {
    case 'n': output->append('\n'); return true;
    case 'r': output->append('\r'); return true;
    case 't': output->append('\t'); return true;
    case '0': output->append('\0'); return true;
    case 'a': output->append('\a'); return true;
    case 'b': output->append('\b'); return true;
    case 'f': output->append('\f'); return true;
    case 'v': output->append('\v'); return true;
    case 'e': output->append(char(0x1b)); return true;
    case '\\': output->append('\\'); return true;
    case 'x': {
        if (*index + 2 >= text.size())
            return false;
        const QString hex = text.mid(*index + 1, 2);
        bool valid = false;
        const int value = hex.toInt(&valid, 16);
        if (!valid)
            return false;
        output->append(static_cast<char>(value));
        *index += 2;
        return true;
    }
    default:
        return false;
    }
}

} // namespace

QString decodeTextBytes(const QByteArray &bytes, const QString &encoding)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QStringDecoder decoder = decoderFor(encoding);
    return decoder.decode(bytes);
#else
    QTextCodec *codec = encoding == QLatin1String("System")
        ? QTextCodec::codecForLocale() : QTextCodec::codecForName(encoding.toLatin1());
    return codec ? codec->toUnicode(bytes) : QString::fromUtf8(bytes);
#endif
}

bool encodeTextBytes(const QString &text, const QString &encoding, QByteArray *output,
                     TextEncodeError *error)
{
    if (!output)
        return false;
    output->clear();
    setError(error, TextEncodeError::None);
    return appendEncodedText(text, encoding, output, error);
}

bool encodeTextWithEscapes(const QString &text, const QString &encoding, QByteArray *output,
                           TextEncodeError *error)
{
    if (!output)
        return false;
    output->clear();
    setError(error, TextEncodeError::None);

    QString plain;
    for (int index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (character != QLatin1Char('\\')) {
            plain.append(character);
            continue;
        }
        if (index + 1 >= text.size() || !appendEncodedText(plain, encoding, output, error)) {
            if (index + 1 >= text.size())
                setError(error, TextEncodeError::InvalidEscape);
            return false;
        }
        plain.clear();
        if (!appendEscape(text.at(++index), text, &index, output)) {
            setError(error, TextEncodeError::InvalidEscape);
            return false;
        }
    }
    return appendEncodedText(plain, encoding, output, error);
}
