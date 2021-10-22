// ---------------------------------------------------------------------
// CFXS Hardware Debugger <https://github.com/CFXS/CFXS-Hardware-Debugger>
// Copyright (C) 2021 | CFXS
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
#include "ProjectManager.hpp"
#include <QDir>

namespace HWD {

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    static bool s_ProjectOpen        = false; // true when project folder has been opened
    static bool s_ProjectInitialized = false; // true when /.cfxs_hwd directory contains a project file
    static QString s_WorkspacePath;
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    _ProjectManagerNotifier* ProjectManager::GetNotifier() {
        static _ProjectManagerNotifier n;
        return &n;
    }

    //////////////////////////////////////////////////////////////////////////////

    void ProjectManager::HWD_Load() {
        HWDLOG_PROJECT_INFO("ProjectManager load");
    }

    void ProjectManager::HWD_Unload() {
        HWDLOG_PROJECT_INFO("ProjectManager unload");
    }

    void ProjectManager::OpenProject(const QString& path) {
        QString workspacePath = path;
        if (workspacePath.contains("/.cfxs_hwd")) {
            HWDLOG_PROJECT_INFO("Trying to open .cfxs_hwd - moving back one directory");
            workspacePath = workspacePath.mid(0, workspacePath.indexOf("/.cfxs_hwd"));
        }

        if (s_WorkspacePath != workspacePath) {
            if (QDir().exists(workspacePath)) {
                HWDLOG_PROJECT_INFO("Open project {}", workspacePath);
                s_WorkspacePath = workspacePath;
                s_ProjectOpen   = true;

                emit GetNotifier()->ProjectOpened();
            } else {
                HWDLOG_PROJECT_ERROR("Invalid path - {}", workspacePath);
            }
        } else {
            HWDLOG_PROJECT_INFO("Project already open - {}", workspacePath);
        }
    }

    bool ProjectManager::IsProjectOpen() {
        return s_ProjectOpen;
    }

    const QString& ProjectManager::GetWorkspacePath() {
        return s_WorkspacePath;
    }

    QString ProjectManager::GetProjectFilePath(Path path) {
        switch (path) {
            case Path::WINDOW_STATE: return GetWorkspacePath() + "/.cfxs_hwd/WindowState.hwd";
        }

        HWDLOG_PROJECT_CRITICAL("Unknown path {}", (int)path);
        return s_WorkspacePath + "_error_temp.txt";
    }

} // namespace HWD