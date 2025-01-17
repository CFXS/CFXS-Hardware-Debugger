// ---------------------------------------------------------------------
// CFXS L0 ARM Debugger <https://github.com/CFXS/CFXS-L0-ARM-Debugger>
// Copyright (C) 2022 | CFXS
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>
// ---------------------------------------------------------------------
// [CFXS] //
#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include <QCloseEvent>
#include <QTreeView>
#include <QAction>
#include <QLayout>
#include <QSettings>
#include <QDesktopServices>
#include <QFileDialog>
#include <QTimer>

#include <DockAreaWidget.h>
#include "CFXS_Center_Widget.hpp"

#include <Core/Project/ProjectManager.hpp>
#include <UI/Windows/AboutWindow/AboutWindow.hpp>
#include <UI/Panels/WorkspacePanel.hpp>
#include <UI/Panels/TextEditPanel.hpp>
#include <UI/Panels/AppLogPanel.hpp>
#include <UI/Panels/HexEditorPanel.hpp>
#include <UI/Panels/Debugger/SymbolListPanel.hpp>
#include <UI/Helpers/CreateQMenuTextSeparator.hpp>

#include <Core/ELF/ELF_Reader.hpp>

#include <Core/Probe/JLink/JLink.hpp>
#include <Core/Probe/STLink/STLink.hpp>
const char* g_TargetDeviceModel = nullptr;
QString g_ProbeID               = "0";

using ads::CDockManager;
using ads::CDockWidget;
using ads::DockWidgetArea;

/////////////////////////////////////////////////////////////
/////////////////// [ Window/Panel Stuff ] //////////////////
/////////////////////////////////////////////////////////////
// Panel/Window differences:
//     - Window: A window that is not a part of the docking system
//     - Panel: A dockable window that's state is saved/loaded when opening/closing projects
// Panels are the main dynamic component of the HWD UI
// Each main debugger function like MemoryView and LiveWatch will have it's own panel (including open source files - TextEditPanel)
//
// Panel names and descriptions are stored in it's objectName in this format: PanelName|PanelData
// This is stored to the /.cfxs_hwd/WindowState.L0 file and when loaded, the PanelData string is passed to the constructed panel if necessary
// Right now this is the only idea I have to not make loading panels in LoadState() too messy
// All panels with the same base panel name must have different PanelData values as the docking system uses a QMap<QString, ...> to store its dock widgets
// If multiple panels with the same name are constructed, then saving and loading panels will not work right because of map key overlaps
// Good panel objectName example:
//    Panel #1 - WorkspacePanel
//    Panel #2 - MemoryViewPanel|0
//    Panel #3 - MemoryViewPanel|1,"Custom Panel Title Label"
//    Panel #4 - TextEditPanel|./CFXS_RTOS_Test/src/main.cpp
//    Panel #5 - TextEditPanel|./CFXS_RTOS_Test/src/_TM4C129X_Startup_RTOS.cpp

QString s_ELFPath;
L0::Probe::I_Probe* g_ActiveProbe = nullptr;
bool g_CortexA;
template<typename T>
void StartConnection() {
    if (g_ProbeID != "0" && g_TargetDeviceModel) {
        if (!g_ActiveProbe) {
            g_ActiveProbe = new T;
        }

        static_cast<T*>(g_ActiveProbe)->L0_SelectDevice(g_ProbeID);
        L0::Target::SupportedDevices::LoadSupportedDevices();
        auto& testDevice = L0::Target::SupportedDevices::GetSupportedDevices().at(g_TargetDeviceModel);
        g_ActiveProbe->Probe_Disconnect();

        g_ActiveProbe->Probe_Connect();

        // Cortex-M does not die with SWD + frequent start/stop memory reads
        if (strcmp(g_TargetDeviceModel, "ATSAMA5D36") == 0) {
            g_ActiveProbe->Target_SelectDebugInterface(L0::Probe::I_Probe::DebugInterface::JTAG); // Cortex-A does die
            g_CortexA = true;
        } else {
            g_ActiveProbe->Target_SelectDebugInterface(L0::Probe::I_Probe::DebugInterface::SWD);
            g_CortexA = false;
        }

        g_ActiveProbe->Target_SelectDevice(testDevice);
        g_ActiveProbe->Target_Connect();
    }
}

namespace L0::UI {

    //////////////////////////////////////////////////////////////////////////////////////////////////
    static constexpr int STATE_SERIALIZER_VERSION      = 1; // Used to track state loading differences
    static const QString LAST_SESSION_PERSPECTIVE_NAME = QSL("__LastSession");
    static const QString CENTRAL_WIDGET_NAME           = QSL("__CentralWidget");
    static const QString KEY_VERSION                   = QSL("version");
    static const QString KEY_WINDOW_GEOMETRY           = QSL("windowGeometry");
    static const QString KEY_WINDOW_STATE              = QSL("windowState");
    static const QString KEY_DOCK_STATE                = QSL("dockState");
    static const QString KEY_OPEN_PANEL_DESCRIPTION    = QSL("openPanels");
    static const QString KEY_WINDOW_DATA               = QSL("windowStateData");
    //////////////////////////////////////////////////////////////////////////////////////////////////
    // Actions
    std::vector<MainWindow::ActionEntryDefinition>* MainWindow::s_ActionDefinitions_View;
    //////////////////////////////////////////////////////////////////////////////////////////////////

    MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(std::make_unique<Ui::MainWindow>()) {
        ui->setupUi(this);
        UpdateTitle();
        resize(1280, 720); // default size

        auto menuFont = ui->menubar->font();
        menuFont.setPointSize(menuFont.pointSize() + 1);
        ui->menubar->setFont(menuFont);

        for (auto action : ui->menubar->actions()) {
            auto actionFont = action->font();
            actionFont.setPointSize(actionFont.pointSize() + 1);
            action->setFont(actionFont);
        }

        auto centralFrame = new CFXS_Center_Widget;
        centralFrame->setStyleSheet("background: rgb(32, 32, 32);");
        centralFrame->setObjectName("MainWindowCentralFrame");

        CDockWidget* centralDockWidget = new CDockWidget(CENTRAL_WIDGET_NAME);
        centralDockWidget->setWidget(centralFrame);
        centralDockWidget->layout()->setContentsMargins({0, 0, 0, 0});

        centralDockWidget->setFeature(CDockWidget::NoTab, true);
        auto centralDockArea = GetDockManager()->setCentralWidget(centralDockWidget);
        centralDockArea->setAllowedAreas(DockWidgetArea::AllDockAreas);

        InitializeActions();

        // Update title if project dir changed
        connect(ProjectManager::GetNotifier(), &ProjectManager::Notifier::ProjectOpened, this, [=]() {
            UpdateTitle();
        });

        // Update recent projects if recent project list changed
        connect(ProjectManager::GetNotifier(), &ProjectManager::Notifier::RecentProjectsChanged, this, [=]() {
            UpdateRecentProjects();
        });
        UpdateRecentProjects();
    }

    MainWindow::~MainWindow() {
    }

    ads::CDockManager* MainWindow::GetDockManager() {
        return ui->dockManager;
    }

    void MainWindow::closeEvent(QCloseEvent* event) {
        LOG_UI_TRACE("Close MainWindow");
        if (g_ActiveProbe)
            g_ActiveProbe->Probe_Disconnect();
        SaveState();
        emit Closed();
        event->accept();
    }

    ///////////////////////////////////////////////////////////////////

    void MainWindow::UpdateTitle() {
        if (ProjectManager::IsProjectOpen()) {
            auto path           = ProjectManager::GetWorkspacePath();
            QString lastDirName = path.mid(path.lastIndexOf("/") + 1);
            setWindowTitle(QSL(CFXS_PROGRAM_NAME) + " - " + lastDirName);
        } else {
            setWindowTitle(QSL(CFXS_PROGRAM_NAME) + " - No project open");
        }
    }

    void MainWindow::UpdateRecentProjects() {
        ui->menuOpen_Recent->clear();

        auto& recentList = ProjectManager::GetRecentProjectPaths();

        if (recentList.isEmpty()) {
            ui->menuOpen_Recent->addAction("No Recent Projects");
        } else {
            for (auto& path : recentList) {
                ui->menuOpen_Recent->addAction(path, [=]() {
                    if (ProjectManager::IsProjectOpen()) {
                        if (ProjectManager::GetWorkspacePath() == path) { // don't open same project again
                            LOG_UI_TRACE("Trying to open same project");
                            return;
                        }

                        SaveState();
                    }
                    ProjectManager::OpenProject(path);
                });
            }
            ui->menuOpen_Recent->addSeparator();
            ui->menuOpen_Recent->addAction("Clear Recent Projects", [=]() {
                ProjectManager::ClearRecentProjects();
            });
        }
    }

    void MainWindow::SaveState() {
        QMap<QString, QVariant> state;
        state[KEY_VERSION]         = STATE_SERIALIZER_VERSION;
        state[KEY_WINDOW_GEOMETRY] = saveGeometry();
        state[KEY_WINDOW_STATE]    = saveState();
        state[KEY_DOCK_STATE]      = GetDockManager()->saveState();

        QStringList openPanelList;

        GetDockManager()->removePerspective(LAST_SESSION_PERSPECTIVE_NAME);
        GetDockManager()->addPerspective(LAST_SESSION_PERSPECTIVE_NAME);
        QSettings cfg;
        GetDockManager()->savePerspectives(cfg);

        auto dockWidgets = GetDockManager()->dockWidgetsMap();
        for (auto w : dockWidgets) {
            // Don't store info about the central widget
            if (w->objectName() != CENTRAL_WIDGET_NAME) {
                if (w->property("MULTI_INSTANCE").isValid()) {
                    if (w->isClosed()) {
                        continue;
                    }
                }

                openPanelList.append(w->objectName());
                auto panel = dynamic_cast<I_Panel*>(w);
                if (panel) {
                    panel->SavePanelState(&cfg);
                }
            }
        }

        state[KEY_OPEN_PANEL_DESCRIPTION] = openPanelList;

        cfg.setValue(KEY_WINDOW_DATA, state);

        cfg.setValue("_temp_TargetDeviceModel", QString(g_TargetDeviceModel));
        cfg.setValue("_temp_ProbeID", g_ProbeID);
        cfg.setValue("_ELF", s_ELFPath);

        emit StateDataReady(&cfg);
    }

    void MainWindow::HideAllPanels() {
        for (auto panel : GetDockManager()->dockWidgetsMap()) {
            if (panel->objectName() != CENTRAL_WIDGET_NAME) {
                if (panel->objectName().startsWith(TextEditPanel::GetPanelBaseName()) ||
                    panel->objectName().startsWith(HexEditorPanel::GetPanelBaseName())) {
                    GetDockManager()->removeDockWidget(panel);
                    panel->deleteLater();
                } else {
                    panel->toggleView(false);
                }
            }
        }
    }

    void MainWindow::LoadState(QSettings& stateData) {
        if (stateData.status() != QSettings::Status::NoError) {
            LOG_UI_ERROR("MainWindow failed to load state");
        } else {
            QMap<QString, QVariant> state = stateData.value(KEY_WINDOW_DATA).toMap();

            if (!state.isEmpty()) {
                if (!state.contains(KEY_VERSION)) {
                    LOG_UI_ERROR("MainWindow failed to load state - no version field");
                    return;
                }

                switch (state[KEY_VERSION].toInt()) {
                    case 1: { // Window state format version 1
                        LOG_UI_TRACE("MainWindow load state [version = {0}]", state[KEY_VERSION].toInt());
                        if (!state.contains(KEY_WINDOW_GEOMETRY) || !state.contains(KEY_WINDOW_STATE) || !state.contains(KEY_DOCK_STATE) ||
                            !state.contains(KEY_OPEN_PANEL_DESCRIPTION)) {
                            LOG_UI_ERROR("MainWindow failed to load state v{0} - invalid data", state[KEY_VERSION].toInt());
                            return;
                        }

                        // Open all stored panels
                        for (auto& panelDescription : state[KEY_OPEN_PANEL_DESCRIPTION].toStringList()) {
                            bool known = true;

                            // panelDescription format:
                            // PanelName|PanelData...
                            QString panelName;
                            QString panelData;
                            auto separatorOffset = panelDescription.indexOf(QSL("|"));

                            if (separatorOffset > 0) {
                                panelName = panelDescription.mid(0, separatorOffset);  // start to '|'
                                panelData = panelDescription.mid(separatorOffset + 1); // '|' to end
                            } else {
                                panelName = panelDescription;
                            }

                            if (panelName == WorkspacePanel::GetPanelBaseName()) {
                                if (auto panel = OpenPanel_Workspace()) {
                                    if (panel)
                                        panel->LoadPanelState(&stateData);
                                }
                            } else if (panelName == AppLogPanel::GetPanelBaseName()) {
                                if (auto panel = OpenPanel_AppLog()) {
                                    if (panel)
                                        panel->LoadPanelState(&stateData);
                                }
                            } else if (panelName == SymbolListPanel::GetPanelBaseName()) {
                                if (auto panel = OpenPanel_Symbols()) {
                                    if (panel)
                                        panel->LoadPanelState(&stateData);
                                }
                            } else if (panelName == HexEditorPanel::GetPanelBaseName()) {
                                // panelData for TextEditPanel is relative file path
                                if (auto panel = OpenHexEditor(panelData)) {
                                    if (panel)
                                        panel->LoadPanelState(&stateData);
                                }
                            } else if (panelName == TextEditPanel::GetPanelBaseName()) {
                                // panelData for TextEditPanel is relative file path
                                if (auto panel = OpenFilePanel(panelData)) {
                                    if (panel)
                                        panel->LoadPanelState(&stateData);
                                }
                            } else {
                                known = false;
                            }

                            if (!known) {
                                LOG_UI_ERROR(" - Unknown panel: {}", panelName);
                            }
                        }

                        // Load data
                        restoreGeometry(state[KEY_WINDOW_GEOMETRY].toByteArray());
                        restoreState(state[KEY_WINDOW_STATE].toByteArray());
                        //GetDockManager()->restoreState(state[KEY_DOCK_STATE].toByteArray());

                        GetDockManager()->loadPerspectives(stateData);
                        GetDockManager()->openPerspective(LAST_SESSION_PERSPECTIVE_NAME);

                        break;
                    }
                    default: LOG_UI_ERROR("MainWindow failed to load state - unsupported version"); return;
                }

                static char tmpx[64];
                if (stateData.value("_temp_TargetDeviceModel").isValid()) {
                    snprintf(tmpx, 64, "%s", stateData.value("_temp_TargetDeviceModel").toString().toStdString().c_str());
                    if (strlen(tmpx))
                        g_TargetDeviceModel = tmpx;
                }
                g_ProbeID = stateData.value("_temp_ProbeID").toString();
                if (g_ProbeID.length() < 16) {
                    StartConnection<L0::Probe::JLink>();
                } else {
                    StartConnection<L0::Probe::STLink>();
                }
                if (stateData.value("_ELF").isValid()) {
                    s_ELFPath = stateData.value("_ELF").toString();
                    auto obj  = new ELF::ELF_Reader(s_ELFPath);
                    obj->LoadFile();
                    obj->LoadSTABS();
                }
            } else {
                LOG_UI_ERROR("MainWindow failed to load state - corrupted data");
                return;
            }
        }
    }

    ///////////////////////////////////////////////////////////////

    void MainWindow::InitializeActions() {
        // Actions
        s_ActionDefinitions_View = new std::vector<MainWindow::ActionEntryDefinition>{
            // Open workspace panel
            {false,
             "Workspace",
             [](MainWindow* dis) {
                 dis->OpenPanel_Workspace();
             },
             FileIconProvider{}.icon(FileIconProvider::Icon::FOLDER_OPEN)},

            // Separator
            {true},

            {false,
             "Symbols",
             [](MainWindow* dis) {
                 dis->OpenPanel_Symbols();
             },
             FileIconProvider{}.icon(FileIconProvider::Icon::HEX)},

            {false,
             "Range Memory View",
             [](MainWindow* dis) {
                 int fmwCount = 0;
                 for (auto panel : dis->GetDockManager()->dockWidgetsMap()) {
                     if (panel->objectName().startsWith(HexEditorPanel::GetPanelBaseName() + "|$FastMemoryView")) {
                         fmwCount++;
                     }
                 }

                 dis->OpenHexEditor("$FastMemoryView" + QString::number(fmwCount));
             },
             FileIconProvider{}.icon(FileIconProvider::Icon::BIN)},

            // Separator
            {true},

            // Open app log
            {false,
             "Application Log",
             [](MainWindow* dis) {
                 dis->OpenPanel_AppLog();
             },
             FileIconProvider{}.icon(FileIconProvider::Icon::LOG)},
        };

        InitializeActions_File();
        InitializeActions_View();
        InitializeActions_Help();
        InitializeActions_Debug();
    }

    void MainWindow::InitializeActions_File() {
        // Open Project...
        connect(ui->actionOpen_Project, &QAction::triggered, this, [=]() {
            QString dir = QFileDialog::getExistingDirectory(
                this, QSL("Open Project"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

            if (dir.isEmpty()) {
                LOG_UI_TRACE("Not opening project - no directory selected");
            } else {
                LOG_UI_TRACE("Selected project directory - {}", dir);

                if (ProjectManager::IsProjectOpen()) {
                    SaveState();
                }

                ProjectManager::OpenProject(dir);
            }
        });

        // Exit
        connect(ui->actionExit, &QAction::triggered, this, [=]() {
            this->close();
        });
    }

    void MainWindow::InitializeActions_View() {
        for (auto& vae : *s_ActionDefinitions_View) {
            if (vae.isSeparator) {
                ui->menuView->addSeparator();
            } else {
                auto action = new QAction(vae.icon, vae.name, this);
                ui->menuView->addAction(action);
                connect(action, &QAction::triggered, this, [=]() {
                    if (vae.callback)
                        vae.callback(this);
                });
            }
        }
    }

    void MainWindow::InitializeActions_Help() {
        // CFXS HWD Github
        connect(ui->actionCFXS_L0_Github_Page, &QAction::triggered, this, [=]() {
            QDesktopServices::openUrl(QSL("https://github.com/CFXS/CFXS-Hardware-Debugger"));
        });

        // CFXS Discord
        connect(ui->actionCFXS_Discord, &QAction::triggered, this, [=]() {
            QDesktopServices::openUrl(QSL("https://discord.gg/ZdhfzQAC4S"));
        });

        // About
        connect(ui->actionAbout, &QAction::triggered, this, [=]() {
            auto aboutWindow = new AboutWindow;
            aboutWindow->show();
        });
    }

    void MainWindow::InitializeActions_Debug() {
        auto m                       = ui->menubar->addMenu("Debug");
        static const char* DEVICES[] = {"TM4C1294NC", "ATSAMA5D36", "STM32H7A3ZI", "STM32H753ZI"};

        m->addAction(Utils::CreateQMenuTextSeparator(
            "Target Device", QSL("padding-left: 4px; color: rgb(220, 220, 220); font-weight: 500; background: transparent;")));

        static QAction* s_DeviceActions[sizeof(DEVICES) / sizeof(DEVICES[0])];

        int idx = 0;
        for (auto d : DEVICES) {
            auto a                 = m->addAction(d);
            s_DeviceActions[idx++] = a;
            QTimer::singleShot(1000, [=]() {
                if (g_TargetDeviceModel && strcmp(g_TargetDeviceModel, d) == 0) {
                    a->setIcon(FileIconProvider{}.icon(FileIconProvider::Icon::L0));
                }
            });
            connect(a, &QAction::triggered, this, [=]() {
                g_TargetDeviceModel = d;
                if (g_ProbeID.length() < 16) {
                    StartConnection<L0::Probe::JLink>();
                } else {
                    StartConnection<L0::Probe::STLink>();
                }
                for (int i = 0; i < sizeof(s_DeviceActions) / sizeof(s_DeviceActions[0]); i++) {
                    if (i == idx - 1) {
                        s_DeviceActions[i]->setIcon(FileIconProvider{}.icon(FileIconProvider::Icon::L0));
                    } else {
                        s_DeviceActions[i]->setIcon(QIcon{});
                    }
                }
            });
        }

        static std::vector<QAction*> s_ProbeActions;

        //////////////////////////////////////////////////////////////////////////
        // J-Link
        {
            m->addSeparator();
            m->addAction(Utils::CreateQMenuTextSeparator(
                "J-Link Probes", QSL("padding-left: 4px; color: rgb(220, 220, 220); font-weight: 500; background: transparent;")));

            auto probes = Probe::JLink::GetConnectedProbes();
            idx         = 0;
            for (auto& p : probes) {
                auto a      = m->addAction(QString(p.modelName) + " (" + QString::number(p.serialNumber) + ")");
                auto serial = p.serialNumber;
                s_ProbeActions.push_back(a);
                idx++;
                QTimer::singleShot(1000, [=]() {
                    if (g_ProbeID.toUInt() == serial) {
                        a->setIcon(FileIconProvider{}.icon(FileIconProvider::Icon::SEGGER));
                    }
                });
                connect(a, &QAction::triggered, this, [=]() {
                    g_ProbeID = QString::number(serial);
                    StartConnection<L0::Probe::JLink>();
                    int i = 0;
                    for (auto pa : s_ProbeActions) {
                        if (i == idx - 1) {
                            pa->setIcon(FileIconProvider{}.icon(FileIconProvider::Icon::SEGGER));
                        } else {
                            pa->setIcon(QIcon{});
                        }
                        i++;
                    }
                });
            }
        }

        //////////////////////////////////////////////////////////////////////////
        // ST-Link
        {
            m->addSeparator();
            m->addAction(Utils::CreateQMenuTextSeparator(
                "ST-Link Probes", QSL("padding-left: 4px; color: rgb(220, 220, 220); font-weight: 500; background: transparent;")));

            auto probes = Probe::STLink::GetConnectedProbes();
            idx         = 0;
            for (auto& p : probes) {
                auto a      = m->addAction(p.modelName + " (" + p.serialNumber + ")");
                auto serial = p.serialNumber;
                s_ProbeActions.push_back(a);
                idx++;
                QTimer::singleShot(1000, [=]() {
                    if (g_ProbeID == serial) {
                        a->setIcon(FileIconProvider{}.icon(FileIconProvider::Icon::STM));
                    }
                });
                connect(a, &QAction::triggered, this, [=]() {
                    g_ProbeID = serial;
                    StartConnection<L0::Probe::STLink>();
                    int i = 0;
                    for (auto pa : s_ProbeActions) {
                        if (i == idx - 1) {
                            pa->setIcon(FileIconProvider{}.icon(FileIconProvider::Icon::STM));
                        } else {
                            pa->setIcon(QIcon{});
                        }
                        i++;
                    }
                });
            }
        }

        m->addSeparator();
        auto rst = m->addAction(FileIconProvider{}.icon(FileIconProvider::Icon::GEAR), "Reset Target");
        connect(rst, &QAction::triggered, this, [=]() {
            if (g_ActiveProbe) {
                g_ActiveProbe->Target_Reset(false);
            }
        });

        auto rcon = m->addAction(FileIconProvider{}.icon(FileIconProvider::Icon::GEAR), "Reconnect Target");
        connect(rcon, &QAction::triggered, this, [=]() {
            if (g_ProbeID.length() < 16) {
                StartConnection<L0::Probe::JLink>();
            } else {
                StartConnection<L0::Probe::STLink>();
            }
        });
    }

    ////////////////////////////////////////////////////////////////////
    // Panel opening

    WorkspacePanel* MainWindow::OpenPanel_Workspace() {
        if (!m_Panel_Workspace) {
            m_Panel_Workspace = new WorkspacePanel;
            GetDockManager()->addDockWidget(ads::DockWidgetArea::LeftDockWidgetArea, m_Panel_Workspace, GetDockManager()->dockArea(0));
            connect(static_cast<WorkspacePanel*>(m_Panel_Workspace), &WorkspacePanel::RequestOpenFile, this, &MainWindow::OpenFileHandler);
        } else {
            m_Panel_Workspace->toggleView();
            m_Panel_Workspace->raise();
        }

        return m_Panel_Workspace;
    }

    AppLogPanel* MainWindow::OpenPanel_AppLog() {
        if (!m_Panel_AppLog) {
            m_Panel_AppLog = new AppLogPanel;
            GetDockManager()->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea, m_Panel_AppLog, GetDockManager()->dockArea(0));
        } else {
            m_Panel_AppLog->toggleView();
            m_Panel_AppLog->raise();
        }

        return m_Panel_AppLog;
    }

    SymbolListPanel* MainWindow::OpenPanel_Symbols() {
        if (!m_Panel_SymbolList) {
            m_Panel_SymbolList = new SymbolListPanel;
            GetDockManager()->addDockWidgetFloating(m_Panel_SymbolList);
        } else {
            m_Panel_SymbolList->toggleView();
            m_Panel_SymbolList->raise();
        }

        return m_Panel_SymbolList;
    }

    HexEditorPanel* MainWindow::OpenPanel_HexEditor(const QString& filePath) {
        auto hexEditPanel = new HexEditorPanel;
        bool fileLoaded   = hexEditPanel->LoadFile(filePath);
        if (!fileLoaded) {
            delete hexEditPanel;
            return nullptr;
        }
        GetDockManager()->addDockWidgetFloating(hexEditPanel);
        hexEditPanel->setFeature(CDockWidget::DockWidgetAlwaysCloseAndDelete, true); // delete on close

        return hexEditPanel;
    }

    TextEditPanel* MainWindow::OpenFilePanel(const QString& path) {
        QString newPath = QDir(ProjectManager::GetWorkspacePath()).relativeFilePath(path);
        auto fullPath   = ProjectManager::GetFullFilePath(newPath);
        QFileInfo fileInfo(fullPath);

        if (!fileInfo.exists()) {
            LOG_UI_WARN("OpenFilePanel file does not exist - {}", path);
            return nullptr;
        }

        TextEditPanel* existingEditor = nullptr;
        for (auto panel : GetDockManager()->dockWidgetsMap()) {
            if (panel->objectName().startsWith(TextEditPanel::GetPanelBaseName()) &&
                panel->property("absoluteFilePath").toString() == fullPath) {
                existingEditor = static_cast<TextEditPanel*>(panel);
                break;
            }
        }

        // if exists, set as current tab
        if (existingEditor) {
            existingEditor->setAsCurrentTab();
            return existingEditor;
        } else {
            return OpenPanel_TextEdit(fullPath);
        }
    }

    HexEditorPanel* MainWindow::OpenHexEditor(const QString& path) {
        QString newPath;
        QFileInfo fileInfo;
        QString fullPath;

        if (path.at(0) == QChar('$')) {
            fullPath = path;
        } else {
            newPath  = QDir(ProjectManager::GetWorkspacePath()).relativeFilePath(path);
            fullPath = ProjectManager::GetFullFilePath(newPath);
            fileInfo = QFileInfo(fullPath);
            if (!fileInfo.exists()) {
                LOG_UI_WARN("OpenHexEditor file does not exist - {}", path);
                return nullptr;
            }
        }

        HexEditorPanel* existingEditor = nullptr;
        for (auto panel : GetDockManager()->dockWidgetsMap()) {
            if (panel->objectName().startsWith(HexEditorPanel::GetPanelBaseName()) && !panel->property("MULTI_INSTANCE").isValid() &&
                panel->property("absoluteFilePath").toString() == fullPath) {
                existingEditor = static_cast<HexEditorPanel*>(panel);
                break;
            }
        }

        // if exists, set as current tab
        if (existingEditor) {
            existingEditor->setAsCurrentTab();
            return existingEditor;
        } else {
            return OpenPanel_HexEditor(fullPath);
        }
    }

    TextEditPanel* MainWindow::OpenPanel_TextEdit(const QString& path) {
        auto textEditPanel = new TextEditPanel;
        textEditPanel->SetFilePath(path);
        GetDockManager()->addDockWidgetTabToArea(textEditPanel, GetDockManager()->centralWidget()->dockAreaWidget());
        textEditPanel->setFeature(CDockWidget::DockWidgetAlwaysCloseAndDelete, true); // delete on close

        return textEditPanel;
    }

    static Probe::JLink* probe = nullptr;

    static void Temp_LoadToTarget(const QString& path) {
        if (!g_TargetDeviceModel) {
            LOG_CORE_CRITICAL("No target device selected\n");
            return;
        }
        if (g_ProbeID == "0") {
            LOG_CORE_CRITICAL("No probe selected\n");
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_CORE_ERROR("Failed to open file {}", path);
            return;
        }
        auto prog = file.readAll();
        file.close();

        if (!probe) {
            Target::SupportedDevices::LoadSupportedDevices();
            auto& testDevice = Target::SupportedDevices::GetSupportedDevices().at(g_TargetDeviceModel);
            probe            = new Probe::JLink;
            probe->L0_SelectDevice(g_ProbeID);
            probe->Probe_Disconnect();
            probe->Probe_Connect();
            probe->Target_SelectDebugInterface(Probe::I_Probe::DebugInterface::SWD);
            probe->Target_SelectDevice(testDevice);
            probe->Target_Connect();
        }

        probe->Target_Halt();
        probe->Target_WriteProgram((uint8_t*)prog.data(), prog.size());
        probe->Target_Reset();
        probe->Target_Run();
    }

    static void Temp_LoadToTarget0x20000000(const QString& path) {
        if (!g_TargetDeviceModel) {
            LOG_CORE_CRITICAL("No target device selected\n");
            return;
        }
        if (g_ProbeID == "0") {
            LOG_CORE_CRITICAL("No probe selected\n");
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_CORE_ERROR("Failed to open file {}", path);
            return;
        }
        auto prog = file.readAll();
        file.close();

        if (g_ProbeID.length() < 16) {
            StartConnection<L0::Probe::JLink>();
        } else {
            StartConnection<L0::Probe::STLink>();
        }

        probe->Target_Halt(1000);
        probe->Target_WriteMemory(prog.data(), prog.size(), 0x20000000);
        probe->Target_SetPC(0x20000000);
        probe->Target_Run();
    }

    static void Temp_EraseTarget() {
        if (!g_TargetDeviceModel) {
            LOG_CORE_CRITICAL("No target device set\n");
            return;
        }
        if (g_ProbeID == "0") {
            LOG_CORE_CRITICAL("No probe selected\n");
            return;
        }

        if (!probe) {
            Target::SupportedDevices::LoadSupportedDevices();
            auto& testDevice = Target::SupportedDevices::GetSupportedDevices().at(g_TargetDeviceModel);
            probe            = new Probe::JLink;
            probe->L0_SelectDevice(g_ProbeID);
            probe->Probe_Disconnect();
            probe->Probe_Connect();
            probe->Target_SelectDebugInterface(Probe::I_Probe::DebugInterface::SWD);
            probe->Target_SelectDevice(testDevice);
            probe->Target_Connect();
        }

        probe->Target_Halt();
        probe->Target_Erase();
    }

    static QByteArray s_MergePartA;
    static void Temp_SetA64(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_CORE_ERROR("Failed to open file {}", path);
            return;
        }
        s_MergePartA = file.readAll();
        file.close();
        while (s_MergePartA.size() < 1024 * 64) {
            s_MergePartA.push_back((char)0);
        }
    }

    static void Temp_SetA128(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_CORE_ERROR("Failed to open file {}", path);
            return;
        }
        s_MergePartA = file.readAll();
        file.close();
        while (s_MergePartA.size() < 1024 * 128) {
            s_MergePartA.push_back((char)0);
        }
    }

    static void Temp_MergeA(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_CORE_ERROR("Failed to open file {}", path);
            return;
        }
        auto prog = file.readAll();
        file.close();

        auto merged = s_MergePartA + prog;

        QFile tempBin(path + ".merged.bin");
        tempBin.open(QIODevice::ReadWrite | QIODevice::Truncate);
        tempBin.write((const char*)merged.data(), merged.size());
        tempBin.close();
    }

    // Check what type of file this is and open it in either the text editor or a special editor
    void MainWindow::OpenFileHandler(const QString& path, const QString& type) {
        if (type == QSL("elf") || type == QSL("out")) {
            s_ELFPath = path;
            auto obj  = new ELF::ELF_Reader(path);
            obj->LoadFile();
            obj->LoadSTABS();
        } else if (type == QSL("$HexEditor")) {
            OpenHexEditor(path);
        } else if (type == QSL("$LoadToTarget")) {
            Temp_LoadToTarget(path);
        } else if (type == QSL("$LoadToTarget0x20000000")) {
            Temp_LoadToTarget0x20000000(path);
        } else if (type == QSL("$EraseTarget")) {
            Temp_EraseTarget();
        } else if (type == QSL("$GluePartA64")) {
            Temp_SetA64(path);
        } else if (type == QSL("$GluePartA128")) {
            Temp_SetA128(path);
        } else if (type == QSL("$MergeA")) {
            Temp_MergeA(path);
        } else {
            OpenFilePanel(path);
        }

    } // namespace L0::UI

} // namespace L0::UI