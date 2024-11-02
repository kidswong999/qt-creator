#ifndef OPENMVPLUGINCONNECT_H
#define OPENMVPLUGINCONNECT_H

#include <QtCore>

#include "tools/myqserialportinfo.h"
#include "utils/qtcassert.h"

#define CONNECT_END() \
do { \
    m_working = false; \
    QTimer::singleShot(0, this, &OpenMVPlugin::workingDone); \
    return; \
} while(0)

#define RECONNECT_END() \
do { \
    m_working = false; \
    QTimer::singleShot(0, this, [this] {connectClicked();}); \
    return; \
} while(0)

#define RECONNECT_WAIT_END() \
do { \
    m_working = false; \
    QTimer::singleShot(0, this, [this] {connectClicked(false, QString(), false, false, false, true);}); \
    return; \
} while(0)

#define RECONNECT_AND_FORCEBOOTLOADER_END() \
do { \
    m_working = false; \
    if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, QString(), false, false, false, false, previousMapping);}); \
    else if(m_autoUpdate == QStringLiteral("release")) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, QString(), m_autoErase, false, false, false, previousMapping);}); \
    else if(m_autoUpdate == QStringLiteral("developement")) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, QString(), m_autoErase, false, true, false, previousMapping);}); \
    else if(QFileInfo(m_autoUpdate).isFile()) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, m_autoUpdate, (m_autoErase || m_autoUpdate.endsWith(QStringLiteral(".dfu"), Qt::CaseInsensitive)), false, false, false, previousMapping);}); \
    else if(m_autoErase) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, QString(), true, true, false, false, previousMapping);}); \
    return; \
} while(0)

#define CLOSE_CONNECT_END() \
do { \
    QEventLoop m_loop; \
    connect(m_iodevice, &OpenMVPluginIO::closeResponse, &m_loop, &QEventLoop::quit); \
    m_iodevice->close(); \
    m_loop.exec(); \
    m_working = false; \
    QTimer::singleShot(0, this, &OpenMVPlugin::workingDone); \
    return; \
} while(0)

#define CLOSE_RECONNECT_END() \
do { \
    QEventLoop m_loop; \
    connect(m_iodevice, &OpenMVPluginIO::closeResponse, &m_loop, &QEventLoop::quit); \
    m_iodevice->close(); \
    m_loop.exec(); \
    m_working = false; \
    QTimer::singleShot(0, this, [this] {connectClicked();}); \
    return; \
} while(0)

#define CLOSE_RECONNECT_WAIT_END() \
do { \
    QEventLoop m_loop; \
    connect(m_iodevice, &OpenMVPluginIO::closeResponse, &m_loop, &QEventLoop::quit); \
    m_iodevice->close(); \
    m_loop.exec(); \
    m_working = false; \
    QTimer::singleShot(0, this, [this] {connectClicked(false, QString(), false, false, false, true);}); \
    return; \
} while(0)

#define CLOSE_RECONNECT_AND_FORCEBOOTLOADER_END() \
do { \
    QEventLoop m_loop; \
    connect(m_iodevice, &OpenMVPluginIO::closeResponse, &m_loop, &QEventLoop::quit); \
    m_iodevice->close(); \
    m_loop.exec(); \
    m_working = false; \
    if((m_autoUpdate.isEmpty()) && (!m_autoErase)) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, QString(), false, false, false, false, previousMapping);}); \
    else if(m_autoUpdate == QStringLiteral("release")) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, QString(), m_autoErase, false, false, false, previousMapping);}); \
    else if(m_autoUpdate == QStringLiteral("developement")) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, QString(), m_autoErase, false, true, false, previousMapping);}); \
    else if(QFileInfo(m_autoUpdate).isFile()) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, m_autoUpdate, (m_autoErase || m_autoUpdate.endsWith(QStringLiteral(".dfu"), Qt::CaseInsensitive)), false, false, false, previousMapping);}); \
    else if(m_autoErase) QTimer::singleShot(0, this, [this, previousMapping] {connectClicked(true, QString(), true, true, false, false, previousMapping);}); \
    return; \
} while(0)

namespace OpenMV {
namespace Internal {

bool validPort(const QJsonDocument &settings, const QString &serialNumberFilter, const MyQSerialPortInfo &port);

} // namespace Internal
} // namespace OpenMV

#endif // OPENMVPLUGINCONNECT_H
