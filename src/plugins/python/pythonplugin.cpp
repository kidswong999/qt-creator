// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "pipsupport.h"
#include "pythonbuildconfiguration.h"
#include "pythonconstants.h"
#include "pythoneditor.h"
#include "pythonkitaspect.h"
#include "pythonproject.h"
#include "pythonrunconfiguration.h"
#include "pythonsettings.h"
#include "pythontr.h"
#include "pythonwizardpage.h"

#include <debugger/debuggerruncontrol.h>

#include <extensionsystem/iplugin.h>

#include <projectexplorer/buildtargetinfo.h>
#include <projectexplorer/jsonwizard/jsonwizardfactory.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/taskhub.h>

#include <utils/fsengine/fileiconprovider.h>
#include <utils/theme/theme.h>

using namespace Debugger;
using namespace ProjectExplorer;
using namespace Utils;

namespace Python::Internal {

class PythonPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "Python.json")

    void initialize() final
    {
        setupPythonEditorFactory(this);

        setupPySideBuildStep();
        setupPythonBuildConfiguration();

        setupPythonRunConfiguration();
        setupPythonRunWorker();
        // OPENMV-DIFF //
        // setupPythonDebugWorker();
        // OPENMV-DIFF //
        setupPythonOutputParser();

        setupPythonSettings(this);
        setupPythonWizard();

        setupPipSupport(this);

        KitManager::setIrrelevantAspects(KitManager::irrelevantAspects()
                                         + QSet<Id>{PythonKitAspect::id()});

        ProjectManager::registerProjectType<PythonProject>(Constants::C_PY_PROJECT_MIME_TYPE);
        ProjectManager::registerProjectType<PythonProject>(Constants::C_PY_PROJECT_MIME_TYPE_LEGACY);
    }

    void extensionsInitialized() final
    {
        // Add MIME overlay icons (these icons displayed at Project dock panel)
        const QString imageFile = Utils::creatorTheme()->imageFile(Theme::IconOverlayPro,
                                                               ProjectExplorer::Constants::FILEOVERLAY_PY);
        FileIconProvider::registerIconOverlayForSuffix(imageFile, "py");

        TaskHub::addCategory({PythonErrorTaskCategory,
                              "Python",
                              Tr::tr("Issues parsed from Python runtime output."),
                              true});
    }
};

} // Python::Internal

#include "pythonplugin.moc"
