#include "plugin.h"

#include "constants.h"
#include "tabbar.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <utils/stylehelper.h>
#include <utils/theme/theme.h>

#include <QBoxLayout>
#include <QFile>
#include <QMainWindow>

// OPENMV-DIFF //
#include <extensionsystem/pluginmanager.h>
// OPENMV-DIFF //

using namespace TabbedEditor::Internal;

TabbedEditorPlugin::TabbedEditorPlugin() :
    m_tabBar(nullptr),
    m_styleUpdatedToBaseColor(false)
{
}

bool TabbedEditorPlugin::initialize(const QStringList &arguments, QString *errorString)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

    // OPENMV-DIFF //
    // connect(Core::ICore::instance(), SIGNAL(themeChanged()), this, SLOT(updateStyleToBaseColor()));
    // connect(Core::EditorManager::instance(), SIGNAL(editorOpened(Core::IEditor*)), SLOT(showTabBar()));
    // OPENMV-DIFF //
#ifdef Q_OS_MAC
    connect(Core::EditorManager::instance(), SIGNAL(editorOpened(Core::IEditor*)), SLOT(showTabBar()));
#else
    connect(Core::ICore::instance(), &Core::ICore::coreOpened, this, &TabbedEditorPlugin::updateStyleToBaseColor);
#endif
    // OPENMV-DIFF //

    QMainWindow *mainWindow = qobject_cast<QMainWindow *>(Core::ICore::mainWindow());
    mainWindow->layout()->setSpacing(0);

    QWidget *wrapper = new QWidget(mainWindow);
    wrapper->setMinimumHeight(0);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tabBar = new TabBar();
    layout->addWidget(m_tabBar);
    layout->addWidget(mainWindow->centralWidget());

    wrapper->setLayout(layout);

    mainWindow->setCentralWidget(wrapper);

    return true;
}

QString TabbedEditorPlugin::getStylesheetPatternFromFile(const QString &filepath)
{
    QFile stylesheetFile(filepath);
    if (!stylesheetFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(stylesheetFile.readAll());
}

void TabbedEditorPlugin::updateStyleToBaseColor()
{
    Utils::Theme *theme = Utils::creatorTheme();

    QString baseColorQss;
    QString borderColorQss;
    QString highlightColorQss;
    QString selectedTabBorderColorQss;
    QString shadowColorQss;

    // OPENMV-DIFF //
    // if(theme->preferredStyles().isEmpty()) {
    // OPENMV-DIFF //
    if(0) { // if(theme->preferredStyles().isEmpty()) {
    // OPENMV-DIFF //
        baseColorQss = getQssStringFromColor(Utils::StyleHelper::baseColor().lighter(130));
        borderColorQss = getQssStringFromColor(Utils::StyleHelper::borderColor());
        highlightColorQss = getQssStringFromColor(Utils::StyleHelper::baseColor());
        selectedTabBorderColorQss
                = getQssStringFromColor(Utils::StyleHelper::highlightColor().lighter());
        shadowColorQss = getQssStringFromColor(Utils::StyleHelper::shadowColor());
    } else { // Flat widget style
        // OPENMV-DIFF //
        // baseColorQss
        //         = getQssStringFromColor(theme->color(Utils::Theme::BackgroundColorHover));
        // borderColorQss = getQssStringFromColor(theme->color(Utils::Theme::BackgroundColorHover));
        // OPENMV-DIFF //
        baseColorQss = getQssStringFromColor(theme->color(Utils::Theme::BackgroundColorDark));
        borderColorQss = getQssStringFromColor(theme->color(Utils::Theme::SplitterColor));
        // OPENMV-DIFF //
        // OPENMV-DIFF //
        // highlightColorQss = getQssStringFromColor(theme->color(Utils::Theme::BackgroundColorDark));
        // selectedTabBorderColorQss
        //         = getQssStringFromColor(theme->color(Utils::Theme::BackgroundColorDark));
        // OPENMV-DIFF //
        highlightColorQss = getQssStringFromColor(theme->color(Utils::Theme::BackgroundColorNormal));
        selectedTabBorderColorQss = getQssStringFromColor(theme->color(Utils::Theme::BackgroundColorHover));
        // OPENMV-DIFF //
        shadowColorQss = getQssStringFromColor(theme->color(Utils::Theme::BackgroundColorNormal));
    }

    QString stylesheetPattern = getStylesheetPatternFromFile(QStringLiteral(":/styles/default.qss"));

    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%FRAME_BACKGROUND_COLOR%"), highlightColorQss);
    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%TAB_SELECTED_BORDER_COLOR%"), selectedTabBorderColorQss);
    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%TAB_SELECTED_BACKGROUND_COLOR%"), baseColorQss);
    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%TAB_SELECTED_BOTTOM_BORDER_COLOR%"), baseColorQss);

    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%TAB_BACKGROUND_COLOR_FROM%"), shadowColorQss);
    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%TAB_BACKGROUND_COLOR_TO%"), shadowColorQss);
    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%TAB_BORDER_COLOR%"), borderColorQss);
    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%TAB_BOTTOM_BORDER_COLOR%"), borderColorQss);
    // OPENMV-DIFF //
    stylesheetPattern = stylesheetPattern.replace(QLatin1String("%TAB_TEXT_COLOR%"),
                                                  getQssStringFromColor(Utils::creatorTheme()->color(Utils::Theme::TextColorNormal)));
    if (!Utils::creatorTheme()->flag(Utils::Theme::DarkUserInterface)) {
        stylesheetPattern = stylesheetPattern.replace(QStringLiteral("close_button_light_grey.png"), QStringLiteral("close_button_dark.png"));
    }
    // OPENMV-DIFF //

    m_tabBar->setStyleSheet(stylesheetPattern);

    // OPENMV-DIFF //
    connect(Core::ICore::instance(), &Core::ICore::saveSettingsRequested, this, [this] {
        Utils::QtcSettings *settings = ExtensionSystem::PluginManager::settings();
        settings->beginGroup("TabbedEditor");
        QStringList positions;
        for (Core::IEditor *editor : m_tabBar->editors()) {
            if (!editor->document()->isTemporary())
            {
                positions << editor->document()->filePath().toString();
            }
        }
        settings->setValue("TabPositions", positions);
        settings->endGroup();
    });

    Utils::QtcSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup("TabbedEditor");
    QStringList positions = settings->value("TabPositions").toStringList();

    for (int i = 0; i < positions.size(); i++) {
        int from = -1;
        for (int j = 0; j < m_tabBar->editors().size(); j++) {
            Core::IEditor *editor = m_tabBar->editors().at(j);
            if (!editor->document()->isTemporary() && editor->document()->filePath().toString() == positions[i]) {
                from = j;
                break;
            }
        }
        if (from != -1) {
            m_tabBar->moveTab(from, i);
        }
    }

    settings->endGroup();
    // OPENMV-DIFF //
}

void TabbedEditorPlugin::showTabBar()
{
    updateStyleToBaseColor();

    if (m_styleUpdatedToBaseColor) {
        disconnect(Core::EditorManager::instance(), SIGNAL(editorOpened(Core::IEditor*)),
                   this, SLOT(showTabBar()));
        return;
    }

    m_styleUpdatedToBaseColor = true;
}

QString TabbedEditorPlugin::getQssStringFromColor(const QColor &color)
{
    return QString::fromLatin1("rgba(%1, %2, %3, %4)").arg(
                QString::number(color.red()),
                QString::number(color.green()),
                QString::number(color.blue()),
                QString::number(color.alpha()));
}
