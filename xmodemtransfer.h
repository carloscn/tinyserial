#ifndef XMODEMTRANSFER_H
#define XMODEMTRANSFER_H

#include <QObject>
#include <QByteArray>
#include <QFile>

class QTimer;

class XmodemTransfer : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Checksum, Crc, OneK, Ymodem };

    explicit XmodemTransfer(QObject *parent = nullptr);

    bool isActive() const;
    bool startSending(const QString &fileName, Mode mode);
    bool startReceiving(const QString &destination, Mode mode, QString *error);
    void consume(const QByteArray &bytes);
    void cancel();

    static quint16 crc16(const QByteArray &data);
    static QByteArray makePacket(quint8 block, const QByteArray &data, int blockSize, bool crc);

signals:
    void bytesReady(const QByteArray &bytes);
    void progress(qint64 completed, qint64 total);
    void finished(bool success, const QString &message);

private:
    enum class State { Idle, WaitingForStart, WaitingForAck, WaitingForEotAck, Receiving };

    static constexpr char Soh = 0x01;
    static constexpr char Stx = 0x02;
    static constexpr char Eot = 0x04;
    static constexpr char Ack = 0x06;
    static constexpr char Nak = 0x15;
    static constexpr char Can = 0x18;
    static constexpr char CrcRequest = 'C';
    static constexpr int MaxRetries = 10;

    void sendNextPacket();
    void resendCurrentPacket();
    void finish(bool success, const QString &message);
    void resetTimeout();
    void handleReceiveBuffer();
    bool validatePacket(const QByteArray &packet, int blockSize) const;
    QString partFileName(const QString &destination) const;

    State state;
    Mode mode;
    QByteArray sendData;
    QByteArray currentPacket;
    QByteArray receiveBuffer;
    QFile receiveFile;
    QString receiveDestination;
    qint64 offset;
    qint64 received;
    quint8 blockNumber;
    int retries;
    bool ymodemHeaderPending;
    bool ymodemFinalHeader;
    bool ymodemEotSeen;
    qint64 ymodemExpectedSize;
    QString sendFileName;
    QTimer *timeout;
};

#endif // XMODEMTRANSFER_H
