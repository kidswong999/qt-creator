#include "openmvplugin.h"
#include "openmvtr.h"
#include "openmvpluginconnect.h"

namespace OpenMV {
namespace Internal {

void OpenMVPlugin::openmvPictotoolBootloader(bool forceFlashFSErase,
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

        m_iodevice->sysReset(true);
        m_iodevice->close();

        loop.exec();

        QElapsedTimer elaspedTimer;
        elaspedTimer.start();

        while(!elaspedTimer.hasExpired(RESET_TO_DFU_SEARCH_TIME))
        {
            QApplication::processEvents();
        }
    }

    // Erase Flash ////////////////////////////////////////

    QString selectedDfuDeviceVidPid = selectedDfuDevice.isEmpty() ? QString() : selectedDfuDevice.split(QStringLiteral(",")).first();

    QStringList eraseCommands;
    QString binProgramCommand;

    if(selectedDfuDevice.isEmpty())
    {
        bool foundMatch = false;

        for(const QJsonValue &val : m_firmwareSettings.object().value(QStringLiteral("boards")).toArray())
        {
            QJsonObject obj = val.toObject();

            if((m_boardType == obj.value(QStringLiteral("boardType")).toString())
            && (obj.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("picotool")))
            {
                QJsonObject bootloaderSettings = obj.value(QStringLiteral("bootloaderSettings")).toObject();
                QJsonArray eraseCommandsArray = bootloaderSettings.value(QStringLiteral("eraseCommands")).toArray();

                for(const QJsonValue &command : eraseCommandsArray)
                {
                    eraseCommands.append(command.toString());
                }

                binProgramCommand = bootloaderSettings.value(QStringLiteral("binProgramCommand")).toString();
                foundMatch = true;
                break;
            }
        }

        if(!foundMatch)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No PicoTool settings for the selected board type!"));

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
            && (obj.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("picotool")))
            {
                QJsonObject bootloaderSettings = obj.value(QStringLiteral("bootloaderSettings")).toObject();
                QJsonArray eraseCommandsArray = bootloaderSettings.value(QStringLiteral("eraseCommands")).toArray();

                for(const QJsonValue &command : eraseCommandsArray)
                {
                    eraseCommands.append(command.toString());
                }

                binProgramCommand = bootloaderSettings.value(QStringLiteral("binProgramCommand")).toString();
                foundMatch = true;
                break;
            }
        }

        if(!foundMatch)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No PicoTool settings for the selected device!"));

            CONNECT_END();
        }
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
                    picotoolDownloadFirmware(Tr::tr("Erasing Disk"), command, process, QFileInfo(file).canonicalFilePath(), eraseCommands.at(i));

                    if((process.result() != Utils::ProcessResult::FinishedWithSuccess) && (process.result() != Utils::ProcessResult::TerminatedAbnormally))
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
                    picotoolReset(command, process);

                    if((process.result() != Utils::ProcessResult::FinishedWithSuccess) && (process.result() != Utils::ProcessResult::TerminatedAbnormally))
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

    // Program Flash //////////////////////////////////////
    {
        QString command;
        Utils::Process process;
        picotoolDownloadFirmware(Tr::tr("Flashing Firmware"), command, process, QDir::toNativeSeparators(QDir::cleanPath(firmwarePath)), binProgramCommand);

        if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
        {
            if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QMessageBox::information(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("PicoTool firmware update complete!\n\n") +
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
            QMessageBox box(QMessageBox::Critical, Tr::tr("Connect"), Tr::tr("PicoTool firmware update failed!"), QMessageBox::Ok, Core::ICore::dialogParent(),
                Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
            box.setDetailedText(command + QStringLiteral("\n\n") + process.stdOut() + QStringLiteral("\n") + process.stdErr());
            box.setDefaultButton(QMessageBox::Ok);
            box.setEscapeButton(QMessageBox::Cancel);
            box.exec();
            CONNECT_END();
        }
    }
}

} // namespace Internal
} // namespace OpenMV
