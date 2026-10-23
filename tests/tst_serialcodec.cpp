// SPDX-License-Identifier: MPL-2.0

#include <QtTest>

#include "serialcodec.h"

class SerialCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void decodesWhitespaceSeparatedBytes();
    void rejectsMalformedHex_data();
    void rejectsMalformedHex();
    void encodesUppercaseSpacedBytes();
};

void SerialCodecTest::decodesWhitespaceSeparatedBytes()
{
    QByteArray decoded;
    QString error;
    QVERIFY(SerialCodec::decodeHex(QStringLiteral("01 ab\nFF"), &decoded, &error));
    QCOMPARE(decoded, QByteArray::fromHex("01abff"));
    QVERIFY(error.isEmpty());
}

void SerialCodecTest::rejectsMalformedHex_data()
{
    QTest::addColumn<QString>("input");
    QTest::newRow("empty") << QString();
    QTest::newRow("spaces") << QStringLiteral(" \t\n");
    QTest::newRow("odd digits") << QStringLiteral("ABC");
    QTest::newRow("invalid character") << QStringLiteral("01 GG");
}

void SerialCodecTest::rejectsMalformedHex()
{
    QFETCH(QString, input);
    QByteArray decoded("unchanged");
    QString error;
    QVERIFY(!SerialCodec::decodeHex(input, &decoded, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(decoded, QByteArray("unchanged"));
}

void SerialCodecTest::encodesUppercaseSpacedBytes()
{
    QCOMPARE(SerialCodec::encodeHex(QByteArray::fromHex("01abff")), QStringLiteral("01 AB FF"));
}

QTEST_APPLESS_MAIN(SerialCodecTest)
#include "tst_serialcodec.moc"
