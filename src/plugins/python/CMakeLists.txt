add_qtc_plugin(Python
  # OPENMV-DIFF #
  # DEPENDS QmlJS
  # PLUGIN_DEPENDS Core LanguageClient ProjectExplorer TextEditor QtSupport
  # OPENMV-DIFF #
  PLUGIN_DEPENDS Core LanguageClient ProjectExplorer TextEditor
  # OPENMV-DIFF #
  SOURCES
    pipsupport.cpp pipsupport.h
    # OPENMV-DIFF #
    # pyside.cpp pyside.h
    # pythonbuildconfiguration.cpp pythonbuildconfiguration.h
    # pysideuicextracompiler.cpp pysideuicextracompiler.h
    # OPENMV-DIFF #
    python.qrc
    # OPENMV-DIFF #
    # pythonbuildsystem.cpp pythonbuildsystem.h
    # OPENMV-DIFF #
    pythonconstants.h
    pythoneditor.cpp pythoneditor.h
    pythonformattoken.h
    pythonhighlighter.cpp pythonhighlighter.h
    pythonindenter.cpp pythonindenter.h
    pythonkitaspect.h pythonkitaspect.cpp
    pythonlanguageclient.cpp pythonlanguageclient.h
    pythonplugin.cpp
    pythonproject.cpp pythonproject.h
    pythonrunconfiguration.cpp pythonrunconfiguration.h
    pythonscanner.cpp pythonscanner.h
    pythonsettings.cpp pythonsettings.h
    pythontr.h
    pythonutils.cpp pythonutils.h
    pythonwizardpage.cpp pythonwizardpage.h
)
