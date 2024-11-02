#include "openmvplugin.h"
#include "openmvtr.h"
#include "openmvpluginconnect.h"

namespace OpenMV {
namespace Internal {

void OpenMVPlugin::openmvDFUBootloader(bool forceFlashFSErase,
                                       bool justEraseFlashFs,
                                       const QString &firmwarePath,
                                       const QString &selectedDfuDevice)
{
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

        m_iodevice->sysReset(false);
        m_iodevice->close();

        loop.exec();

        QElapsedTimer elaspedTimer;
        elaspedTimer.start();

        while(!elaspedTimer.hasExpired(100))
        {
            QApplication::processEvents();
        }
    }

    QString selectedDfuDeviceVidPid = selectedDfuDevice.isEmpty() ? QString() : selectedDfuDevice.split(QStringLiteral(",")).first();
    QString selectedDfuDeviceSerialNumber = selectedDfuDevice.isEmpty() ? QString() : selectedDfuDevice.split(QStringLiteral(",")).last();

    QString boardTypeToDfuDeviceVidPid;
    QStringList eraseCommands, programCommandsCmd, programCommandsPath;
    QString binProgramCommand;

    QString firmwarePathFileName = QFileInfo(firmwarePath).fileName();

    if(selectedDfuDevice.isEmpty())
    {
        bool foundMatch = false;

        for(const QJsonValue &val : m_firmwareSettings.object().value(QStringLiteral("boards")).toArray())
        {
            QJsonObject obj = val.toObject();

            if(obj.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("openmv_dfu"))
            {
                QJsonObject bootloaderSettings = obj.value(QStringLiteral("bootloaderSettings")).toObject();

                if(m_boardType == obj.value(QStringLiteral("boardType")).toString())
                {
                    boardTypeToDfuDeviceVidPid = obj.value(QStringLiteral("bootloaderVidPid")).toString();

                    QJsonArray eraseCommandsArray = bootloaderSettings.value(QStringLiteral("eraseCommands")).toArray();
                    for(const QJsonValue &command : eraseCommandsArray)
                    {
                        eraseCommands.append(command.toString());
                    }

                    QJsonArray extraProgramCommandsArray = bootloaderSettings.value(QStringLiteral("programCommands")).toArray();
                    for(const QJsonValue &command : extraProgramCommandsArray)
                    {
                        QJsonObject obj2 = command.toObject();
                        programCommandsCmd.append(obj2.value(QStringLiteral("cmd")).toString());
                        programCommandsPath.append(obj2.value(QStringLiteral("path")).toString());
                    }

                    for (const QJsonValue &cmd : bootloaderSettings.value(QStringLiteral("binProgamCommands")).toArray())
                    {
                        if (firmwarePathFileName.toLower() == cmd.toObject().value(QStringLiteral("name")).toString().toLower())
                        {
                            binProgramCommand = cmd.toObject().value(QStringLiteral("cmd")).toString();
                        }
                    }

                    foundMatch = true;
                    break;
                }
            }
        }

        if(!foundMatch)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No DFU settings for the selected board type!") + QString(QStringLiteral("\n\nVID: %1, PID: %2")).arg(m_boardVID).arg(m_boardPID));

            CONNECT_END();
        }

        if (binProgramCommand.isEmpty() && QFileInfo(firmwarePath).exists())
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No matching interface for the selected file name!") + QString(QStringLiteral("\n\nVID: %1, PID: %2")).arg(m_boardVID).arg(m_boardPID));

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
            && (obj.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("openmv_dfu")))
            {
                QJsonObject bootloaderSettings = obj.value(QStringLiteral("bootloaderSettings")).toObject();

                QJsonArray eraseCommandsArray = bootloaderSettings.value(QStringLiteral("eraseCommands")).toArray();
                for(const QJsonValue &command : eraseCommandsArray)
                {
                    eraseCommands.append(command.toString());
                }

                QJsonArray extraProgramCommandsArray = bootloaderSettings.value(QStringLiteral("programCommands")).toArray();
                for(const QJsonValue &command : extraProgramCommandsArray)
                {
                    QJsonObject obj2 = command.toObject();
                    programCommandsCmd.append(obj2.value(QStringLiteral("cmd")).toString());
                    programCommandsPath.append(obj2.value(QStringLiteral("path")).toString());
                }

                for (const QJsonValue &cmd : bootloaderSettings.value(QStringLiteral("binProgamCommands")).toArray())
                {
                    if (firmwarePathFileName.toLower() == cmd.toObject().value(QStringLiteral("name")).toString().toLower())
                    {
                        binProgramCommand = cmd.toObject().value(QStringLiteral("cmd")).toString();
                    }
                }

                foundMatch = true;
                break;
            }
        }

        if(!foundMatch)
        {
            QStringList dfuDeviceVidPidList = selectedDfuDeviceVidPid.split(QLatin1Char(':'));

            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No DFU settings for the selected device!") + QString(QStringLiteral("\n\nVID: %1, PID: %2"))
                                  .arg(dfuDeviceVidPidList.first().toInt(nullptr, 16))
                                  .arg(dfuDeviceVidPidList.last().toInt(nullptr, 16)));

            CONNECT_END();
        }

        if (binProgramCommand.isEmpty() && QFileInfo(firmwarePath).exists())
        {
            QStringList dfuDeviceVidPidList = selectedDfuDeviceVidPid.split(QLatin1Char(':'));

            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No matching interface for the selected file name!") + QString(QStringLiteral("\n\nVID: %1, PID: %2"))
                                  .arg(dfuDeviceVidPidList.first().toInt(nullptr, 16))
                                  .arg(dfuDeviceVidPidList.last().toInt(nullptr, 16)));

            CONNECT_END();
        }
    }

    QString dfuDeviceVidPid = selectedDfuDevice.isEmpty() ? boardTypeToDfuDeviceVidPid : selectedDfuDeviceVidPid;
    QString dfuDeviceSerial = QString(QStringLiteral(" -S %1")).arg(selectedDfuDevice.isEmpty()
        ? QStringLiteral("NULL")
        : selectedDfuDeviceSerialNumber);

    if(dfuDeviceSerial == QStringLiteral(" -S NULL"))
    {
        dfuDeviceSerial = QString();
    }

    if(forceFlashFSErase)
    {
        QTemporaryFile file;

        if(file.open())
        {
            if(file.write(QByteArray(FLASH_SECTOR_ERASE, 0)) == FLASH_SECTOR_ERASE)
            {
                file.setAutoRemove(false);
                file.close();

                QString command;
                Utils::Process process;

                for(int i = 0, j = eraseCommands.size(); i < j; i++)
                {
                    downloadFirmware(Tr::tr("Erasing Disk"), command, process,
                                     QFileInfo(file).canonicalFilePath(),
                                     dfuDeviceVidPid, eraseCommands.at(i) +
                                     ((justEraseFlashFs && ((i + 1) == j)) ? QStringLiteral(" --reset") : QStringLiteral("")) + dfuDeviceSerial);

                    if(((i + 1) != j) && (process.result() != Utils::ProcessResult::FinishedWithSuccess) && (process.result() != Utils::ProcessResult::TerminatedAbnormally))
                    {
                        QMessageBox box(QMessageBox::Critical, Tr::tr("Connect"), Tr::tr("Timeout Error!"), QMessageBox::Ok, Core::ICore::dialogParent(),
                            Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                            (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                        box.setDetailedText(command + QStringLiteral("\n\n") + process.stdOut() + QStringLiteral("\n") + process.stdErr());
                        box.setDefaultButton(QMessageBox::Ok);
                        box.setEscapeButton(QMessageBox::Cancel);
                        box.exec();

                        CONNECT_END();
                    }
                    else if(process.result() == Utils::ProcessResult::TerminatedAbnormally)
                    {
                        CONNECT_END();
                    }
                }

                if(justEraseFlashFs)
                {
                    if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QMessageBox::information(Core::ICore::dialogParent(),
                        Tr::tr("Connect"),
                        QString(QStringLiteral("%1%2%3%4")).arg(Tr::tr("Onboard Data Flash Erased!\n\n"))
                        .arg(Tr::tr("Your OpenMV Cam will start running its built-in self-test if no sd card is attached... this may take a while.\n\n"))
                        .arg(Tr::tr("Click OK when your OpenMV Cam's RGB LED starts blinking blue - which indicates the self-test is complete."))
                        .arg(Tr::tr("\n\nIf you overwrote main.py on your OpenMV Cam and did not erase the disk then your OpenMV Cam will just run that main.py."
                                "\n\nIn this case click OK when you see your OpenMV Cam's internal flash drive mount (a window may or may not pop open).")));

                    RECONNECT_WAIT_END();
                }
            }
            else
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    Tr::tr("Connect"),
                    Tr::tr("Error: %L1!").arg(file.errorString()));

                CONNECT_END();
            }
        }
        else
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("Error: %L1!").arg(file.errorString()));

            CONNECT_END();
        }
    }

    QString command;
    Utils::Process process;

    if (!binProgramCommand.isEmpty())
    {
        downloadFirmware(Tr::tr("Flashing Firmware"), command, process,
                         QDir::toNativeSeparators(QDir::cleanPath(firmwarePath)),
                         dfuDeviceVidPid, binProgramCommand + QStringLiteral(" --reset") + dfuDeviceSerial);

        if(process.result() == Utils::ProcessResult::TerminatedAbnormally)
        {
            CONNECT_END();
        }

        if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QMessageBox::information(Core::ICore::dialogParent(),
            Tr::tr("Connect"),
            Tr::tr("DFU firmware update complete!\n\n") +
            Tr::tr("Click the Ok button after your OpenMV Cam has enumerated and finished running its built-in self test (blue led blinking - this takes a while).") +
            Tr::tr("\n\nIf you overwrote main.py on your OpenMV Cam and did not erase the disk then your OpenMV Cam will just run that main.py."
               "\n\nIn this case click OK when you see your OpenMV Cam's internal flash drive mount (a window may or may not pop open)."));

        RECONNECT_WAIT_END();
    }
    else
    {
        for(int i = 0, j = programCommandsCmd.size(); i < j; i++)
        {
            downloadFirmware(Tr::tr("Flashing Firmware"), command, process,
                             Core::ICore::userResourcePath(QStringLiteral("firmware")).pathAppended(programCommandsPath.at(i)).toString(),
                             dfuDeviceVidPid, programCommandsCmd.at(i) +
                             (((i + 1) == j) ? QStringLiteral(" --reset") : QStringLiteral("")) + dfuDeviceSerial);

            if(((i + 1) != j) && (process.result() != Utils::ProcessResult::FinishedWithSuccess) && (process.result() != Utils::ProcessResult::TerminatedAbnormally))
            {
                QMessageBox box(QMessageBox::Critical, Tr::tr("Connect"), Tr::tr("DFU firmware update failed!"), QMessageBox::Ok, Core::ICore::dialogParent(),
                    Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                    (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                box.setDetailedText(command + QStringLiteral("\n\n") + process.stdOut() + QStringLiteral("\n") + process.stdErr());
                box.setDefaultButton(QMessageBox::Ok);
                box.setEscapeButton(QMessageBox::Cancel);
                box.exec();

                CONNECT_END();
            }
            else if(process.result() == Utils::ProcessResult::TerminatedAbnormally)
            {
                CONNECT_END();
            }

            if((i + 1) == j)
            {
                if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QMessageBox::information(Core::ICore::dialogParent(),
                    Tr::tr("Connect"),
                    Tr::tr("DFU firmware update complete!\n\n") +
                    Tr::tr("Click the Ok button after your OpenMV Cam has enumerated and finished running its built-in self test (blue led blinking - this takes a while).") +
                    Tr::tr("\n\nIf you overwrote main.py on your OpenMV Cam and did not erase the disk then your OpenMV Cam will just run that main.py."
                       "\n\nIn this case click OK when you see your OpenMV Cam's internal flash drive mount (a window may or may not pop open)."));

                RECONNECT_WAIT_END();
            }
        }
    }
}

} // namespace Internal
} // namespace OpenMV
