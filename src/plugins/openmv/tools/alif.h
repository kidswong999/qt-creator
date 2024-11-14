#ifndef ALIF_H
#define ALIF_H

#include <QList>
#include <QString>

#include <utils/qtcprocess.h>

namespace OpenMV {
namespace Internal {

bool alifSyncTools();
QList<QPair<int, int> > alifVidPidList(const QJsonDocument &settings);
QList<QString> alifGetDevices(const QJsonDocument &settings);
bool alifDownloadFirmware(const QString &port, const QString &originalFirmwareFolder, const QJsonObject &obj);

} // namespace Internal
} // namespace OpenMV

#endif // ALIF_H
