#include <QtCore>
#include <QSerialPortInfo>

#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>

namespace OpenMV {
namespace Internal {

QString serialPortDriveSerialNumber(const QString &portName)
{
#if defined(Q_OS_WIN)
    Utils::Process process;
    std::chrono::seconds timeout(10);
    process.setTextChannelMode(Utils::Channel::Output, Utils::TextChannelMode::MultiLine);
    process.setTextChannelMode(Utils::Channel::Error, Utils::TextChannelMode::MultiLine);
    process.setCommand(Utils::CommandLine(Utils::FilePath::fromString(QStringLiteral("powershell")),
        QStringList()
        << QStringLiteral("-Command")
        << QString(QStringLiteral("(Get-PnpDeviceProperty -InstanceId (Get-WmiObject Win32_SerialPort | Where-Object { $_.DeviceID -eq '%1' }).PNPDeviceId | Where-Object { $_.KeyName -eq 'DEVPKEY_Device_Parent' }).Data")).arg(QString(portName))));
    process.runBlocking(timeout, Utils::EventLoopMode::Off);

    if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
    {
        QRegularExpressionMatch match = QRegularExpression("\\\\(\\w+)$").match(process.stdOut().trimmed());
        if (match.hasMatch()) return match.captured(1);
    }
#elif defined(Q_OS_LINUX)
#elif defined(Q_OS_MAC)
#endif

    return QString();
}

QString driveSerialNumber(const QString &drivePath)
{
#if defined(Q_OS_WIN)
    Utils::Process process;
    std::chrono::seconds timeout(10);
    process.setTextChannelMode(Utils::Channel::Output, Utils::TextChannelMode::MultiLine);
    process.setTextChannelMode(Utils::Channel::Error, Utils::TextChannelMode::MultiLine);
    process.setCommand(Utils::CommandLine(Utils::FilePath::fromString(QStringLiteral("powershell")),
        QStringList()
        << QStringLiteral("-Command")
        << QString(QStringLiteral("(Get-Disk -Number (Get-Partition -DriveLetter '%1').DiskNumber).SerialNumber")).arg(QString(drivePath).remove(QStringLiteral(":")).remove(QStringLiteral("/")))));
    process.runBlocking(timeout, Utils::EventLoopMode::Off);

    if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
    {
        QString serialNumber = process.stdOut().trimmed();
        return serialNumber;
    }
#elif defined(Q_OS_LINUX)
#elif defined(Q_OS_MAC)
#endif

    return QString();
}

} // namespace Internal
} // namespace OpenMV
