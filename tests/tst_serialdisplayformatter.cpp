#include <QtTest>

#include "serialdisplayformatter.h"

class SerialDisplayFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void formatsAsciiWithoutDecorations();
    void formatsHexWithDirectionAndTimestamp();
};

void SerialDisplayFormatterTest::formatsAsciiWithoutDecorations()
{
    QCOMPARE(formatSerialDisplayData("hello", true, SerialDirection::Receive,
                                     false, false, QTime(1, 2, 3, 4)),
             QStringLiteral("hello"));
}

void SerialDisplayFormatterTest::formatsHexWithDirectionAndTimestamp()
{
    QCOMPARE(formatSerialDisplayData(QByteArray::fromHex("01af"), false,
                                     SerialDirection::Transmit, true, true,
                                     QTime(1, 2, 3, 4)),
             QStringLiteral("<01:02:03.004>tx -> 01 AF"));
}

QTEST_APPLESS_MAIN(SerialDisplayFormatterTest)
#include "tst_serialdisplayformatter.moc"
