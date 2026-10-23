#ifndef SERIALPORTSELECTION_H
#define SERIALPORTSELECTION_H

#include <QStringList>

int serialPortIndexToKeep(const QStringList &availablePortNames,
                          const QString &selectedPortName,
                          const QString &connectedPortName,
                          bool connected);

#endif // SERIALPORTSELECTION_H
