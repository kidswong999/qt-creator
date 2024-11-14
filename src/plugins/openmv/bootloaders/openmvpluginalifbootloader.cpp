#include "openmvplugin.h"
#include "openmvtr.h"
#include "openmvpluginconnect.h"

namespace OpenMV {
namespace Internal {

void OpenMVPlugin::openmvAlifBootloader(const QString &forceFirmwarePath,
                                        bool forceFlashFSErase,
                                        bool justEraseFlashFs,
                                        Utils::QtcSettings *settings,
                                        QString originalFirmwareFolder,
                                        const QString &selectedDfuDevice)
{
    QJsonObject outObj;

    if(originalFirmwareFolder.isEmpty())
    {
        QList<QPair<int, int> > vidpidlist = alifVidPidList(m_firmwareSettings);
        QMap<QString, QString> mappings;

        for (const QJsonValue &val : m_firmwareSettings.object().value(QStringLiteral("boards")).toArray())
        {
            QJsonObject obj = val.toObject();

            if(obj.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("alif_tools"))
            {
                QString a = val.toObject().value(QStringLiteral("boardDisplayName")).toString();
                QStringList vidpid = val.toObject().value(QStringLiteral("bootloaderVidPid")).toString().split(QStringLiteral(":"));

                if(vidpidlist.contains(QPair<int , int>(vidpid.at(0).toInt(nullptr, 16), vidpid.at(1).toInt(nullptr, 16))))
                {
                    mappings.insert(a, val.toObject().value(QStringLiteral("boardFirmwareFolder")).toString());
                }
            }
        }

        if(!mappings.isEmpty())
        {
            int index = mappings.keys().indexOf(settings->value(LAST_BOARD_TYPE_STATE_ALIF).toString());
            bool ok = mappings.size() == 1;
            QString temp = (mappings.size() == 1) ? mappings.firstKey() : QInputDialog::getItem(Core::ICore::dialogParent(),
                Tr::tr("Connect"), Tr::tr("Please select the board type"),
                mappings.keys(), (index != -1) ? index : 0, false, &ok,
                Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));

            if(ok)
            {
                settings->setValue(LAST_BOARD_TYPE_STATE_ALIF, temp);
                originalFirmwareFolder = mappings.value(temp);
            }
            else
            {
                CONNECT_END();
            }
        }
    }

    if(!originalFirmwareFolder.isEmpty())
    {
        bool foundMatch = false;

        for(const QJsonValue &val : m_firmwareSettings.object().value(QStringLiteral("boards")).toArray())
        {
            QJsonObject obj = val.toObject();

            if((originalFirmwareFolder == obj.value(QStringLiteral("boardFirmwareFolder")).toString())
            && (obj.value(QStringLiteral("bootloaderType")).toString() == QStringLiteral("alif_tools")))
            {
                outObj = obj.value(QStringLiteral("bootloaderSettings")).toObject();
                foundMatch = true;
                break;
            }
        }

        if(!foundMatch)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("No settings for the selected board type %L1!").arg(originalFirmwareFolder));

            CONNECT_END();
        }
    }
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Connect"),
            Tr::tr("No settings found!"));

        CONNECT_END();
    }

    if((!forceFirmwarePath.isEmpty()) || (forceFlashFSErase && justEraseFlashFs))
    {
        if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QMessageBox::information(Core::ICore::dialogParent(),
            Tr::tr("Connect"),
            Tr::tr("Only firmware recovery is supported using the Alif Semiconductor's SE Tools."));

        CONNECT_END();
    }

    if(selectedDfuDevice.isEmpty()) // Shouldn't be possible.
    {
        RECONNECT_END();
    }
    else
    {
        if(alifDownloadFirmware(selectedDfuDevice.split(QStringLiteral(",")).last(), originalFirmwareFolder, outObj))
        {
            if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QMessageBox::information(Core::ICore::dialogParent(),
                Tr::tr("Connect"),
                Tr::tr("Firmware update complete!\n\n") +
                Tr::tr("If you are forcing SBL mode, disconnect your OpenMV Cam from your computer and reset the SBL switch. "
                       "Then reconnect your OpenMV Cam to your computer.\n\n") +
                Tr::tr("Click the Ok button after your OpenMV Cam has enumerated and finished running its built-in self test (blue led blinking - this takes a while).") +
                Tr::tr("\n\nIf you overwrote main.py on your OpenMV Cam and did not erase the disk then your OpenMV Cam will just run that main.py."
                   "\n\nIn this case click OK when you see your OpenMV Cam's internal flash drive mount (a window may or may not pop open)."));

            RECONNECT_WAIT_END();
        }
        else
        {
            CONNECT_END();
        }
    }
}

} // namespace Internal
} // namespace OpenMV
