#include <QtTest>

#include "textcodec.h"

class TextCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void utf8RoundTrip();
    void encodesEscapesAsBytes();
    void rejectsInvalidEscape();
};

void TextCodecTest::utf8RoundTrip()
{
    const QString text = QString::fromUtf8("串口测试");
    QByteArray bytes;
    QVERIFY(encodeTextBytes(text, QStringLiteral("UTF-8"), &bytes));
    QCOMPARE(decodeTextBytes(bytes, QStringLiteral("UTF-8")), text);
}

void TextCodecTest::encodesEscapesAsBytes()
{
    QByteArray bytes;
    QVERIFY(encodeTextWithEscapes(QStringLiteral("AT\\r\\n\\x1b"), QStringLiteral("UTF-8"), &bytes));
    QCOMPARE(bytes, QByteArray::fromHex("41540d0a1b"));
}

void TextCodecTest::rejectsInvalidEscape()
{
    QByteArray bytes;
    TextEncodeError error = TextEncodeError::None;
    QVERIFY(!encodeTextWithEscapes(QStringLiteral("\\x1"), QStringLiteral("UTF-8"), &bytes, &error));
    QCOMPARE(error, TextEncodeError::InvalidEscape);
}

QTEST_APPLESS_MAIN(TextCodecTest)
#include "tst_textcodec.moc"
