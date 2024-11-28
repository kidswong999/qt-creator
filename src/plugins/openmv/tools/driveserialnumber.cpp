/* Copyright (C) 2023-2024 OpenMV, LLC.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Any redistribution, use, or modification in source or binary form
 *    is done solely for personal benefit and not for any commercial
 *    purpose or for monetary gain. For commercial licensing options,
 *    please contact openmv@openmv.io
 *
 * THIS SOFTWARE IS PROVIDED BY THE LICENSOR AND COPYRIGHT OWNER "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE LICENSOR OR COPYRIGHT
 * OWNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
//     process.runBlocking(timeout, Utils::EventLoopMode::Off);

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
    process.runBlocking(timeout, Utils::EventLoopMode::Off);

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
    process.runBlocking(timeout, Utils::EventLoopMode::Off);

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
    process.runBlocking(timeout, Utils::EventLoopMode::Off);

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
                            process.runBlocking(timeout, Utils::EventLoopMode::Off);

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
