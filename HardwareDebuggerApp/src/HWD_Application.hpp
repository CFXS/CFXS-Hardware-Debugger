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
#pragma once

#include <Test/RVeips_ProbeTest.hpp>
#include <QSettings>

#include "Core/Application.hpp"

namespace HWD {

    class HWD_Application : public Application {
    public:
        HWD_Application(int argc, char** argv);
        virtual ~HWD_Application() = default;

        virtual void OnCreate() override;
        virtual void OnDestroy() override;

    private:
        void LoadGlobalSettings();

        void Load_Probe();
        void Unload_Probe();

    private:
        static void SaveWindowState(QSettings* stateData);

    private:
        std::unique_ptr<Test::RVeips_ProbeTest> m_RihardsTest;
    };

} // namespace HWD