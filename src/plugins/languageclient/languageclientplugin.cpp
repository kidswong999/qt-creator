// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "callandtypehierarchy.h"
#include "languageclientmanager.h"
#include "languageclientoutline.h"
#include "languageclientsettings.h"
#include "languageclienttr.h"
#include "snippet.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>

#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>

#include <projectexplorer/taskhub.h>

#include <QAction>
#include <QMenu>

namespace LanguageClient {

class LanguageClientPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "LanguageClient.json")

public:
    LanguageClientPlugin()
    {
        qRegisterMetaType<LanguageServerProtocol::JsonRpcMessage>();
    }

private:
    void initialize() final;
    void extensionsInitialized() final;
    ShutdownFlag aboutToShutdown() final;

    LanguageClientOutlineWidgetFactory m_outlineFactory;
};

void LanguageClientPlugin::initialize()
{
    using namespace Core;

    setupCallHierarchyFactory();
    setupTypeHierarchyFactory();
    setupLanguageClientProjectPanel();
    setupLanguageClientManager(this);

#ifdef WITH_TESTS
    addTestCreator(&createSnippetParsingTest);
#endif

    LanguageClientSettings::registerClientType({Constants::LANGUAGECLIENT_STDIO_SETTINGS_ID,
                                                Tr::tr("Generic StdIO Language Server"),
                                                []() { return new StdIOSettings; }});

    ActionBuilder inspectAction(this, "LanguageClient.InspectLanguageClients");
    inspectAction.setText(Tr::tr("Inspect Language Clients..."));
    // OPENMV-DIFF //
    // inspectAction.addToContainer(Core::Constants::M_TOOLS_DEBUG);
    // OPENMV-DIFF //
    inspectAction.addOnTriggered(this, &LanguageClientManager::showInspector);

    ProjectExplorer::TaskHub::addCategory(
                {Constants::TASK_CATEGORY_DIAGNOSTICS,
                 Tr::tr("Language Server Diagnostics"),
                 Tr::tr("Issues provided by the Language Server in the current document.")});
}

void LanguageClientPlugin::extensionsInitialized()
{
    LanguageClientSettings::init();
}

ExtensionSystem::IPlugin::ShutdownFlag LanguageClientPlugin::aboutToShutdown()
{
    LanguageClientManager::shutdown();
    if (LanguageClientManager::isShutdownFinished())
        return ExtensionSystem::IPlugin::SynchronousShutdown;
    QTC_ASSERT(LanguageClientManager::instance(),
               return ExtensionSystem::IPlugin::SynchronousShutdown);
    connect(LanguageClientManager::instance(), &LanguageClientManager::shutdownFinished,
            this, &ExtensionSystem::IPlugin::asynchronousShutdownFinished);
    return ExtensionSystem::IPlugin::AsynchronousShutdown;
}

} // namespace LanguageClient

#include "languageclientplugin.moc"
