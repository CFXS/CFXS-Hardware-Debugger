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
#pragma once
#include <QFileSystemModel>
#include <UI/Providers/FileIconProvider.hpp>
#include <QTreeView>

namespace L0::UI {

    class FileBrowserModel : public QFileSystemModel {
    public:
        FileBrowserModel(QTreeView* workingTreeView, QObject* parent = nullptr);

        QVariant data(const QModelIndex& index, int role) const;

        QList<QModelIndex> GetPersistendIndexList() const;

    private:
        FileIconProvider m_IconProvider;
        QTreeView* m_WorkingTreeView;
    };

} // namespace L0::UI