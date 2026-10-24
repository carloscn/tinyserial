// SPDX-License-Identifier: MPL-2.0

#ifndef MODEMLINES_H
#define MODEMLINES_H

#include <QSerialPort>
#include <QString>
#include <QVector>

// The RS-232 control lines TinySerial shows.  DTR and RTS are outputs the
// user can switch; CTS, DSR, CD and RI are inputs driven by the device.
enum class ModemLine {
    Dtr,
    Rts,
    Cts,
    Dsr,
    Cd,
    Ri
};

struct ModemLineChange
{
    ModemLine line;
    bool active;
};

QString modemLineName(ModemLine line);
QSerialPort::PinoutSignal modemLineSignal(ModemLine line);
bool modemLineActive(QSerialPort::PinoutSignals signalState, ModemLine line);
QVector<ModemLine> modemInputLines();

// Lines among `lines` whose state differs between the two readings, in the
// order given.
QVector<ModemLineChange> modemLineChanges(QSerialPort::PinoutSignals before,
                                          QSerialPort::PinoutSignals after,
                                          const QVector<ModemLine> &lines);

#endif // MODEMLINES_H
