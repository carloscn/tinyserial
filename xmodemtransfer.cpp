#include "xmodemtransfer.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>

XmodemTransfer::XmodemTransfer(QObject *parent)
    : QObject(parent), state(State::Idle), mode(Mode::Crc), offset(0), received(0),
      blockNumber(1), retries(0), ymodemHeaderPending(false), ymodemFinalHeader(false),
      ymodemEotSeen(false), ymodemExpectedSize(0), timeout(new QTimer(this))
{
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, this, [this] {
        if (state == State::Idle)
            return;
        if (++retries > MaxRetries) {
            finish(false, QStringLiteral("Transfer timed out after %1 retries.").arg(MaxRetries));
            return;
        }
        if (state == State::WaitingForStart || state == State::Receiving) {
            emit bytesReady(QByteArray(1, mode == Mode::Checksum ? Nak : CrcRequest));
        } else if (state == State::WaitingForEotAck) {
            emit bytesReady(QByteArray(1, Eot));
        } else {
            resendCurrentPacket();
        }
        resetTimeout();
    });
}

bool XmodemTransfer::isActive() const
{
    return state != State::Idle;
}

quint16 XmodemTransfer::crc16(const QByteArray &data)
{
    quint16 crc = 0;
    for (const unsigned char byte : data) {
        crc ^= static_cast<quint16>(byte) << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021)
                                  : static_cast<quint16>(crc << 1);
    }
    return crc;
}

QByteArray XmodemTransfer::makePacket(quint8 block, const QByteArray &data, int blockSize, bool crc)
{
    QByteArray payload = data.left(blockSize);
    payload.append(blockSize - payload.size(), char(0x1a));
    QByteArray packet;
    packet.reserve(3 + blockSize + (crc ? 2 : 1));
    packet.append(blockSize == 1024 ? Stx : Soh);
    packet.append(static_cast<char>(block));
    packet.append(static_cast<char>(0xff - block));
    packet.append(payload);
    if (crc) {
        const quint16 value = crc16(payload);
        packet.append(static_cast<char>(value >> 8));
        packet.append(static_cast<char>(value & 0xff));
    } else {
        quint8 sum = 0;
        for (const unsigned char byte : payload)
            sum = static_cast<quint8>(sum + byte);
        packet.append(static_cast<char>(sum));
    }
    return packet;
}

bool XmodemTransfer::startSending(const QString &fileName, Mode requestedMode)
{
    if (isActive())
        return false;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        emit finished(false, QStringLiteral("Could not open the selected file."));
        return false;
    }
    sendData = file.readAll();
    mode = requestedMode;
    sendFileName = QFileInfo(fileName).fileName();
    offset = 0;
    blockNumber = 1;
    retries = 0;
    ymodemHeaderPending = mode == Mode::Ymodem;
    ymodemFinalHeader = false;
    state = State::WaitingForStart;
    resetTimeout();
    return true;
}

QString XmodemTransfer::partFileName(const QString &destination) const
{
    const QFileInfo info(destination);
    return info.dir().filePath(QStringLiteral(".%1.part").arg(info.fileName()));
}

bool XmodemTransfer::startReceiving(const QString &destination, Mode requestedMode, QString *error)
{
    if (isActive())
        return false;
    if (QFileInfo::exists(destination)) {
        if (error)
            *error = QStringLiteral("The destination file already exists.");
        return false;
    }
    const QString part = partFileName(destination);
    if (QFileInfo::exists(part)) {
        if (error)
            *error = QStringLiteral("A partial download already exists for this file.");
        return false;
    }
    receiveFile.setFileName(part);
    if (!receiveFile.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        if (error)
            *error = QStringLiteral("Could not create the temporary download file.");
        return false;
    }
    mode = requestedMode;
    receiveDestination = destination;
    receiveBuffer.clear();
    received = 0;
    blockNumber = 1;
    retries = 0;
    ymodemEotSeen = false;
    ymodemExpectedSize = 0;
    state = State::Receiving;
    emit bytesReady(QByteArray(1, mode == Mode::Checksum ? Nak : CrcRequest));
    resetTimeout();
    return true;
}

void XmodemTransfer::consume(const QByteArray &bytes)
{
    if (state == State::Idle || bytes.isEmpty())
        return;

    if (state == State::Receiving) {
        receiveBuffer.append(bytes);
        handleReceiveBuffer();
        return;
    }

    for (const char value : bytes) {
        if (value == Can) {
            finish(false, QStringLiteral("Transfer cancelled by the device."));
            return;
        }
        if (state == State::WaitingForStart) {
            if (value != CrcRequest && value != Nak)
                continue;
            if (mode != Mode::Ymodem)
                mode = value == Nak ? Mode::Checksum : mode;
            retries = 0;
            sendNextPacket();
        } else if (state == State::WaitingForAck) {
            if (value == Ack) {
                if (mode == Mode::Ymodem && ymodemHeaderPending) {
                    ymodemHeaderPending = false;
                    state = State::WaitingForStart;
                    resetTimeout();
                    continue;
                }
                if (mode == Mode::Ymodem && ymodemFinalHeader) {
                    finish(true, QStringLiteral("File transfer completed."));
                    return;
                }
                retries = 0;
                offset += (mode == Mode::OneK || mode == Mode::Ymodem ? 1024 : 128);
                ++blockNumber;
                emit progress(qMin(offset, static_cast<qint64>(sendData.size())), sendData.size());
                sendNextPacket();
            } else if (value == Nak) {
                if (++retries > MaxRetries)
                    finish(false, QStringLiteral("The device rejected too many blocks."));
                else
                    resendCurrentPacket();
            }
        } else if (state == State::WaitingForEotAck) {
            if (value == Nak) {
                emit bytesReady(QByteArray(1, Eot));
            } else if (value == Ack && mode == Mode::Ymodem) {
                ymodemFinalHeader = true;
                state = State::WaitingForStart;
                resetTimeout();
            } else if (value == Ack) {
                finish(true, QStringLiteral("File transfer completed."));
            }
        }
    }
}

void XmodemTransfer::sendNextPacket()
{
    if (mode == Mode::Ymodem && (ymodemHeaderPending || ymodemFinalHeader)) {
        QByteArray header;
        if (!ymodemFinalHeader) {
            header = sendFileName.toUtf8();
            header.append('\0');
            header.append(QByteArray::number(sendData.size()));
            header.append('\0');
        }
        currentPacket = makePacket(0, header, 128, true);
        state = State::WaitingForAck;
        emit bytesReady(currentPacket);
        resetTimeout();
        return;
    }
    if (offset >= sendData.size()) {
        state = State::WaitingForEotAck;
        emit bytesReady(QByteArray(1, Eot));
        resetTimeout();
        return;
    }
    const int blockSize = mode == Mode::OneK || mode == Mode::Ymodem ? 1024 : 128;
    currentPacket = makePacket(blockNumber, sendData.mid(offset, blockSize), blockSize,
                               mode != Mode::Checksum);
    state = State::WaitingForAck;
    emit bytesReady(currentPacket);
    resetTimeout();
}

void XmodemTransfer::resendCurrentPacket()
{
    if (!currentPacket.isEmpty())
        emit bytesReady(currentPacket);
}

bool XmodemTransfer::validatePacket(const QByteArray &packet, int blockSize) const
{
    if (packet.size() != 3 + blockSize + (mode == Mode::Checksum ? 1 : 2)
        || static_cast<quint8>(packet.at(1)) + static_cast<quint8>(packet.at(2)) != 0xff)
        return false;
    const QByteArray payload = packet.mid(3, blockSize);
    if (mode == Mode::Checksum) {
        quint8 sum = 0;
        for (const unsigned char byte : payload)
            sum = static_cast<quint8>(sum + byte);
        return sum == static_cast<quint8>(packet.back());
    }
    const quint16 expected = (static_cast<quint8>(packet.at(packet.size() - 2)) << 8)
        | static_cast<quint8>(packet.back());
    return crc16(payload) == expected;
}

void XmodemTransfer::handleReceiveBuffer()
{
    while (!receiveBuffer.isEmpty() && state == State::Receiving) {
        const char first = receiveBuffer.front();
        if (first == Can) {
            finish(false, QStringLiteral("Transfer cancelled by the device."));
            return;
        }
        if (first == Eot) {
            receiveBuffer.remove(0, 1);
            if (mode == Mode::Ymodem && !ymodemEotSeen) {
                ymodemEotSeen = true;
                emit bytesReady(QByteArray(1, Nak));
                continue;
            }
            emit bytesReady(QByteArray(1, Ack));
            if (mode == Mode::Ymodem) {
                emit bytesReady(QByteArray(1, CrcRequest));
                continue;
            }
            receiveFile.close();
            if (!QFile::rename(partFileName(receiveDestination), receiveDestination))
                finish(false, QStringLiteral("Could not finalize the received file."));
            else
                finish(true, QStringLiteral("File transfer completed."));
            return;
        }
        if (first != Soh && first != Stx) {
            receiveBuffer.remove(0, 1);
            continue;
        }
        const int blockSize = first == Stx ? 1024 : 128;
        const int frameSize = 3 + blockSize + (mode == Mode::Checksum ? 1 : 2);
        if (receiveBuffer.size() < frameSize)
            return;
        const QByteArray packet = receiveBuffer.left(frameSize);
        receiveBuffer.remove(0, frameSize);
        if (!validatePacket(packet, blockSize)) {
            emit bytesReady(QByteArray(1, Nak));
            continue;
        }
        const quint8 incoming = static_cast<quint8>(packet.at(1));
        if (mode == Mode::Ymodem && incoming == 0) {
            const QByteArray payload = packet.mid(3, blockSize);
            const int nameEnd = payload.indexOf('\0');
            if (nameEnd == 0) {
                receiveFile.resize(ymodemExpectedSize);
                receiveFile.close();
                emit bytesReady(QByteArray(1, Ack));
                if (!QFile::rename(partFileName(receiveDestination), receiveDestination))
                    finish(false, QStringLiteral("Could not finalize the received file."));
                else
                    finish(true, QStringLiteral("File transfer completed."));
                return;
            }
            const int sizeStart = nameEnd + 1;
            const int sizeEnd = payload.indexOf('\0', sizeStart);
            bool validSize = false;
            ymodemExpectedSize = payload.mid(sizeStart, sizeEnd - sizeStart).toLongLong(&validSize);
            if (!validSize || ymodemExpectedSize < 0) {
                finish(false, QStringLiteral("Invalid YMODEM file header."));
                return;
            }
            blockNumber = 1;
            ymodemEotSeen = false;
            emit bytesReady(QByteArray(1, Ack));
            emit bytesReady(QByteArray(1, CrcRequest));
            resetTimeout();
            continue;
        }
        if (incoming == blockNumber) {
            const QByteArray payload = packet.mid(3, blockSize);
            if (receiveFile.write(payload) != payload.size()) {
                finish(false, QStringLiteral("Could not write the received file."));
                return;
            }
            received += payload.size();
            ++blockNumber;
            emit progress(received, 0);
        }
        if (incoming == blockNumber - 1)
            emit bytesReady(QByteArray(1, Ack));
        else
            emit bytesReady(QByteArray(1, Nak));
        retries = 0;
        resetTimeout();
    }
}

void XmodemTransfer::cancel()
{
    if (!isActive())
        return;
    emit bytesReady(QByteArray(3, Can));
    finish(false, QStringLiteral("File transfer cancelled."));
}

void XmodemTransfer::resetTimeout()
{
    timeout->start(10000);
}

void XmodemTransfer::finish(bool success, const QString &message)
{
    timeout->stop();
    if (receiveFile.isOpen())
        receiveFile.close();
    sendData.clear();
    currentPacket.clear();
    receiveBuffer.clear();
    state = State::Idle;
    emit finished(success, message);
}
