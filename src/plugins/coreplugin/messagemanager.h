// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"

#include <QStringList>

// OPENMV-DIFF //
#include "messageoutputwindow.h"
// OPENMV-DIFF //

QT_BEGIN_NAMESPACE
class QFont;
QT_END_NAMESPACE

namespace Core::MessageManager {

CORE_EXPORT void setFont(const QFont &font);
CORE_EXPORT void setWheelZoomEnabled(bool enabled);

CORE_EXPORT void writeSilently(const QString &message);
CORE_EXPORT void writeFlashing(const QString &message);
CORE_EXPORT void writeDisrupting(const QString &message);

CORE_EXPORT void writeSilently(const QStringList &messages);
CORE_EXPORT void writeFlashing(const QStringList &messages);
CORE_EXPORT void writeDisrupting(const QStringList &messages);

void init();
void destroy();

// OPENMV-DIFF //
CORE_EXPORT OutputWindow *outputWindow();
CORE_EXPORT void printData(const QByteArray &data);
CORE_EXPORT void grayOutOldContent();
CORE_EXPORT void popup();
CORE_EXPORT void flash();
// OPENMV-DIFF //

} // namespace Core::MessageManager
