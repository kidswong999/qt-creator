// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"
#include "icontext.h"

// OPENMV-DIFF //
#include <utils/ansiescapecodehandler.h>
#include "openmvpluginescapecodeparser.h"
// OPENMV-DIFF //
#include <utils/storekey.h>
#include <utils/outputformat.h>

#include <QPlainTextEdit>

namespace Utils {
class OutputFormatter;
class OutputLineParser;
}

namespace Core {

namespace Internal { class OutputWindowPrivate; }

class CORE_EXPORT OutputWindow : public QPlainTextEdit
{
    Q_OBJECT

public:
    enum class FilterModeFlag {
        Default       = 0x00, // Plain text, non case sensitive, for initialization
        RegExp        = 0x01,
        CaseSensitive = 0x02,
        Inverted      = 0x04,
    };
    Q_DECLARE_FLAGS(FilterModeFlags, FilterModeFlag)

    OutputWindow(Context context, const Utils::Key &settingsKey, QWidget *parent = nullptr);
    ~OutputWindow() override;

    void setLineParsers(const QList<Utils::OutputLineParser *> &parsers);
    Utils::OutputFormatter *outputFormatter() const;

    void appendMessage(const QString &out, Utils::OutputFormat format);

    void registerPositionOf(unsigned taskId, int linkedOutputLines, int skipLines, int offset = 0);
    bool knowsPositionOf(unsigned taskId) const;
    void showPositionOf(unsigned taskId);

    void grayOutOldContent();
    void clear();
    void flush();
    void reset();

    void scrollToBottom();

    void setMaxCharCount(int count);
    int maxCharCount() const;

    void setBaseFont(const QFont &newFont);
    float fontZoom() const;
    void setFontZoom(float zoom);
    void resetZoom() { setFontZoom(0); }
    void setWheelZoomEnabled(bool enabled);

    void updateFilterProperties(
            const QString &filterText,
            Qt::CaseSensitivity caseSensitivity,
            bool regexp,
            bool isInverted);

    // OPENMV-DIFF //
    void appendText(const QString &text);
    void save();
    void setTabSettings(int tabWidth);
    OpenMVPluginEscapeCodeParser *getParser() { return m_parser; }
    // OPENMV-DIFF //

    void setOutputFileNameHint(const QString &fileName);

signals:
    void wheelZoom();
    // OPENMV-DIFF //
    void writeBytes(const QByteArray &data);
    // OPENMV-DIFF //

public slots:
    void setWordWrapEnabled(bool wrap);

protected:
    virtual void handleLink(const QPoint &pos);
    virtual void adaptContextMenu(QMenu *menu, const QPoint &pos);

    // OPENMV-DIFF //
    bool isScrollbarAtBottom() const;
    // OPENMV-DIFF //

private:
    QMimeData *createMimeDataFromSelection() const override;
    void keyPressEvent(QKeyEvent *ev) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *) override;
    void wheelEvent(QWheelEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    using QPlainTextEdit::setFont; // call setBaseFont instead, which respects the zoom factor
    void enableUndoRedo();
    void filterNewContent();
    void handleNextOutputChunk();
    void handleOutputChunk(const QString &output, Utils::OutputFormat format);
    void updateAutoScroll();

    // OPENMV-DIFF //
    QString doNewlineEnforcement(const QString &out);

    Utils::AnsiEscapeCodeHandler m_handler;
    OpenMVPluginEscapeCodeParser *m_parser;
    // OPENMV-DIFF //

    Internal::OutputWindowPrivate *d = nullptr;
};

} // namespace Core
