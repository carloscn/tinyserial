#include "serialportselection.h"

int serialPortIndexToKeep(const QStringList &availablePortNames,
                          const QString &selectedPortName,
                          const QString &connectedPortName,
                          bool connected)
{
    return availablePortNames.indexOf(connected ? connectedPortName : selectedPortName);
}
