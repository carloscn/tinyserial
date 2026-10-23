// SPDX-License-Identifier: MPL-2.0

#include <QtTest>

#include "serialcodec.h"

class SerialCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void decodesWhitespaceSeparatedBytes();
    void acceptsValidInput();
    void rejectsMalformedHex_data();
    void rejectsMalformedHex();
    void encodesUppercaseSpacedBytes();
};

void SerialCodecTest::decodesWhitespaceSeparatedBytes()
{
    QByteArray decoded;
    SerialCodec::HexError error = SerialCodec::HexError::Empty;
    QVERIFY(SerialCodec::decodeHex(QStringLiteral("01 ab\nFF"), &decoded, &error));
    QCOMPARE(decoded, QByteArray::fromHex("01abff"));
    QCOMPARE(error, SerialCodec::HexError::None);
}

void SerialCodecTest::acceptsValidInput()
{
    QByteArray decoded;
    SerialCodec::HexError error = SerialCodec::HexError::Empty;
    QVERIFY(SerialCodec::decodeHex(QStringLiteral("00 ff"), &decoded, &error));
    QCOMPARE(error, SerialCodec::HexError::None);
}

void SerialCodecTest::rejectsMalformedHex_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("expectedError");
    QTest::newRow("empty") << QString() << int(SerialCodec::HexError::Empty);
    QTest::newRow("spaces") << QStringLiteral(" \t\n") << int(SerialCodec::HexError::Empty);
    QTest::newRow("odd digits") << QStringLiteral("ABC") << int(SerialCodec::HexError::IncompleteByte);
    QTest::newRow("invalid character") << QStringLiteral("01 GG") << int(SerialCodec::HexError::InvalidCharacter);
}

void SerialCodecTest::rejectsMalformedHex()
{
    QFETCH(QString, input);
    QFETCH(int, expectedError);
    QByteArray decoded("unchanged");
    SerialCodec::HexError error = SerialCodec::HexError::None;
    QVERIFY(!SerialCodec::decodeHex(input, &decoded, &error));
    QCOMPARE(int(error), expectedError);
    QCOMPARE(decoded, QByteArray("unchanged"));
}

void SerialCodecTest::encodesUppercaseSpacedBytes()
{
    QCOMPARE(SerialCodec::encodeHex(QByteArray::fromHex("01abff")), QStringLiteral("01 AB FF"));
}

QTEST_APPLESS_MAIN(SerialCodecTest)
#include "tst_serialcodec.moc"
