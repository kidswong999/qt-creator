// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/fileutils.h>
#include <utils/temporarydirectory.h>

#include <languageclient/client.h>
#include <languageclient/languageclientsettings.h>

// OPENMV-DIFF //
#if defined(PYTHON_LIBRARY)
#  define PYTHON_EXPORT Q_DECL_EXPORT
#elif defined(PYTHON_STATIC_LIBRARY)
#  define PYTHON_EXPORT
#else
#  define PYTHON_EXPORT Q_DECL_IMPORT
#endif
// OPENMV-DIFF //

namespace Core { class IDocument; }
namespace ProjectExplorer { class ExtraCompiler; }
namespace TextEditor { class TextDocument; }

namespace Python::Internal {

class PySideUicExtraCompiler;
class PythonLanguageServerState;
class PyLSInterface;

// OPENMV-DIFF //
// class PyLSClient : public LanguageClient::Client
// OPENMV-DIFF //
class PYTHON_EXPORT PyLSClient : public LanguageClient::Client
// OPENMV-DIFF //
{
    Q_OBJECT
public:
    explicit PyLSClient(PyLSInterface *interface);
    ~PyLSClient();

    void openDocument(TextEditor::TextDocument *document) override;
    void projectClosed(ProjectExplorer::Project *project) override;

    void updateExtraCompilers(ProjectExplorer::Project *project,
                              const QList<PySideUicExtraCompiler *> &extraCompilers);

    static PyLSClient *clientForPython(const Utils::FilePath &python);
    void updateConfiguration();

    // OPENMV-DIFF //
    static void setPortPath(const Utils::FilePath &portPath);
    // OPENMV-DIFF //

private:
    void updateExtraCompilerContents(ProjectExplorer::ExtraCompiler *compiler,
                                     const Utils::FilePath &file);
    void closeExtraDoc(const Utils::FilePath &file);
    void closeExtraCompiler(ProjectExplorer::ExtraCompiler *compiler);

    Utils::FilePaths m_extraWorkspaceDirs;
    Utils::FilePath m_extraCompilerOutputDir;

    QHash<ProjectExplorer::Project *, QList<ProjectExplorer::ExtraCompiler *>> m_extraCompilers;
};

void openDocumentWithPython(const Utils::FilePath &python, TextEditor::TextDocument *document);

} // Python::Internal
