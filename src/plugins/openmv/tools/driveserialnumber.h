#ifndef DRIVE_SERIAL_NUMBER_H
#define DRIVE_SERIAL_NUMBER_H

#include <QString>

namespace OpenMV {
namespace Internal {

QString serialPortDriveSerialNumber(const QString &portName);
QString driveSerialNumber(const QString &drivePath);

} // namespace Internal
} // namespace OpenMV

#endif // DRIVE_SERIAL_NUMBER_H
