// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputpane.h"

// OPENMV-DIFF //
#include <QToolButton>
#include <QAction>
// OPENMV-DIFF //

namespace Core {
class OutputWindow;

namespace Internal {

class MessageOutputWindow : public IOutputPane
{
    Q_OBJECT

public:
    MessageOutputWindow();
    ~MessageOutputWindow() override;

    QWidget *outputWidget(QWidget *parent) override;

    void clearContents() override;

    void append(const QString &text);
    bool canFocus() const override;
    bool hasFocus() const override;
    void setFocus() override;

    bool canNext() const override;
    bool canPrevious() const override;
    void goToNext() override;
    void goToPrev() override;
    bool canNavigate() const override;

    // OPENMV-DIFF //
    QList<QWidget*> toolBarWidgets() const;
    OutputWindow *m_widget;
    QToolButton *m_saveButton;
    QAction *m_saveAction;
    // OPENMV-DIFF //

private:
    void updateFilter() override;
    // OPENMV-DIFF //
    //OutputWindow *m_widget;
    // OPENMV-DIFF //
};

} // namespace Internal
} // namespace Core
