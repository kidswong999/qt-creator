#include <QtCore>
#include <QSerialPortInfo>

#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>

#include "myqserialportinfo.h"

namespace OpenMV {
namespace Internal {

extern QMutex dfu_util_working;

QString serialPortDriveSerialNumber(const QString &portName)
{
// #if defined(Q_OS_WIN)
//     Utils::Process process;
//     std::chrono::seconds timeout(10);
//     process.setTextChannelMode(Utils::Channel::Output, Utils::TextChannelMode::MultiLine);
//     process.setTextChannelMode(Utils::Channel::Error, Utils::TextChannelMode::MultiLine);
//     process.setCommand(Utils::CommandLine(Utils::FilePath::fromString(QStringLiteral("powershell")),
//         QStringList()
//         << QStringLiteral("-Command")
//         << QString(QStringLiteral("(Get-PnpDeviceProperty -InstanceId (Get-WmiObject Win32_SerialPort | Where-Object { $_.DeviceID -eq '%1' }).PNPDeviceId | Where-Object { $_.KeyName -eq 'DEVPKEY_Device_Parent' }).Data")).arg(QString(portName))));
//     process.runBlocking(timeout, Utils::EventLoopMode::On);

//     if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
//     {
//         QRegularExpressionMatch match = QRegularExpression("\\\\(\\w+)$").match(process.stdOut().trimmed());
//         if (match.hasMatch()) return match.captured(1);
//     }
// #elif (defined(Q_OS_LINUX) || defined(Q_OS_MAC))
    MyQSerialPortInfo info = MyQSerialPortInfo(QSerialPortInfo(portName));
    return info.serialNumber();
// #endif

//    return QString();
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
    process.runBlocking(timeout, Utils::EventLoopMode::On);

    if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
    {
        QString serialNumber = process.stdOut().trimmed();
        return serialNumber;
    }
#elif defined(Q_OS_LINUX)
    if(!dfu_util_working.try_lock()) return QString();

    Utils::Process process;
    std::chrono::seconds timeout(10);
    process.setTextChannelMode(Utils::Channel::Output, Utils::TextChannelMode::MultiLine);
    process.setTextChannelMode(Utils::Channel::Error, Utils::TextChannelMode::MultiLine);
    process.setCommand(Utils::CommandLine(Utils::FilePath::fromString(QStringLiteral("lsblk")),
        QStringList()
        << QStringLiteral("-J")
        << QStringLiteral("-o")
        << QStringLiteral("MOUNTPOINT,NAME,SERIAL")));
    process.runBlocking(timeout, Utils::EventLoopMode::On);

    if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
    {
        QJsonDocument doc = QJsonDocument::fromJson(process.stdOut().toUtf8());

        if (!doc.isNull())
        {
            QString cleanDrivePath = QDir::fromNativeSeparators(QDir::cleanPath(drivePath));

            for (const QJsonValue &val : doc.object().value(QStringLiteral("blockdevices")).toArray())
            {
                QJsonObject obj = val.toObject();
                QString serialNumber = obj.value(QStringLiteral("serial")).toString();
                QString mountPoint = QDir::fromNativeSeparators(QDir::cleanPath(obj.value(QStringLiteral("mountpoint")).toString()));

                if (mountPoint == cleanDrivePath)
                {
                    dfu_util_working.unlock();
                    return serialNumber;
                }

                for (const QJsonValue &childVal : obj.value(QStringLiteral("children")).toArray())
                {
                    QJsonObject childObj = childVal.toObject();
                    QString childSerialNumber = childObj.value(QStringLiteral("serial")).toString();
                    QString childMountPoint = QDir::fromNativeSeparators(QDir::cleanPath(childObj.value(QStringLiteral("mountpoint")).toString()));

                    if (childMountPoint == cleanDrivePath)
                    {
                        dfu_util_working.unlock();
                        return childSerialNumber.isEmpty() ? serialNumber : childSerialNumber;
                    }
                }
            }
        }
    }

    dfu_util_working.unlock();
#elif defined(Q_OS_MAC)
    Utils::Process process;
    std::chrono::seconds timeout(10);
    process.setTextChannelMode(Utils::Channel::Output, Utils::TextChannelMode::MultiLine);
    process.setTextChannelMode(Utils::Channel::Error, Utils::TextChannelMode::MultiLine);
    process.setCommand(Utils::CommandLine(Utils::FilePath::fromString(QStringLiteral("system_profiler")), QStringList()
                                          << QStringLiteral("SPUSBDataType")
                                          << QStringLiteral("-json")));
    process.runBlocking(timeout, Utils::EventLoopMode::On);

    if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
    {
        QJsonDocument doc = QJsonDocument::fromJson(process.stdOut().toUtf8());

        if (!doc.isNull())
        {
            for (const QJsonValue &val : doc.object().value(QStringLiteral("SPUSBDataType")).toArray())
            {
                QJsonObject obj = val.toObject();

                for (const QJsonValue &val : obj.value(QStringLiteral("_items")).toArray())
                {
                    QJsonObject obj = val.toObject();
                    QString serialNumber = obj.value(QStringLiteral("serial_num")).toString();

                    for (const QJsonValue &val : obj.value(QStringLiteral("Media")).toArray())
                    {
                        QJsonObject obj = val.toObject();
                        QString diskName = obj.value(QStringLiteral("bsd_name")).toString();

                        if (!diskName.isEmpty())
                        {
                            process.setCommand(Utils::CommandLine(Utils::FilePath::fromString(QStringLiteral("diskutil")), QStringList()
                                                                  << QStringLiteral("list")
                                                                  << QStringLiteral("-plist")
                                                                  << diskName));
                            process.runBlocking(timeout, Utils::EventLoopMode::On);

                            if (process.stdOut().contains(QString(QStringLiteral("<string>%1</string>")).arg(drivePath)))
                            {
                                return serialNumber;
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    return QString();
}

} // namespace Internal
} // namespace OpenMV
