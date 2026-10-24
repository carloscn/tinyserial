// SPDX-License-Identifier: MPL-2.0

#include <QtTest>

#include "modemlines.h"

class ModemLinesTest : public QObject
{
    Q_OBJECT

private slots:
    void namesEveryLine();
    void readsLineState();
    void listsInputsInPanelOrder();
    void reportsOnlyChangedLines();
    void ignoresLinesOutsideTheList();
};

void ModemLinesTest::namesEveryLine()
{
    QCOMPARE(modemLineName(ModemLine::Dtr), QStringLiteral("DTR"));
    QCOMPARE(modemLineName(ModemLine::Rts), QStringLiteral("RTS"));
    QCOMPARE(modemLineName(ModemLine::Cts), QStringLiteral("CTS"));
    QCOMPARE(modemLineName(ModemLine::Dsr), QStringLiteral("DSR"));
    QCOMPARE(modemLineName(ModemLine::Cd), QStringLiteral("CD"));
    QCOMPARE(modemLineName(ModemLine::Ri), QStringLiteral("RI"));
}

void ModemLinesTest::readsLineState()
{
    const QSerialPort::PinoutSignals state =
        QSerialPort::DataTerminalReadySignal | QSerialPort::DataCarrierDetectSignal;
    QVERIFY(modemLineActive(state, ModemLine::Dtr));
    QVERIFY(modemLineActive(state, ModemLine::Cd));
    QVERIFY(!modemLineActive(state, ModemLine::Rts));
    QVERIFY(!modemLineActive(state, ModemLine::Ri));
    QCOMPARE(modemLineSignal(ModemLine::Cts), QSerialPort::ClearToSendSignal);
    QCOMPARE(modemLineSignal(ModemLine::Dsr), QSerialPort::DataSetReadySignal);
}

void ModemLinesTest::listsInputsInPanelOrder()
{
    const QVector<ModemLine> expected = {ModemLine::Cts, ModemLine::Dsr, ModemLine::Cd, ModemLine::Ri};
    QCOMPARE(modemInputLines(), expected);
}

void ModemLinesTest::reportsOnlyChangedLines()
{
    const QSerialPort::PinoutSignals before = QSerialPort::ClearToSendSignal | QSerialPort::DataSetReadySignal;
    const QSerialPort::PinoutSignals after = QSerialPort::DataSetReadySignal | QSerialPort::RingIndicatorSignal;

    const QVector<ModemLineChange> changes = modemLineChanges(before, after, modemInputLines());
    QCOMPARE(changes.size(), 2);
    QCOMPARE(changes.at(0).line, ModemLine::Cts);
    QCOMPARE(changes.at(0).active, false);
    QCOMPARE(changes.at(1).line, ModemLine::Ri);
    QCOMPARE(changes.at(1).active, true);

    QVERIFY(modemLineChanges(after, after, modemInputLines()).isEmpty());
}

void ModemLinesTest::ignoresLinesOutsideTheList()
{
    const QVector<ModemLineChange> changes = modemLineChanges(
        QSerialPort::NoSignal, QSerialPort::DataTerminalReadySignal | QSerialPort::RequestToSendSignal,
        modemInputLines());
    QVERIFY(changes.isEmpty());
}

QTEST_GUILESS_MAIN(ModemLinesTest)
#include "tst_modemlines.moc"
