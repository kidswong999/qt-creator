// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "androidavdmanager.h"
#include "androidconfigurations.h"
#include "androidtr.h"

#include <coreplugin/icore.h>

#include <utils/qtcprocess.h>

#include <QLoggingCategory>
#include <QMainWindow>
#include <QMessageBox>

using namespace Utils;
using namespace std::chrono_literals;

namespace Android::Internal::AndroidAvdManager {

static Q_LOGGING_CATEGORY(avdManagerLog, "qtc.android.avdManager", QtWarningMsg)

QString startAvd(const QString &name, const std::optional<QFuture<void>> &future)
{
    if (!findAvd(name).isEmpty() || startAvdAsync(name))
        return waitForAvd(name, future);
    return {};
}

static bool is32BitUserSpace()
{
    // Do a similar check as android's emulator is doing:
    if (HostOsInfo::isLinuxHost()) {
        if (QSysInfo::WordSize == 32) {
            Process proc;
            proc.setCommand({"getconf", {"LONG_BIT"}});
            proc.runBlocking(3s);
            if (proc.result() != ProcessResult::FinishedWithSuccess)
                return true;
            return proc.allOutput().trimmed() == "32";
        }
    }
    return false;
}

bool startAvdAsync(const QString &avdName)
{
    const FilePath emulatorPath = AndroidConfig::emulatorToolPath();
    if (!emulatorPath.exists()) {
        QMetaObject::invokeMethod(Core::ICore::mainWindow(), [emulatorPath] {
            QMessageBox::critical(Core::ICore::dialogParent(),
                                  Tr::tr("Emulator Tool Is Missing"),
                                  Tr::tr("Install the missing emulator tool (%1) to the"
                                         " installed Android SDK.")
                                  .arg(emulatorPath.displayName()));
        });
        return false;
    }

    CommandLine cmd(emulatorPath);
    if (is32BitUserSpace())
        cmd.addArg("-force-32bit");
    cmd.addArgs(AndroidConfig::emulatorArgs(), CommandLine::Raw);
    cmd.addArgs({"-avd", avdName});
    qCDebug(avdManagerLog).noquote() << "Running command (startAvdAsync):" << cmd.toUserOutput();
    if (Process::startDetached(cmd, {}, DetachedChannelMode::Discard))
        return true;

    QMetaObject::invokeMethod(Core::ICore::mainWindow(), [avdName] {
        QMessageBox::critical(Core::ICore::dialogParent(), Tr::tr("AVD Start Error"),
                              Tr::tr("Failed to start AVD emulator for \"%1\" device.").arg(avdName));
    });
    return false;
}

QString findAvd(const QString &avdName)
{
    const QStringList lines = AndroidConfig::devicesCommandOutput();
    for (const QString &line : lines) {
        // skip the daemon logs
        if (line.startsWith("* daemon"))
            continue;

        const QString serialNumber = line.left(line.indexOf('\t')).trimmed();
        if (!serialNumber.startsWith("emulator"))
            continue;

        if (AndroidConfig::getAvdName(serialNumber) == avdName)
            return serialNumber;
    }
    return {};
}

static bool waitForBooted(const QString &serialNumber, const std::optional<QFuture<void>> &future)
{
    // found a serial number, now wait until it's done booting...
    for (int i = 0; i < 60; ++i) {
        if (future && future->isCanceled())
            return false;
        if (isAvdBooted(serialNumber))
            return true;
        QThread::sleep(2);
        if (!AndroidConfig::isConnected(serialNumber)) // device was disconnected
            return false;
    }
    return false;
}

QString waitForAvd(const QString &avdName, const std::optional<QFuture<void>> &future)
{
    // we cannot use adb -e wait-for-device, since that doesn't work if a emulator is already running
    // 60 rounds of 2s sleeping, two minutes for the avd to start
    QString serialNumber;
    for (int i = 0; i < 60; ++i) {
        if (future && future->isCanceled())
            return {};
        serialNumber = findAvd(avdName);
        if (!serialNumber.isEmpty())
            return waitForBooted(serialNumber, future) ? serialNumber : QString();
        QThread::sleep(2);
    }
    return {};
}

bool isAvdBooted(const QString &device)
{
    const CommandLine cmd{AndroidConfig::adbToolPath(), {AndroidDeviceInfo::adbSelector(device),
                          "shell", "getprop", "init.svc.bootanim"}};
    qCDebug(avdManagerLog).noquote() << "Running command (isAvdBooted):" << cmd.toUserOutput();
    Process adbProc;
    adbProc.setCommand(cmd);
    adbProc.runBlocking();
    if (adbProc.result() != ProcessResult::FinishedWithSuccess)
        return false;
    return adbProc.allOutput().trimmed() == "stopped";
}

} // namespace Android::Internal::AndroidAvdManager
