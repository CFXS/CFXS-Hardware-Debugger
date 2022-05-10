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
#include "JLink.hpp"

namespace L0::Probe {

    using namespace Driver::JLink_Types;

    void JLink::UpdateProbeInfo() {
        LOG_PROBE_TRACE("[JLink@{0}] UpdateProbeInfo:", fmt::ptr(this));

        m_ProbeCapabilities = GetDriver()->probe_GetCapabilities();

        LOG_PROBE_TRACE("{0} Capabilities:", GetModelName());
        if (m_ProbeCapabilities & ProbeCapabilities::ADAPTIVE_CLOCKING)
            LOG_PROBE_TRACE(" - Adaptive clocking");
        if (m_ProbeCapabilities & ProbeCapabilities::RESET_STOP_TIMED)
            LOG_PROBE_TRACE(" - Halt after reset");
        if (m_ProbeCapabilities & ProbeCapabilities::SWO)
            LOG_PROBE_TRACE(" - SWO");
    }

} // namespace L0::Probe