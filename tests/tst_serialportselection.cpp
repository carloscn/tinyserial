#include <QtTest>

#include "serialportselection.h"

class SerialPortSelectionTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesDisconnectedSelection();
    void preservesConnectedDevice();
    void reportsRemovedDevice();
};

void SerialPortSelectionTest::preservesDisconnectedSelection()
{
    const QStringList ports = {QStringLiteral("ttyUSB0"), QStringLiteral("ttyACM0")};
    QCOMPARE(serialPortIndexToKeep(ports, QStringLiteral("ttyACM0"), QString(), false), 1);
}

void SerialPortSelectionTest::preservesConnectedDevice()
{
    const QStringList ports = {QStringLiteral("ttyUSB0"), QStringLiteral("ttyACM0")};
    QCOMPARE(serialPortIndexToKeep(ports, QStringLiteral("ttyUSB0"),
                                   QStringLiteral("ttyACM0"), true), 1);
}

void SerialPortSelectionTest::reportsRemovedDevice()
{
    QCOMPARE(serialPortIndexToKeep({QStringLiteral("ttyUSB0")}, QString(),
                                   QStringLiteral("ttyACM0"), true), -1);
}

QTEST_APPLESS_MAIN(SerialPortSelectionTest)
#include "tst_serialportselection.moc"
