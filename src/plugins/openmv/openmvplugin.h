#ifndef OPENMVPLUGIN_H
#define OPENMVPLUGIN_H

#include <QtConcurrent>
#include <QtCore>
#include <QtGui>
#include <QtGui/private/qzipreader_p.h>
#include <QtGui/private/qzipwriter_p.h>
#include <QtNetwork>
#include <QtWidgets>

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/fileutils.h>
#include <coreplugin/fancyactionbar.h>
#include <coreplugin/fancytabwidget.h>
#include <coreplugin/icore.h>
#include <coreplugin/mainwindow.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/openmvpluginescapecodeparser.h>
#include <coreplugin/outputwindow.h>
#include <syntax-highlighting/src/lib/definition_p.h>
#include <syntax-highlighting/src/lib/keywordlist_p.h>
#include <texteditor/highlighter.h>
#include <texteditor/codeassist/completionassistprovider.h>
#include <texteditor/codeassist/keywordscompletionassist.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>
#include <extensionsystem/pluginspec.h>
#include <utils/appmainwindow.h>
#include <utils/checkablemessagebox.h>
#include <utils/elidinglabel.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/utilsicons.h>
#include <utils/pathchooser.h>
#include <utils/proxyaction.h>
#include <utils/styledbar.h>
#include <utils/qtcprocess.h>
#include <utils/theme/theme.h>
#include <utils/tooltip/tooltip.h>

#if defined(Q_OS_WIN)
    #include "openmveject.h"
#elif defined(Q_OS_LINUX)
    #include <dirent.h>
    #include <unistd.h>
#elif defined(Q_OS_MAC)
    #include <unistd.h>
#endif

#include "openmvpluginserialport.h"
#include "openmvpluginio.h"
#include "openmvpluginfb.h"
#include "openmvterminal.h"
#include "openmvcamerasettings.h"
#include "openmvdataseteditor.h"
#include "histogram/openmvpluginhistogram.h"
#include "tools/bossac.h"
#include "tools/dfu-util.h"
#include "tools/edgeimpulse.h"
#include "tools/imx.h"
#include "tools/keypointseditor.h"
#include "tools/myqserialportinfo.h"
#include "tools/picotool.h"
#include "tools/tag16h5.h"
#include "tools/tag25h7.h"
#include "tools/tag25h9.h"
#include "tools/tag36h10.h"
#include "tools/tag36h11.h"
#include "tools/tag36artoolkit.h"
#include "tools/thresholdeditor.h"
#include "tools/videotools.h"

#define LIGHT_SPLASH_PATH ":/openmv/openmv-media/splash/openmv-splash/splash-small.png"
#define LIGHT_SPLASH_HIDPI_PATH ":/openmv/openmv-media/splash/openmv-splash/splash-large.png"
#define DARK_SPLASH_PATH ":/openmv/openmv-media/splash/openmv-splash-slate/splash-small.png"
#define DARK_SPLASH_HIDPI_PATH ":/openmv/openmv-media/splash/openmv-splash-slate/splash-large.png"
#define CONNECT_PATH ":/openmv/images/connect.png"
#define CONNECT_USB_DARK_PATH ":/openmv/images/connect-usb-dark.png"
#define CONNECT_WIFI_DARK_PATH ":/openmv/images/connect-wifi-dark.png"
#define CONNECT_USB_WIFI_DARK_PATH ":/openmv/images/connect-usb-wifi-dark.png"
#define CONNECT_USB_LIGHT_PATH ":/openmv/images/connect-usb-light.png"
#define CONNECT_WIFI_LIGHT_PATH ":/openmv/images/connect-wifi-light.png"
#define CONNECT_USB_WIFI_LIGHT_PATH ":/openmv/images/connect-usb-wifi-light.png"
#define DISCONNECT_PATH ":/openmv/images/disconnect.png"
#define START_PATH ":/openmv/projectexplorer/images/run.png"
#define STOP_PATH ":/openmv/images/application-exit.png"
#define NEW_FOLDER_PATH ":/openmv/images/new-folder.png"
#define SNAPSHOT_PATH ":/openmv/images/snapshot.png"

#define SETTINGS_GROUP "OpenMV"
#define EDITOR_MANAGER_STATE "EditorManagerState"
#define MSPLITTER_STATE "MSplitterState"
#define HSPLITTER_STATE "HSplitterState"
#define VSPLITTER_STATE "VSplitterState"
#define AUTO_RECONNECT_STATE "AutoReconnectState"
#define STOP_SCRIPT_CONNECT_DISCONNECT_STATE "StopScriptConnectDisconnect"
#define ENABLE_SYNCING_IMPORTS_STATE "EnableSyncingImports"
#define ENABLE_FILTERING_EXAMPLES_STATE "EnableFilteringExamples"
#define ZOOM_STATE "ZoomState"
#define OUTPUT_WINDOW_FONT_ZOOM_STATE "OutputWindowFontZoomState"
#define JPG_COMPRESS_STATE "JPGCompressState"
#define DISABLE_FRAME_BUFFER_STATE "DisableFrameBufferState"
#define HISTOGRAM_COLOR_SPACE_STATE "HistogramColorSpace"
#define DONT_SHOW_EXAMPLES_AGAIN "DontShowExamplesAgain"
#define DONT_SHOW_LED_STATES_AGAIN "DontShowLEDStatesAgain"
#define DONT_SHOW_UPGRADE_FW_AGAIN "DontShowUpgradeFWAgain"
#define LAST_FORM_KEY "LastFormKey"
#define LAST_FIRMWARE_PATH "LastFirmwarePath"
#define LAST_FLASH_FS_ERASE_STATE "LastFlashFSEraseState"
#define LAST_DFU_ACTION "LastDFUAction"
#define LAST_DFU_FLASH_FS_ERASE_STATE "LastDFUFlashFSEraseState"
#define LAST_BOARD_TYPE_STATE "LastBoardTypeState"
#define LAST_SERIAL_PORT_STATE "LastSerialPortState"
#define LAST_DFU_PORT_STATE "LastDFUPortState"
#define LAST_SAVE_IMAGE_PATH "LastSaveImagePath"
#define LAST_SAVE_TEMPLATE_PATH "LastSaveTemplatePath"
#define LAST_SAVE_DESCRIPTOR_PATH "LastSaveDescriptorPath"
#define LAST_OPEN_TERMINAL_SELECT "LastOpenTerminalSelect"
#define LAST_OPEN_TERMINAL_SERIAL_PORT "LastOpenTerminalSerialPort"
#define LAST_OPEN_TERMINAL_SERIAL_PORT_BAUD_RATE "LastOpenTerminalSerialPortBaudRate"
#define LAST_OPEN_TERMINAL_UDP_TYPE_SELECT "LastOpenTerminalUDPTypeSelect"
#define LAST_OPEN_TERMINAL_UDP_PORT "LastOpenTerminalUDPPort"
#define LAST_OPEN_TERMINAL_UDP_SERVER_PORT "LastOpenTerminalSereverUDPPort"
#define LAST_OPEN_TERMINAL_TCP_TYPE_SELECT "LastOpenTerminalTCPTypeSelect"
#define LAST_OPEN_TERMINAL_TCP_PORT "LastOpenTerminalTCPPort"
#define LAST_OPEN_TERMINAL_TCP_SERVER_PORT "LastOpenTerminalSereverTCPPort"
#define LAST_THRESHOLD_EDITOR_STATE "LastThresholdEditorState"
#define LAST_THRESHOLD_EDITOR_PATH "LastThresholdEditorPath"
#define LAST_EDIT_KEYPOINTS_STATE "LastEditKeyointsState"
#define LAST_EDIT_KEYPOINTS_PATH "LastEditKeypointsPath"
#define LAST_MERGE_KEYPOINTS_OPEN_PATH "LastMergeKeypointsOpenPath"
#define LAST_MERGE_KEYPOINTS_SAVE_PATH "LastMergeKeypointsSavePath"
#define LAST_APRILTAG_RANGE_MIN "LastAprilTagRangeMin"
#define LAST_APRILTAG_RANGE_MAX "LastAprilTagRangeMax"
#define LAST_APRILTAG_INCLUDE "LastAprilTagInclude"
#define LAST_APRILTAG_PATH "LastAprilTagPath"
#define LAST_MODEL_NO_CAM_PATH "LastModelNoCamPath"
#define LAST_MODEL_WITH_CAM_PATH "LastModelWithCamPath"
#define LAST_DATASET_EDITOR_PATH "LastDatasetEditorPath"
#define LAST_DATASET_EDITOR_LOADED "LastDatasetEditorLoaded"
#define LAST_DATASET_EDITOR_EXPORT_PATH "LastDatasetEditorExportPath"
#define RESOURCES_MAJOR "ResourcesMajor"
#define RESOURCES_MINOR "ResourcesMinor"
#define RESOURCES_PATCH "ResourcesPatch"

#define SERIAL_PORT_SETTINGS_GROUP "OpenMVSerialPort"
#define OPEN_TERMINAL_SETTINGS_GROUP "OpenMVOpenTerminal"
#define OPEN_TERMINAL_DISPLAY_NAME "DisplayName"
#define OPEN_TERMINAL_OPTION_INDEX "OptionIndex"
#define OPEN_TERMINAL_COMMAND_STR "CommandStr"
#define OPEN_TERMINAL_COMMAND_VAL "CommandVal"

#define RECONNECTS_MAX 10
#define OLD_API_MAJOR 1
#define OLD_API_MINOR 7
#define OLD_API_PATCH 0
#define OLD_API_BOARD "OPENMV2"

#define LEARN_MTU_ADDED_MAJOR 9
#define LEARN_MTU_ADDED_MINOR 9
#define LEARN_MTU_ADDED_PATCH 9

#define OPENMV_DISK_ADDED_MAJOR 3
#define OPENMV_DISK_ADDED_MINOR 2
#define OPENMV_DISK_ADDED_PATCH 0
#define OPENMV_DISK_ADDED_NAME "/.openmv_disk"

#define OPENMV_DBG_PROTOCOL_CHNAGE_MAJOR 3
#define OPENMV_DBG_PROTOCOL_CHNAGE_MINOR 5
#define OPENMV_DBG_PROTOCOL_CHNAGE_PATCH 3

#define OPENMV_RGB565_BYTE_REVERSAL_FIXED_MAJOR 3
#define OPENMV_RGB565_BYTE_REVERSAL_FIXED_MINOR 8
#define OPENMV_RGB565_BYTE_REVERSAL_FIXED_PATCH 0

#define OPENMV_NEW_PIXFORMAT_MAJOR 4
#define OPENMV_NEW_PIXFORMAT_MINOR 1
#define OPENMV_NEW_PIXFORMAT_PATCH 3

#define OPENMV_ADD_MAIN_TERMINAL_INPUT_MAJOR 4
#define OPENMV_ADD_MAIN_TERMINAL_INPUT_MINOR 3
#define OPENMV_ADD_MAIN_TERMINAL_INPUT_PATCH 2

#define OPENMV_ADD_TIME_INPUT_MAJOR 4
#define OPENMV_ADD_TIME_INPUT_MINOR 3
#define OPENMV_ADD_TIME_INPUT_PATCH 2

#define FRAME_SIZE_DUMP_SPACING     5 // in ms
#define GET_SCRIPT_RUNNING_SPACING  100 // in ms
#define GET_TX_BUFFER_SPACING       5 // in ms

#define FPS_AVERAGE_BUFFER_DEPTH    100 // in samples
#define WIFI_PORT_RETIRE            20 // in seconds
#define ERROR_FILTER_MAX_SIZE       1000 // in chars
#define FPS_TIMER_EXPIRATION_TIME   2000 // in milliseconds
#define RESET_TO_DFU_SEARCH_TIME    2000 // in milliseconds

#define FILE_FLUSH_BYTES 1024 // Extra disk activity to flush changes...
#define FLASH_SECTOR_ERASE 4096 // Flash sector size in bytes.
#define FOLDER_SCAN_TIME 10000 // in ms

namespace OpenMV {
namespace Internal {

class OpenMVPluginCompletionAssistProvider : public TextEditor::CompletionAssistProvider
{

public:

    OpenMVPluginCompletionAssistProvider(const QStringList &variables,
                                         const QStringList &classes, const QMap<QString, QStringList> &classArgs,
                                         const QStringList &functions, const QMap<QString, QStringList> &functionArgs,
                                         const QStringList &methods, const QMap<QString, QStringList> &methodArgs,
                                         QObject *parent) : CompletionAssistProvider(parent)
    {
        m_keywords = TextEditor::Keywords(variables,
                                          classes, classArgs,
                                          functions, functionArgs,
                                          methods, methodArgs);
    }

    TextEditor::IAssistProcessor *createProcessor(const TextEditor::AssistInterface *assistInterface) const
    {
        Q_UNUSED(assistInterface)

        return new TextEditor::KeywordsCompletionAssistProcessor(m_keywords);
    }

    int activationCharSequenceLength() const
    {
        return 1;
    }

    bool isActivationCharSequence(const QString &sequence) const
    {
        return (sequence.at(0) == QLatin1Char('.')) || (sequence.at(0) == QLatin1Char('(')) || (sequence.at(0) == QLatin1Char(','));
    }

private:

    TextEditor::Keywords m_keywords;
};

typedef struct importData
{
    QString moduleName, modulePath;
    QByteArray moduleHash;
}
importData_t;

typedef QList<importData_t> importDataList_t;

QByteArray loadFilter(const QByteArray &data);
importDataList_t loadFolder(const QString &rootPath, const QString &path, bool flat);

class LoadFolderThread: public QObject
{
    Q_OBJECT

    public: explicit LoadFolderThread(const QString &path, bool flat) { m_path = path; m_flat = flat; }
    public slots: void loadFolderSlot() { emit folderLoaded(loadFolder(m_path, m_path, m_flat)); }
    signals: void folderLoaded(const importDataList_t &output);
    private: QString m_path; bool m_flat;
};

class wifiPort_t
{

public:

    QString addressAndPort;
    QString name;
    QTime time;

    bool operator ==(const wifiPort_t &port) const
    {
        return (addressAndPort == port.addressAndPort) && (name == port.name);
    }
};

QPair<QStringList, QStringList> filterPorts(const QString &serialNumberFilter,
                                            bool forceBootloader,
                                            const QList<wifiPort_t> &availableWifiPorts);

class ScanSerialPortsThread: public QObject
{
    Q_OBJECT

    public: explicit ScanSerialPortsThread(const QString &serialNumberFilter) { m_serialNumberFilter = serialNumberFilter; }
    public slots: void scanSerialPortsSlot() { emit serialPorts(filterPorts(m_serialNumberFilter, true, QList<wifiPort_t>())); }
    signals: void serialPorts(const QPair<QStringList, QStringList> &output);
    private: QString m_serialNumberFilter;
};

class OpenMVPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "OpenMV.json")

public:

    explicit OpenMVPlugin();
    bool initialize(const QStringList &arguments, QString *errorMessage);
    void extensionsInitialized();
    bool delayedInitialize();
    ExtensionSystem::IPlugin::ShutdownFlag aboutToShutdown();
    QObject *remoteCommand(const QStringList &options, const QString &workingDirectory, const QStringList &arguments);
    QList<QObject *> createTestObjects() const { return QList<QObject *>(); }

public slots: // private

    void registerOpenMVCam(const QString board, const QString id);
    bool registerOpenMVCamDialog(const QString board, const QString id);
    void packageUpdate();
    void bootloaderClicked();
    void installTheLatestDevelopmentRelease();
    void connectClicked(bool forceBootloader = false,
                        QString forceFirmwarePath = QString(),
                        bool forceFlashFSErase = false,
                        bool justEraseFlashFs = false,
                        bool installTheLatestDevelopmentFirmware = false,
                        bool waitForCamera = false,
                        QString previousMapping = QString());
    void disconnectClicked(bool reset = false);
    void startClicked();
    void stopClicked();
    void processEvents();
    void errorFilter(const QByteArray &data);
    void configureSettings();
    void saveScript();
    void saveImage(const QPixmap &data);
    void saveTemplate(const QRect &rect);
    void saveDescriptor(const QRect &rect);
    QMultiMap<QString, QAction *> aboutToShowExamplesRecursive(const QString &path, QMenu *parent, bool notExamples = false);
    void updateCam(bool forceYes = false);
    void setPortPath(bool silent = false);
    void openTerminalAboutToShow();
    QList<int> openThresholdEditor(const QVariant parameters = QVariant());
    void openKeypointsEditor();
    void openAprilTagGenerator(apriltag_family_t *family);

signals:

    void workingDone(); // private
    void disconnectDone(); // private

private:

    bool getTheLatestDevelopmentFirmware(const QString &arch, QString *path);

    bool m_autoConnect;
    QString m_autoUpdate;
    bool m_autoErase;
    bool m_autoRun;
    bool m_disableStop;

    OpenMVPluginSerialPort *m_ioport;
    OpenMVPluginIO *m_iodevice;

    QElapsedTimer m_frameSizeDumpTimer;
    QElapsedTimer m_getScriptRunningTimer;
    QElapsedTimer m_getTxBufferTimer;

    QElapsedTimer m_timer;
    QQueue<qint64> m_queue;

    bool m_working;
    bool m_connected;
    bool m_running;
    QMetaObject::Connection m_connect_disconnect;
    int m_major;
    int m_minor;
    int m_patch;
    QString m_boardTypeFolder;
    QString m_fullBoardType;
    QString m_boardType;
    QString m_boardId;
    int m_boardVID;
    int m_boardPID;
    QString m_sensorType;
    int m_reconnects;
    QString m_portName;
    QString m_portPath;
    QString m_formKey;

    QString m_serialNumberFilter;
    QRegularExpression m_errorFilterRegex;
    QString m_errorFilterString;

    QAction *m_bootloaderAction;
    QAction *m_eraseAction;
    QAction *m_autoReconnectAction;
    QAction *m_stopOnConnectDiconnectionAction;
    QAction *m_enableSyncingImportsAction;
    QAction *m_enableFilteringExamplesAction;

    Core::Command *m_openDriveFolderCommand; QAction *m_openDriveFolderAction;
    Core::Command *m_configureSettingsCommand; QAction *m_configureSettingsAction;
    Core::Command *m_saveCommand; QAction *m_saveAction;
    Core::Command *m_resetCommand; QAction *m_resetAction;
    Core::Command *m_developmentReleaseCommand; QAction *m_developmentReleaseAction;
    Core::ActionContainer *m_openTerminalMenu;
    Core::Command *m_connectCommand; QAction *m_connectAction;
    Core::Command *m_disconnectCommand; QAction *m_disconnectAction;
    Core::Command *m_startCommand; QAction *m_startAction;
    Core::Command *m_stopCommand; QAction *m_stopAction;

    QToolButton *m_jpgCompress;
    QToolButton *m_disableFrameBuffer;
    OpenMVDatasetEditor *m_datasetEditor;
    OpenMVPluginFB *m_frameBuffer;
    OpenMVPluginHistogram *m_histogram;

    Utils::ElidingLabel *m_boardLabel;
    Utils::ElidingLabel *m_sensorLabel;
    Utils::ElidingToolButton *m_versionButton;
    Utils::ElidingLabel *m_portLabel;
    Utils::ElidingToolButton *m_pathButton;
    Utils::ElidingLabel  *m_fpsLabel;

    ///////////////////////////////////////////////////////////////////////////

    typedef struct documentation
    {
        QString moduleName;
        QString className;
        QString name;
        QString text;
    }
    documentation_t;

    QList<documentation_t> m_modules;
    QList<documentation_t> m_classes;
    QList<documentation_t> m_datas;
    QList<documentation_t> m_functions;
    QList<documentation_t> m_methods;
    QSet<QString> m_arguments;
    QList<wifiPort_t> m_availableWifiPorts;

    typedef struct openTerminalMenuData
    {
        QString displayName;
        int optionIndex;
        QString commandStr;
        int commandVal;
    }
    openTerminalMenuData_t;

    QList<openTerminalMenuData_t> m_openTerminalMenuData;

    bool openTerminalMenuDataContains(const QString &displayName)
    {
        foreach(const openTerminalMenuData_t &data, m_openTerminalMenuData)
        {
            if(data.displayName == displayName)
            {
                return true;
            }
        }

        return false;
    }

    ///////////////////////////////////////////////////////////////////////////

    importDataList_t m_exampleModules;
    importDataList_t m_documentsModules;

    QRegularExpression m_emRegEx;
    QRegularExpression m_spanRegEx;
    QRegularExpression m_linkRegEx;
    QRegularExpression m_classRegEx;
    QRegularExpression m_cdfmRegExInside;
    QRegularExpression m_argumentRegEx;
    QRegularExpression m_tupleRegEx;
    QRegularExpression m_listRegEx;
    QRegularExpression m_dictionaryRegEx;

    void processDocumentationMatch(const QRegularExpressionMatch &match,
                                   QStringList &providerVariables,
                                   QStringList &providerClasses, QMap<QString, QStringList> &providerClassArgs,
                                   QStringList &providerFunctions, QMap<QString, QStringList> &providerFunctionArgs,
                                   QStringList &providerMethods, QMap<QString, QStringList> &providerMethodArgs);
    void parseImports(const QString &fileText, const QString &moduleFolder, const QStringList &builtInModules, importDataList_t &targetModules, QStringList &errorModules);
    bool importHelper(const QByteArray &text);

    ///////////////////////////////////////////////////////////////////////////

    typedef struct exampleFilter
    {
        QRegularExpression path;
        QRegularExpression boardType;
        QRegularExpression sensorType;
        QString flatten;
    }
    exampleFilter_t;

    QList<exampleFilter_t> m_exampleFilters;

    bool matchFlatten(const QString &filePath, const QSet<QString> &flattenSet);
    bool matchExample(const QString &filePath, QString *flattenRegex);
};

} // namespace Internal
} // namespace OpenMV

#endif // OPENMVPLUGIN_H
