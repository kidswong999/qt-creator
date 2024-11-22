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

#include "openmvcamerasettings.h"
#include "ui_openmvcamerasettings.h"

#include "openmvtr.h"

#define SETTINGS_GROUP "BoardConfig"
#define REPL_UART "REPLUart"
#define WIFI_DEBUG "WiFiDebug"

#define WIFI_SETTINGS_GROUP "WiFiConfig"
#define WIFI_MODE "Mode"
#define CLIENT_MODE_SSID "ClientSSID"
#define CLIENT_MODE_PASS "ClientKey"
#define CLIENT_MODE_TYPE "ClientSecurity"
#define CLIENT_MODE_CHANNEL "ClientChannel"
#define ACCESS_POINT_MODE_SSID "AccessPointSSID"
#define ACCESS_POINT_MODE_PASS "AccessPointKey"
#define ACCESS_POINT_MODE_TYPE "AccessPointSecurity"
#define ACESSS_POINT_MODE_CHANNEL "AccessPointChannel"
#define BOARD_NAME "BoardName"

namespace OpenMV {
namespace Internal {

OpenMVCameraSettings::OpenMVCameraSettings(const QString &fileName, QWidget *parent) : QDialog(parent), m_settings(new QSettings(fileName, QSettings::IniFormat, this)), m_ui(new Ui::OpenMVCameraSettings)
{
    m_ui->setupUi(this);
    setWindowFlags(Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                   (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));

    m_settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
    bool repluart = m_settings->value(QStringLiteral(REPL_UART)).toBool();
    bool wifiDebug = m_settings->value(QStringLiteral(WIFI_DEBUG)).toBool();
    m_settings->endGroup();

    m_settings->beginGroup(QStringLiteral(WIFI_SETTINGS_GROUP));
    int wifiMode = m_settings->value(QStringLiteral(WIFI_MODE)).toInt();
    QString clientModeSSID = m_settings->value(QStringLiteral(CLIENT_MODE_SSID)).toString();
    QString clientModePass = m_settings->value(QStringLiteral(CLIENT_MODE_PASS)).toString();
    int clientModeType = m_settings->value(QStringLiteral(CLIENT_MODE_TYPE)).toInt();
    QString accessPointModeSSID = m_settings->value(QStringLiteral(ACCESS_POINT_MODE_SSID)).toString();
    QString accessPointModePass = m_settings->value(QStringLiteral(ACCESS_POINT_MODE_PASS)).toString();
    int accessPointModeType = m_settings->value(QStringLiteral(ACCESS_POINT_MODE_TYPE)).toInt();
    QString boardName = m_settings->value(QStringLiteral(BOARD_NAME)).toString();
    m_settings->endGroup();

    m_ui->wifiSettingsBox->setChecked(wifiDebug);
    m_ui->clientModeButton->setChecked(!wifiMode);
    m_ui->accessPointModeButton->setChecked(wifiMode);

    m_ui->clientModeSSIDEntry->addItem(clientModeSSID.isEmpty() ? Tr::tr("Please enter your WiFi network here") : clientModeSSID);
    m_ui->clientModePasswordEntry->setText(clientModePass);
    m_ui->clientModeTypeEntry->setCurrentIndex(((1 <= clientModeType) && (clientModeType <= 3)) ? (clientModeType - 1) : 0);

    m_ui->accessPointModeSSIDEntry->setText(accessPointModeSSID);
    m_ui->accessPointModePasswordEntry->setText(accessPointModePass);
    m_ui->accessPointModeTypeEntry->setCurrentIndex((accessPointModeType == 3) ? 1 : 0);

    m_ui->boardNameEntry->setText(boardName);

    m_ui->clientModeWidget->setEnabled(m_ui->wifiSettingsBox->isChecked() && m_ui->clientModeButton->isChecked());
    m_ui->accessPointModeWidget->setEnabled(m_ui->wifiSettingsBox->isChecked() && m_ui->accessPointModeButton->isChecked());

    connect(m_ui->wifiSettingsBox, &QGroupBox::clicked, this, [this] {
        m_ui->clientModeWidget->setEnabled(m_ui->wifiSettingsBox->isChecked() && m_ui->clientModeButton->isChecked());
        m_ui->accessPointModeWidget->setEnabled(m_ui->wifiSettingsBox->isChecked() && m_ui->accessPointModeButton->isChecked());
    });

    connect(m_ui->clientModeButton, &QRadioButton::toggled,
            m_ui->clientModeWidget, &QWidget::setEnabled);

    connect(m_ui->accessPointModeButton, &QRadioButton::toggled,
            m_ui->accessPointModeWidget, &QWidget::setEnabled);

    m_ui->replBox->setChecked(repluart);
}

OpenMVCameraSettings::~OpenMVCameraSettings()
{
    delete m_ui;
}

void OpenMVCameraSettings::accept()
{
    m_settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
    m_settings->setValue(QStringLiteral(REPL_UART), m_ui->replBox->isChecked());
    m_settings->setValue(QStringLiteral(WIFI_DEBUG), m_ui->wifiSettingsBox->isChecked());
    m_settings->endGroup();

    m_settings->beginGroup(QStringLiteral(WIFI_SETTINGS_GROUP));
    m_settings->setValue(QStringLiteral(WIFI_MODE), m_ui->accessPointModeButton->isChecked() ? 1 : 0);
    m_settings->setValue(QStringLiteral(CLIENT_MODE_SSID), m_ui->clientModeSSIDEntry->currentText().left(32));
    m_settings->setValue(QStringLiteral(CLIENT_MODE_PASS), m_ui->clientModePasswordEntry->text().left(64));
    m_settings->setValue(QStringLiteral(CLIENT_MODE_TYPE), m_ui->clientModeTypeEntry->currentIndex() + 1);
    m_settings->setValue(QStringLiteral(CLIENT_MODE_CHANNEL), m_settings->value(QStringLiteral(CLIENT_MODE_CHANNEL), 2).toInt());
    m_settings->setValue(QStringLiteral(ACCESS_POINT_MODE_SSID), m_ui->accessPointModeSSIDEntry->text().left(32));
    m_settings->setValue(QStringLiteral(ACCESS_POINT_MODE_PASS), m_ui->accessPointModePasswordEntry->text().left(64));
    m_settings->setValue(QStringLiteral(ACCESS_POINT_MODE_TYPE), m_ui->accessPointModeTypeEntry->currentIndex() ? 3 : 1);
    m_settings->setValue(QStringLiteral(ACESSS_POINT_MODE_CHANNEL), m_settings->value(QStringLiteral(ACESSS_POINT_MODE_CHANNEL), 2).toInt());
    m_settings->setValue(QStringLiteral(BOARD_NAME), m_ui->boardNameEntry->text().left(32).isEmpty() ? QStringLiteral("OpenMV Cam") : m_ui->boardNameEntry->text().left(32));
    m_settings->endGroup();

    QDialog::accept();
}

} // namespace Internal
} // namespace OpenMV
