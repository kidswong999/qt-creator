#include "openmvplugin.h"
#include "openmvtr.h"
#include "openmvpluginconnect.h"

namespace OpenMV {
namespace Internal {

void OpenMVPlugin::openmvBossacBootloader(bool forceFlashFSErase,
                                          bool justEraseFlashFs,
                                          const QString &firmwarePath,
                                          const QString &selectedDfuDevice)
{
    // Stopping ///////////////////////////////////////////////////////

    if(selectedDfuDevice.isEmpty())
    {
        QEventLoop loop;

        connect(m_iodevice, &OpenMVPluginIO::closeResponse,
                &loop, &QEventLoop::quit);

        if(!m_portPath.isEmpty())
        {
#if defined(Q_OS_WIN)
            wchar_t driveLetter[m_portPath.size()];
            m_portPath.toWCharArray(driveLetter);

            if(!ejectVolume(driveLetter[0]))
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    Tr::tr("Disconnect"),
                    Tr::tr("Failed to eject \"%L1\"!").arg(m_portPath));
            }
#elif defined(Q_OS_LINUX)
            Utils::Process process;
            std::chrono::seconds timeout(10);
            process.setCommand(Utils::CommandLine(Utils::FilePath::fromString(QStringLiteral("umount")),
                                                  QStringList() << QDir::toNativeSeparators(QDir::cleanPath(m_portPath))));
            process.runBlocking(timeout, Utils::EventLoopMode::On);

            if(process.result() != Utils::ProcessResult::FinishedWithSuccess)
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    Tr::tr("Disconnect"),
                    Tr::tr("Failed to eject \"%L1\"!").arg(m_portPath));
            }
#elif defined(Q_OS_MAC)
            if(sync_volume_np(m_portPath.toUtf8().constData(), SYNC_VOLUME_FULLSYNC | SYNC_VOLUME_WAIT) < 0)
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    Tr::tr("Disconnect"),
                    Tr::tr("Failed to eject \"%L1\"!").arg(m_portPath));
            }
#endif
        }

        m_iodevice->sysReset(!justEraseFlashFs);
        m_iodevice->close();

        loop.exec();

        QElapsedTimer elaspedTimer;
        elaspedTimer.start();

        while(!elaspedTimer.hasExpired(RESET_TO_DFU_SEARCH_TIME))
        {
            QApplication::processEvents();
        }
    }

    QString selectedDfuDeviceVidPid = selectedDfuDevice.isEmpty() ? QString() : selectedDfuDevice;
    QStringList selectedDfuDeviceVidPidList = selectedDfuDeviceVidPid.split(QLatin1Char(':'));
    QString dfuDevicePort;

    bool old = false;
    int oldVid = 0;
    int oldPid = 0;

    for (const QJsonValue &value : m_firmwareSettings.object().value(QStringLiteral("boards")).toArray())
    {
        QJsonObject object = value.toObject();
        QStringList bootloaderVidPid = object.value(QStringLiteral("bootloaderVidPid")).toString().split(QStringLiteral(":"));

        if ((bootloaderVidPid.size() == 2)
        && (object.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("bossac")))
        {
            QJsonObject bootloaderSettings = object.value(QStringLiteral("bootloaderSettings")).toObject();
            QStringList altvidpid = bootloaderSettings.value(QStringLiteral("altvidpid")).toString().split(QStringLiteral(":"));

            if((altvidpid.size() == 2) && (selectedDfuDeviceVidPidList.size() == 2)
            && ((selectedDfuDeviceVidPidList.first().toInt(nullptr, 16) == altvidpid.at(0).toInt(nullptr, 16)))
            && ((selectedDfuDeviceVidPidList.last().toInt(nullptr, 16) == altvidpid.at(1).toInt(nullptr, 16))))
            {
                old = true;
                oldVid = bootloaderVidPid.at(0).toInt(nullptr, 16);
                oldPid = bootloaderVidPid.at(1).toInt(nullptr, 16);
                break;
            }
        }
    }

    if(old)
    {
        for(QSerialPortInfo raw_port : QSerialPortInfo::availablePorts())
        {
            MyQSerialPortInfo port(raw_port);

            if(port.hasVendorIdentifier() && ((port.vendorIdentifier() == selectedDfuDeviceVidPidList.first().toInt(nullptr, 16)))
            && port.hasProductIdentifier() && ((port.productIdentifier() == selectedDfuDeviceVidPidList.last().toInt(nullptr, 16))))
            {
                dfuDevicePort = port.portName();
                break;
            }
        }

        if(dfuDevicePort.isEmpty())
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("BOSSAC device %1 missing!").arg(selectedDfuDeviceVidPid));

            CONNECT_END();
        }

        Utils::Process process;
        bossacRunBootloader(process, dfuDevicePort);

        selectedDfuDeviceVidPid = QString(QStringLiteral("%1:%2").arg(oldVid, 4, 16, QLatin1Char('0')).arg(oldPid, 4, 16, QLatin1Char('0')));

        QElapsedTimer elaspedTimer;
        elaspedTimer.start();
        dfuDevicePort = QString();

        while(dfuDevicePort.isEmpty() && (!elaspedTimer.hasExpired(RESET_TO_DFU_SEARCH_TIME)))
        {
            QApplication::processEvents();

            for(QSerialPortInfo raw_port : QSerialPortInfo::availablePorts())
            {
                MyQSerialPortInfo port(raw_port);

                if (port.hasVendorIdentifier() && port.hasProductIdentifier())
                {
                    for (const QJsonValue &value : m_firmwareSettings.object().value(QStringLiteral("boards")).toArray())
                    {
                        QJsonObject object = value.toObject();

                        if (object.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("bossac"))
                        {
                            QStringList vidpid = object.value(QStringLiteral("bootloaderVidPid")).toString().split(QStringLiteral(":"));

                            if((vidpid.size() == 2)
                            && ((port.vendorIdentifier() == vidpid.at(0).toInt(nullptr, 16)))
                            && ((port.productIdentifier() == vidpid.at(1).toInt(nullptr, 16))))
                            {
                                dfuDevicePort = port.portName();
                                break;
                            }
                        }
                    }
                }

                if (!dfuDevicePort.isEmpty())
                {
                    break;
                }
            }
        }

        if(dfuDevicePort.isEmpty())
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("BOSSAC device %1 missing!").arg(selectedDfuDeviceVidPid));

            CONNECT_END();
        }
    }

    QString boardTypeToDfuDeviceVidPid;
    QString binProgramCommand;

    if(selectedDfuDevice.isEmpty())
    {
        bool foundMatch = false;

        for(const QJsonValue &val : m_firmwareSettings.object().value(QStringLiteral("boards")).toArray())
        {
            QJsonObject obj = val.toObject();

            if((m_boardType == obj.value(QStringLiteral("boardType")).toString())
            && (obj.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("bossac")))
            {
                boardTypeToDfuDeviceVidPid = obj.value(QStringLiteral("bootloaderVidPid")).toString();
                QJsonObject bootloaderSettings = obj.value(QStringLiteral("bootloaderSettings")).toObject();
                binProgramCommand = bootloaderSettings.value(QStringLiteral("binProgramCommand")).toString();
                foundMatch = true;
                break;
            }
        }

        if(!foundMatch)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No BOSSAC settings for the selected board type!"));

            CONNECT_END();
        }
    }
    else
    {
        bool foundMatch = false;

        for(const QJsonValue &val : m_firmwareSettings.object().value(QStringLiteral("boards")).toArray())
        {
            QJsonObject obj = val.toObject();

            if((selectedDfuDeviceVidPid.toLower() == obj.value(QStringLiteral("bootloaderVidPid")).toString().toLower())
            && (obj.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("bossac")))
            {
                QJsonObject bootloaderSettings = obj.value(QStringLiteral("bootloaderSettings")).toObject();
                binProgramCommand = bootloaderSettings.value(QStringLiteral("binProgramCommand")).toString();
                foundMatch = true;
                break;
            }
        }

        if(!foundMatch)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No BOSSAC settings for the selected device!"));

            CONNECT_END();
        }
    }

    if(forceFlashFSErase && justEraseFlashFs)
    {
        if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QMessageBox::information(Core::ICore::dialogParent(),
            Tr::tr("Connect"),
            QString(QStringLiteral("%1"))
            .arg(Tr::tr("Your Nano 33 BLE doesn't have an onboard data flash disk.")));

        if(selectedDfuDevice.isEmpty())
        {
            RECONNECT_WAIT_END();
        }
        else
        {
            RECONNECT_END();
        }
    }

    QString dfuDeviceVidPid = selectedDfuDevice.isEmpty() ? boardTypeToDfuDeviceVidPid : selectedDfuDeviceVidPid;
    QStringList dfuDeviceVidPidList = dfuDeviceVidPid.split(QLatin1Char(':'));

    QElapsedTimer elaspedTimer;
    elaspedTimer.start();
    dfuDevicePort = QString();

    while(dfuDevicePort.isEmpty() && (!elaspedTimer.hasExpired(RESET_TO_DFU_SEARCH_TIME)))
    {
        QApplication::processEvents();

        for(QSerialPortInfo raw_port : QSerialPortInfo::availablePorts())
        {
            MyQSerialPortInfo port(raw_port);

            if(port.hasVendorIdentifier() && ((port.vendorIdentifier() == dfuDeviceVidPidList.first().toInt(nullptr, 16)))
            && port.hasProductIdentifier() && ((port.productIdentifier() == dfuDeviceVidPidList.last().toInt(nullptr, 16))))
            {
                dfuDevicePort = port.portName();
                break;
            }
        }
    }

    if(dfuDevicePort.isEmpty())
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Connect"),
            Tr::tr("BOSSAC device %1 missing!").arg(dfuDeviceVidPid));

        CONNECT_END();
    }

    QString command;
    Utils::Process process;
    bossacDownloadFirmware(Tr::tr("Flashing Firmware"), command, process, QDir::toNativeSeparators(QDir::cleanPath(firmwarePath)), dfuDevicePort, binProgramCommand);

    if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
    {
        if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QMessageBox::information(Core::ICore::dialogParent(),
            Tr::tr("Connect"),
            Tr::tr("BOSSAC firmware update complete!\n\n") +
            Tr::tr("Click the Ok button after your OpenMV Cam has enumerated and finished running its built-in self test (blue led blinking - this takes a while).") +
            Tr::tr("\n\nIf you overwrote main.py on your OpenMV Cam and did not erase the disk then your OpenMV Cam will just run that main.py."
               "\n\nIn this case click OK when you see your OpenMV Cam's internal flash drive mount (a window may or may not pop open)."));

        RECONNECT_WAIT_END();
    }
    else if(process.result() == Utils::ProcessResult::TerminatedAbnormally)
    {
        CONNECT_END();
    }
    else
    {
        QMessageBox box(QMessageBox::Critical, Tr::tr("Connect"), Tr::tr("BOSSAC firmware update failed!"), QMessageBox::Ok, Core::ICore::dialogParent(),
            Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
            (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
        box.setDetailedText(command + QStringLiteral("\n\n") + process.stdOut() + QStringLiteral("\n") + process.stdErr());
        box.setDefaultButton(QMessageBox::Ok);
        box.setEscapeButton(QMessageBox::Cancel);
        box.exec();
        CONNECT_END();
    }
}

} // namespace Internal
} // namespace OpenMV
