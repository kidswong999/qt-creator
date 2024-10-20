// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "pythonlanguageclient.h"

#include "pipsupport.h"
#include "pythonbuildconfiguration.h"
#include "pysideuicextracompiler.h"
#include "pythonconstants.h"
#include "pythonproject.h"
#include "pythonsettings.h"
#include "pythontr.h"
#include "pythonutils.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/progressmanager/progressmanager.h>

#include <languageclient/languageclientinterface.h>
#include <languageclient/languageclientmanager.h>
#include <languageserverprotocol/textsynchronization.h>
#include <languageserverprotocol/workspace.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/extracompiler.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include <utils/async.h>
#include <utils/infobar.h>
#include <utils/qtcprocess.h>

#include <QFutureWatcher>
#include <QJsonDocument>
#include <QTimer>

using namespace LanguageClient;
using namespace LanguageServerProtocol;
using namespace ProjectExplorer;
using namespace Utils;

namespace Python::Internal {

static constexpr char installPylsInfoBarId[] = "Python::InstallPyls";
static constexpr char updatePylsInfoBarId[] = "Python::updatePyls";
static constexpr char alwaysUpdateKey[] = "Python/AlwaysUpdatePyls";

class PythonLanguageServerState
{
public:
    enum {
        NotInstallable,
        Installable,
        Updatable,
        Installed
    } state;
    FilePath pylsModulePath;
};

static QHash<FilePath, PyLSClient*> &pythonClients()
{
    static QHash<FilePath, PyLSClient*> clients;
    return clients;
}

static FilePath pyLspPath(const FilePath &python)
{
    const QString version = pythonVersion(python);
    if (!python.needsDevice())
        return Core::ICore::userResourcePath() / "pylsp" / version;
    if (const expected_str<FilePath> tmpDir = python.tmpDir())
        return *tmpDir / "qc-pylsp" / version;
    return {};
}

static PythonLanguageServerState checkPythonLanguageServer(const FilePath &python)
{
    using namespace LanguageClient;
    auto lspPath = pyLspPath(python);
    if (lspPath.isEmpty())
        return {PythonLanguageServerState::NotInstallable, FilePath()};

    Process pythonProcess;
    pythonProcess.setCommand({python, {"-m", "pip", "-V"}});
    using namespace std::chrono_literals;
    pythonProcess.runBlocking(2s);
    bool pipAvailable = pythonProcess.allOutput().startsWith("pip ");

    if (lspPath.pathAppended("bin").pathAppended("pylsp").withExecutableSuffix().exists()) {
        if (pipAvailable) {
            Process pythonProcess;
            Environment env = pythonProcess.environment();
            env.set("PYTHONPATH", lspPath.toUserOutput());
            pythonProcess.setEnvironment(env);
            pythonProcess.setCommand({python, {"-m", "pip", "list", "--outdated", "--format=json"}});
            pythonProcess.runBlocking(20s);
            QString output = pythonProcess.allOutput();

            // Only the first line contains the json data. Following lines might contain warnings.
            if (int index = output.indexOf('\n'); index >= 0)
                output.truncate(index);

            const QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
            for (const QJsonValue &value : doc.array()) {
                if (value.toObject().value("name") == "python-lsp-server")
                    return {PythonLanguageServerState::Updatable, lspPath};
            }
        }
        return {PythonLanguageServerState::Installed, lspPath};
    }

    if (pipAvailable)
        return {PythonLanguageServerState::Installable, lspPath};
    return {PythonLanguageServerState::NotInstallable, FilePath()};
}


class PyLSInterface : public StdIOClientInterface
{
public:
    PyLSInterface()
        : m_extraPythonPath("QtCreator-pyls-XXXXXX")
    { }

    TemporaryDirectory m_extraPythonPath;
protected:
    void startImpl() override
    {
        const FilePath python = m_cmd.executable();
        Environment env = python.deviceEnvironment();
        const FilePath lspPath = pyLspPath(python);
        if (!lspPath.isEmpty() && lspPath.exists() && QTC_GUARD(lspPath.isSameDevice(python))) {
            env.appendOrSet("PYTHONPATH", lspPath.path());
        }
        if (!python.needsDevice()) {
            // todo check where to put this tempdir in remote setups
            env.appendOrSet("PYTHONPATH", m_extraPythonPath.path().toString());
        }
        if (env.hasChanges())
            setEnvironment(env);
        StdIOClientInterface::startImpl();
    }
};

PyLSClient *clientForPython(const FilePath &python)
{
    if (auto client = pythonClients()[python])
        return client;
    auto interface = new PyLSInterface;
    interface->setCommandLine({python, {"-m", "pylsp"}});
    auto client = new PyLSClient(interface);
    client->setName(Tr::tr("Python Language Server (%1)").arg(python.toUserOutput()));
    client->setActivateDocumentAutomatically(true);
    client->updateConfiguration();
    LanguageFilter filter;
    filter.mimeTypes = QStringList() << Constants::C_PY_MIMETYPE << Constants::C_PY3_MIMETYPE;
    client->setSupportedLanguage(filter);
    client->start();
    pythonClients()[python] = client;
    return client;
}

PyLSClient::PyLSClient(PyLSInterface *interface)
    : Client(interface)
    , m_extraCompilerOutputDir(interface->m_extraPythonPath.path())
{
    connect(this, &Client::initialized, this, &PyLSClient::updateConfiguration);
    connect(PythonSettings::instance(), &PythonSettings::pylsConfigurationChanged,
            this, &PyLSClient::updateConfiguration);
    connect(PythonSettings::instance(), &PythonSettings::pylsEnabledChanged,
            this, [this](const bool enabled){
                if (!enabled)
                    LanguageClientManager::shutdownClient(this);
            });

}

PyLSClient::~PyLSClient()
{
    pythonClients().remove(pythonClients().key(this));
}

void PyLSClient::updateConfiguration()
{
    const auto doc = QJsonDocument::fromJson(PythonSettings::pylsConfiguration().toUtf8());
    if (doc.isArray())
        Client::updateConfiguration(doc.array());
    else if (doc.isObject())
        Client::updateConfiguration(doc.object());
}

// OPENMV-DIFF //
static Utils::FilePath m_portPath;
void PyLSClient::setPortPath(const FilePath &portPath) {
    m_portPath = portPath;
}
// OPENMV-DIFF //

void PyLSClient::openDocument(TextEditor::TextDocument *document)
{
    using namespace LanguageServerProtocol;
    if (reachable()) {
        const FilePath documentPath = document->filePath();
        if (PythonProject *project = pythonProjectForFile(documentPath)) {
            if (Target *target = project->activeTarget()) {
                if (BuildConfiguration *buildConfig = target->activeBuildConfiguration()) {
                    if (BuildStepList *buildSteps = buildConfig->buildSteps()) {
                        BuildStep *buildStep = buildSteps->firstStepWithId(PySideBuildStep::id());
                        if (auto *pythonBuildStep = qobject_cast<PySideBuildStep *>(buildStep))
                            updateExtraCompilers(project, pythonBuildStep->extraCompilers());
                    }
                }
            }
        } else if (isSupportedDocument(document)) {
            // OPENMV-DIFF //
            // const FilePath workspacePath = documentPath.parentDir();
            // if (!m_extraWorkspaceDirs.contains(workspacePath)) {
            //     WorkspaceFoldersChangeEvent event;
            //     event.setAdded({WorkSpaceFolder(hostPathToServerUri(workspacePath),
            //                                     workspacePath.fileName())});
            //     DidChangeWorkspaceFoldersParams params;
            //     params.setEvent(event);
            //     DidChangeWorkspaceFoldersNotification change(params);
            //     sendMessage(change);
            //     m_extraWorkspaceDirs.append(workspacePath);
            // }
            // OPENMV-DIFF //
            const FilePath workspacePath = documentPath.parentDir();
            const FilePath workspacePath2 = Core::ICore::userResourcePath(QStringLiteral("micropython-headers"));
            if ((!m_portPath.isEmpty()) && workspacePath.isChildOf(m_portPath)) {
                if (!m_extraWorkspaceDirs.contains(workspacePath) && !m_extraWorkspaceDirs.contains(workspacePath2)) {
                    WorkspaceFoldersChangeEvent event;
                    event.setAdded({WorkSpaceFolder(hostPathToServerUri(workspacePath),
                                                    workspacePath.fileName()),
                                    WorkSpaceFolder(hostPathToServerUri(workspacePath2),
                                                    workspacePath2.fileName())});
                    DidChangeWorkspaceFoldersParams params;
                    params.setEvent(event);
                    DidChangeWorkspaceFoldersNotification change(params);
                    sendMessage(change);
                    m_extraWorkspaceDirs.append(workspacePath);
                    m_extraWorkspaceDirs.append(workspacePath2);
                }
            } else {
                if (!m_extraWorkspaceDirs.contains(workspacePath2)) {
                    WorkspaceFoldersChangeEvent event;
                    event.setAdded({WorkSpaceFolder(hostPathToServerUri(workspacePath2),
                                                    workspacePath2.fileName())});
                    DidChangeWorkspaceFoldersParams params;
                    params.setEvent(event);
                    DidChangeWorkspaceFoldersNotification change(params);
                    sendMessage(change);
                    m_extraWorkspaceDirs.append(workspacePath2);
                }
            }
            // OPENMV-DIFF //
        }
    }
    Client::openDocument(document);
}

void PyLSClient::projectClosed(ProjectExplorer::Project *project)
{
    for (ProjectExplorer::ExtraCompiler *compiler : m_extraCompilers.value(project))
        closeExtraCompiler(compiler);
    Client::projectClosed(project);
}

void PyLSClient::updateExtraCompilers(ProjectExplorer::Project *project,
                                      const QList<PySideUicExtraCompiler *> &extraCompilers)
{
    auto oldCompilers = m_extraCompilers.take(project);
    for (PySideUicExtraCompiler *extraCompiler : extraCompilers) {
        QTC_ASSERT(extraCompiler->targets().size() == 1 , continue);
        int index = oldCompilers.indexOf(extraCompiler);
        if (index < 0) {
            m_extraCompilers[project] << extraCompiler;
            connect(extraCompiler,
                    &ExtraCompiler::contentsChanged,
                    this,
                    [this, extraCompiler](const FilePath &file) {
                        updateExtraCompilerContents(extraCompiler, file);
                    });
            if (extraCompiler->isDirty())
                extraCompiler->compileFile();
        } else {
            m_extraCompilers[project] << oldCompilers.takeAt(index);
        }
    }
    for (ProjectExplorer::ExtraCompiler *compiler : oldCompilers)
        closeExtraCompiler(compiler);
}

void PyLSClient::updateExtraCompilerContents(ExtraCompiler *compiler, const FilePath &file)
{
    const FilePath target = m_extraCompilerOutputDir.pathAppended(file.fileName());

    target.writeFileContents(compiler->content(file));
}

void PyLSClient::closeExtraCompiler(ProjectExplorer::ExtraCompiler *compiler)
{
    const FilePath file = compiler->targets().constFirst();
    m_extraCompilerOutputDir.pathAppended(file.fileName()).removeFile();
    compiler->disconnect(this);
}

PyLSClient *PyLSClient::clientForPython(const FilePath &python)
{
    return pythonClients()[python];
}

class PyLSConfigureAssistant : public QObject
{
public:
    PyLSConfigureAssistant();

    void handlePyLSState(const FilePath &python,
                         const PythonLanguageServerState &state,
                         TextEditor::TextDocument *document);
    void resetEditorInfoBar(TextEditor::TextDocument *document);
    void installPythonLanguageServer(const FilePath &python,
                                     QPointer<TextEditor::TextDocument> document,
                                     const FilePath &pylsPath, bool silent, bool upgrade);
    void openDocument(const FilePath &python, TextEditor::TextDocument *document);

    QHash<FilePath, QList<TextEditor::TextDocument *>> m_infoBarEntries;
    QHash<TextEditor::TextDocument *, QPointer<QFutureWatcher<PythonLanguageServerState>>>
        m_runningChecks;
};

void PyLSConfigureAssistant::installPythonLanguageServer(const FilePath &python,
                                                         QPointer<TextEditor::TextDocument> document,
                                                         const FilePath &pylsPath,
                                                         bool silent,
                                                         bool upgrade)
{
    document->infoBar()->removeInfo(installPylsInfoBarId);

    // Hide all install info bar entries for this python, but keep them in the list
    // so the language server will be setup properly after the installation is done.
    for (TextEditor::TextDocument *additionalDocument : m_infoBarEntries[python])
        additionalDocument->infoBar()->removeInfo(installPylsInfoBarId);

    auto install = new PipInstallTask(python);

    connect(install, &PipInstallTask::finished, this,
            [this, python, document, install](const bool success) {
        const QList<TextEditor::TextDocument *> additionalDocuments = m_infoBarEntries.take(python);
        if (success) {
            if (PyLSClient *client = clientForPython(python)) {
                if (document)
                    LanguageClientManager::openDocumentWithClient(document, client);
                for (TextEditor::TextDocument *additionalDocument : additionalDocuments)
                    LanguageClientManager::openDocumentWithClient(additionalDocument, client);
            }
        }
        install->deleteLater();
    });

    install->setTargetPath(pylsPath);
    install->setPackages({PipPackage{"python-lsp-server[all]", "Python Language Server"}});
    install->setUpgrade(upgrade);
    // OPENMV-DIFF //
    silent = true;
    // OPENMV-DIFF //
    install->setSilent(silent);
    install->run();
}

void PyLSConfigureAssistant::openDocument(const FilePath &python, TextEditor::TextDocument *document)
{
    resetEditorInfoBar(document);
    if (!PythonSettings::pylsEnabled() || !python.exists())
        return;

    if (auto client = pythonClients().value(python)) {
        LanguageClientManager::openDocumentWithClient(document, client);
        return;
    }

    using CheckPylsWatcher = QFutureWatcher<PythonLanguageServerState>;
    QPointer<CheckPylsWatcher> watcher = new CheckPylsWatcher();

    // cancel and delete watcher after a 10 second timeout
    QTimer::singleShot(10000, this, [watcher]() {
        if (watcher) {
            watcher->cancel();
            watcher->deleteLater();
        }
    });

    connect(watcher, &CheckPylsWatcher::resultReadyAt, this,
            [this, watcher, python, document = QPointer<TextEditor::TextDocument>(document)] {
                if (!document || !watcher)
                    return;
                handlePyLSState(python, watcher->result(), document);
            });
    connect(watcher, &CheckPylsWatcher::finished, watcher, &CheckPylsWatcher::deleteLater);
    connect(watcher, &CheckPylsWatcher::finished, this, [this, document] {
        m_runningChecks.remove(document);
    });
    watcher->setFuture(Utils::asyncRun(&checkPythonLanguageServer, python));
    m_runningChecks[document] = watcher;
}

void PyLSConfigureAssistant::handlePyLSState(const FilePath &python,
                                             const PythonLanguageServerState &state,
                                             TextEditor::TextDocument *document)
{
    if (state.state == PythonLanguageServerState::NotInstallable)
        return;

    InfoBar *infoBar = document->infoBar();
    if (state.state == PythonLanguageServerState::Installable
        && infoBar->canInfoBeAdded(installPylsInfoBarId)) {
        // OPENMV-DIFF //
        installPythonLanguageServer(python, document, state.pylsModulePath, false, false);
        return;
        // OPENMV-DIFF //
        auto message = Tr::tr("Install Python language server (PyLS) for %1 (%2). "
                              "The language server provides Python specific completion and annotation.")
                           .arg(pythonName(python), python.toUserOutput());
        InfoBarEntry info(installPylsInfoBarId, message, InfoBarEntry::GlobalSuppression::Enabled);
        info.addCustomButton(Tr::tr("Install"), [this, python, document, state] {
            installPythonLanguageServer(python, document, state.pylsModulePath, false, false);
        });
        infoBar->addInfo(info);
        m_infoBarEntries[python] << document;
    } else if (state.state == PythonLanguageServerState::Updatable) {
        if (infoBar->canInfoBeAdded(updatePylsInfoBarId)) {
            // OPENMV-DIFF //
            installPythonLanguageServer(python, document, state.pylsModulePath, false, true);
            return;
            // OPENMV-DIFF //
            auto message = Tr::tr("Update Python language server (PyLS) for %1 (%2).")
                               .arg(pythonName(python), python.toUserOutput());
            InfoBarEntry info(updatePylsInfoBarId, message);
            info.addCustomButton(Tr::tr("Always Update"), [this, python, document, state] {
                document->infoBar()->removeInfo(updatePylsInfoBarId);
                Core::ICore::settings()->setValue(alwaysUpdateKey, true);
                InfoBar::globallySuppressInfo(updatePylsInfoBarId);
                installPythonLanguageServer(python, document, state.pylsModulePath, false, true);
            });
            info.addCustomButton(Tr::tr("Update"), [this, python, document, state] {
                document->infoBar()->removeInfo(updatePylsInfoBarId);
                installPythonLanguageServer(python, document, state.pylsModulePath, false, true);
            });
            info.addCustomButton(Tr::tr("Never"), [document, python] {
                document->infoBar()->removeInfo(updatePylsInfoBarId);
                InfoBar::globallySuppressInfo(updatePylsInfoBarId);
                if (auto client = clientForPython(python))
                    LanguageClientManager::openDocumentWithClient(document, client);
            });
            info.setCancelButtonInfo([python, document]{
                if (auto client = clientForPython(python))
                    LanguageClientManager::openDocumentWithClient(document, client);
            });
            infoBar->addInfo(info);

            m_infoBarEntries[python] << document;
        } else if (Core::ICore::settings()->value(alwaysUpdateKey, false).toBool()) {
            installPythonLanguageServer(python, document, state.pylsModulePath, true, true);
        } else if (auto client = clientForPython(python)) {
            LanguageClientManager::openDocumentWithClient(document, client);
        }
    } else if (state.state == PythonLanguageServerState::Installed) {
        if (auto client = clientForPython(python))
            LanguageClientManager::openDocumentWithClient(document, client);
    }
}

void PyLSConfigureAssistant::resetEditorInfoBar(TextEditor::TextDocument *document)
{
    for (QList<TextEditor::TextDocument *> &documents : m_infoBarEntries)
        documents.removeAll(document);
    document->infoBar()->removeInfo(installPylsInfoBarId);
    if (auto watcher = m_runningChecks.value(document))
        watcher->cancel();
}

PyLSConfigureAssistant::PyLSConfigureAssistant()
{
    Core::EditorManager::instance();

    connect(Core::EditorManager::instance(),
            &Core::EditorManager::documentClosed,
            this,
            [this](Core::IDocument *document) {
                if (auto textDocument = qobject_cast<TextEditor::TextDocument *>(document))
                    resetEditorInfoBar(textDocument);
            });
}

static PyLSConfigureAssistant &pyLSConfigureAssistant()
{
    static PyLSConfigureAssistant thePyLSConfigureAssistant;
    return thePyLSConfigureAssistant;
}

void openDocumentWithPython(const FilePath &python, TextEditor::TextDocument *document)
{
    pyLSConfigureAssistant().openDocument(python, document);
}

} // Python::Internal
