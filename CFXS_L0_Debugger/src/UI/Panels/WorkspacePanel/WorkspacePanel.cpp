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
#include "WorkspacePanel.hpp"

#include <Core/PlatformUtils.hpp>
#include <Core/Project/ProjectManager.hpp>
#include <QAction>
#include <QDir>
#include <QMenu>
#include <QProcess>
#include <QScrollBar>
#include <QTimer>

#include "ui_WorkspacePanel.h"

namespace L0::UI {

    ////////////////////////////////////////////////////////////
    const QStringList s_KnownExtensionList = {
        QSL("c"),   QSL("cc"),           QSL("cpp"),        QSL("cxx"), QSL("c++"), QSL("h"),         QSL("hh"),         QSL("hpp"),
        QSL("hxx"), QSL("h++"),          QSL("asm"),        QSL("s"),   QSL("inc"), QSL("txt"),       QSL("json"),       QSL("xml"),
        QSL("yml"), QSL("yaml"),         QSL("l0"),         QSL("ld"),  QSL("py"),  QSL("icf"),       QSL("cmake"),      QSL("map"),
        QSL("lhg"), QSL("clang-format"), QSL("clang-tidy"), QSL("lua"), QSL("md"),  QSL("gitignore"), QSL("gitmodules"), QSL("elf"),
        QSL("out")};
    ////////////////////////////////////////////////////////////

    WorkspacePanel::WorkspacePanel() : ads::CDockWidget(GetPanelBaseName()), ui(std::make_unique<Ui::WorkspacePanel>()) {
        LOG_UI_TRACE("Create {}", GetPanelBaseName());
        ui->setupUi(this);

        m_FB_Model = new FileBrowserModel(ui->tw_FileBrowser, this);
        m_FB_Model->setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);

        ui->tw_FileBrowser->setSelectionMode(QAbstractItemView::NoSelection); // No highlight
        ui->tw_FileBrowser->setFocusPolicy(Qt::NoFocus);                      // No focus box

        ui->tw_FileBrowser->setModel(m_FB_Model);

        for (int i = 1; i < ui->tw_FileBrowser->model()->columnCount(); i++) {
            ui->tw_FileBrowser->hideColumn(i); // hide all columns except name
        }

        ui->tw_FileBrowser->setHeaderHidden(true);
        ui->tw_FileBrowser->setUniformRowHeights(true);

        ui->tw_FileBrowser->setIndentation(16);

        connect(ui->tw_FileBrowser, &QTreeView::doubleClicked, this, [=](const QModelIndex& index) {
            auto info = m_FB_Model->fileInfo(index);
            if (info.isFile()) {
                // If extension matches external app list then open with external app
                if (s_KnownExtensionList.contains(info.suffix().toLower())) {
                    LOG_CORE_TRACE("Open file internally \"{}\"", info.absoluteFilePath());
                    emit RequestOpenFile(info.absoluteFilePath(), info.suffix().toLower());
                } else {
                    LOG_CORE_TRACE("Open file externally \"{}\"", info.absoluteFilePath());
                    QDesktopServices::openUrl(info.absoluteFilePath());
                }
            }
        });

        ui->tw_FileBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui->tw_FileBrowser, &QTreeView::customContextMenuRequested, this, [=](const QPoint& point) {
            QModelIndex index = ui->tw_FileBrowser->indexAt(point);
            if (index.isValid()) {
                auto info = m_FB_Model->fileInfo(index);
                if (!info.isFile() && !info.isDir())
                    return;

                OpenItemContextMenu(point, index, info);
            }
        });

        setWidget(ui->root);

        SetRootPath(ProjectManager::GetWorkspacePath());

        // Set root path if project dir changed
        connect(ProjectManager::GetNotifier(), &ProjectManager::Notifier::ProjectOpened, this, [=]() {
            SetRootPath(ProjectManager::GetWorkspacePath());
        });
    }

    void WorkspacePanel::OpenItemContextMenu(const QPoint& point, const QModelIndex& index, const QFileInfo& info) {
        auto menu = new QMenu(this);

        auto showInExplorerAction = new QAction(QPixmap(QSL(":/Icon/folder-open")), "Show in File Explorer", this);
        menu->addAction(showInExplorerAction);
        connect(showInExplorerAction, &QAction::triggered, this, [=]() {
            if (info.isFile()) {
                LOG_UI_TRACE("Show File in File Explorer \"{}\"", info.absoluteFilePath());
                PlatformUtils::ShowInFileExplorer(info.absoluteFilePath());
            } else {
                LOG_UI_TRACE("Show Folder in File Explorer \"{}\"", info.absolutePath());
                PlatformUtils::ShowInFileExplorer(info.absolutePath());
            }
        });

        if (info.isFile()) {
            menu->addSeparator();

            // Open as text file
            auto openAsTextAction = new QAction(QPixmap(QSL(":/Icon/doc")), "Open as Text", this);
            menu->addAction(openAsTextAction);
            connect(openAsTextAction, &QAction::triggered, this, [=]() {
                LOG_CORE_TRACE("Open as Text \"{}\"", info.absoluteFilePath());
                emit RequestOpenFile(info.absoluteFilePath(), "txt");
            });

            // Open With...
            auto openWithAction = new QAction(QPixmap(QSL(":/Icon/file")), "Open With...", this);
            menu->addAction(openWithAction);
            connect(openWithAction, &QAction::triggered, this, [=]() {
#if defined(Q_OS_WIN)
                LOG_CORE_TRACE("Open With... \"{}\"", info.absoluteFilePath());
                QProcess proc;
                proc.startDetached("rundll32.exe", {QSL("Shell32.dll,OpenAs_RunDLL"), info.absoluteFilePath().replace("/", "\\")});
#else
#error Open With... not implemented for this platform
#endif
            });

            menu->addSeparator();

            { // Show file size
                auto tempSizeLevel = menu->addMenu("File Size");
                char tmp[3][32];
                snprintf(tmp[0], sizeof(tmp), "%llu bytes", info.size());
                snprintf(tmp[1], sizeof(tmp), "%.2f kB", info.size() / 1024.0f);
                snprintf(tmp[2], sizeof(tmp), "%.3f MB", info.size() / (1024.0f * 1024.0f));
                tempSizeLevel->addAction(new QAction(tmp[0], this));
                tempSizeLevel->addAction(new QAction(tmp[1], this));
                tempSizeLevel->addAction(new QAction(tmp[2], this));
            }
        } else if (info.isDir()) {
        }

        menu->popup(ui->tw_FileBrowser->viewport()->mapToGlobal(point));
    }

    void WorkspacePanel::SetRootPath(const QString& path) {
        if (!path.isEmpty() && QDir().exists(path)) {
            m_FB_Model->setRootPath(path);
            ui->tw_FileBrowser->setRootIndex(m_FB_Model->index(path));
            ui->tw_FileBrowser->setVisible(true);
        } else {
            ui->tw_FileBrowser->setVisible(false);
        }
    }

    void WorkspacePanel::SavePanelState(QSettings* cfg) {
        QString cfgKey = objectName();
        cfgKey.replace('/', '\\'); // QSettings does not like '/' in keys
        LOG_UI_TRACE("WorkspacePanel save state - {}", cfgKey);

        cfg->beginGroup(cfgKey);
        cfg->setValue("version", 1);
        cfg->setValue("scroll_y", ui->tw_FileBrowser->verticalScrollBar()->value());

        QStringList expandedEntries;
        auto rootPathLen = m_FB_Model->rootPath().size();
        for (auto& index : m_FB_Model->GetPersistentIndexList()) {
            if (ui->tw_FileBrowser->isExpanded(index)) {
                auto path = index.data(QFileSystemModel::Roles::FilePathRole).toString().mid(rootPathLen);
                expandedEntries.append(path);
            }
        }

        cfg->setValue("expandedEntries", expandedEntries);

        cfg->endGroup();
    }

    void WorkspacePanel::LoadPanelState(QSettings* cfg) {
        QString cfgKey = objectName();
        cfgKey.replace('/', '\\'); // QSettings does not like '/' in keys
        LOG_UI_TRACE("WorkspacePanel load state - {}", objectName());

        if (!cfg->childGroups().contains(cfgKey)) {
            LOG_UI_WARN(" - No config entry for {}", cfgKey);
            return; // no cfg entry
        }

        cfg->beginGroup(cfgKey);
        auto version = cfg->value("version").toInt();
        if (version == 1) {
            auto entries     = cfg->value("expandedEntries").toStringList();
            QString rootPath = m_FB_Model->rootPath();

            ui->tw_FileBrowser->setUpdatesEnabled(false);
            ui->tw_FileBrowser->collapseAll();
            for (auto entry : entries) {
                ui->tw_FileBrowser->setExpanded(m_FB_Model->index(rootPath + entry), true);
            }
            ui->tw_FileBrowser->setUpdatesEnabled(true);

            auto scroll_y = cfg->value("scroll_y").toInt();
            if (scroll_y) {
                QTimer::singleShot(
                    1,
                    [=]() { // TODO(POTENTIAL_CRASH): find proper safe way to do this - instant set does not scroll, probably because model has not been updated
                        ui->tw_FileBrowser->verticalScrollBar()->setValue(scroll_y);
                    });
            }
        } else {
            LOG_UI_ERROR(" - Unsupported WorkspacePanel state data version {}", version);
        }
        cfg->endGroup();
    }

} // namespace L0::UI