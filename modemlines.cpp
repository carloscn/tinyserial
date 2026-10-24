// SPDX-License-Identifier: MPL-2.0

#include "modemlines.h"

QString modemLineName(ModemLine line)
{
    switch (line) {
    case ModemLine::Dtr: return QStringLiteral("DTR");
    case ModemLine::Rts: return QStringLiteral("RTS");
    case ModemLine::Cts: return QStringLiteral("CTS");
    case ModemLine::Dsr: return QStringLiteral("DSR");
    case ModemLine::Cd: return QStringLiteral("CD");
    case ModemLine::Ri: return QStringLiteral("RI");
    }
    return QString();
}

QSerialPort::PinoutSignal modemLineSignal(ModemLine line)
{
    switch (line) {
    case ModemLine::Dtr: return QSerialPort::DataTerminalReadySignal;
    case ModemLine::Rts: return QSerialPort::RequestToSendSignal;
    case ModemLine::Cts: return QSerialPort::ClearToSendSignal;
    case ModemLine::Dsr: return QSerialPort::DataSetReadySignal;
    case ModemLine::Cd: return QSerialPort::DataCarrierDetectSignal;
    case ModemLine::Ri: return QSerialPort::RingIndicatorSignal;
    }
    return QSerialPort::NoSignal;
}

bool modemLineActive(QSerialPort::PinoutSignals signalState, ModemLine line)
{
    return signalState.testFlag(modemLineSignal(line));
}

QVector<ModemLine> modemInputLines()
{
    return {ModemLine::Cts, ModemLine::Dsr, ModemLine::Cd, ModemLine::Ri};
}

QVector<ModemLineChange> modemLineChanges(QSerialPort::PinoutSignals before,
                                          QSerialPort::PinoutSignals after,
                                          const QVector<ModemLine> &lines)
{
    QVector<ModemLineChange> changes;
    for (ModemLine line : lines) {
        const bool active = modemLineActive(after, line);
        if (active != modemLineActive(before, line))
            changes.append({line, active});
    }
    return changes;
}
