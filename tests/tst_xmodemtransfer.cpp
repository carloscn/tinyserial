#include <QtTest>

#include "xmodemtransfer.h"

class XmodemTransferTest : public QObject
{
    Q_OBJECT

private slots:
    void crcMatchesXmodemReference();
    void crcPacketPadsAndChecksumsPayload();
    void checksumPacketUses128ByteFrame();
    void ymodemHeaderUsesBlockZero();
};

void XmodemTransferTest::crcMatchesXmodemReference()
{
    QCOMPARE(XmodemTransfer::crc16(QByteArrayLiteral("123456789")), quint16(0x31c3));
}

void XmodemTransferTest::crcPacketPadsAndChecksumsPayload()
{
    const QByteArray packet = XmodemTransfer::makePacket(1, QByteArrayLiteral("abc"), 1024, true);
    QCOMPARE(packet.size(), 3 + 1024 + 2);
    QCOMPARE(packet.at(0), char(0x02));
    QCOMPARE(static_cast<quint8>(packet.at(1)), quint8(1));
    QCOMPARE(static_cast<quint8>(packet.at(2)), quint8(0xfe));
    QCOMPARE(packet.mid(3, 3), QByteArrayLiteral("abc"));
    QCOMPARE(static_cast<quint8>(packet.at(6)), quint8(0x1a));
}

void XmodemTransferTest::checksumPacketUses128ByteFrame()
{
    const QByteArray packet = XmodemTransfer::makePacket(7, QByteArrayLiteral("A"), 128, false);
    QCOMPARE(packet.size(), 3 + 128 + 1);
    QCOMPARE(packet.at(0), char(0x01));
    QCOMPARE(static_cast<quint8>(packet.at(1)), quint8(7));
    quint8 sum = 0;
    for (int index = 3; index < 3 + 128; ++index)
        sum = static_cast<quint8>(sum + static_cast<quint8>(packet.at(index)));
    QCOMPARE(static_cast<quint8>(packet.back()), sum);
}

void XmodemTransferTest::ymodemHeaderUsesBlockZero()
{
    const QByteArray header("firmware.bin\0" "1234\0", 18);
    const QByteArray packet = XmodemTransfer::makePacket(0, header, 128, true);
    QCOMPARE(static_cast<quint8>(packet.at(1)), quint8(0));
    QCOMPARE(packet.mid(3, header.size()), header);
}

QTEST_APPLESS_MAIN(XmodemTransferTest)
#include "tst_xmodemtransfer.moc"
