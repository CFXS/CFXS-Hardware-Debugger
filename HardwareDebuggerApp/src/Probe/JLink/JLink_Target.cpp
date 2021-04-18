// [source]
#include "JLink.hpp"

namespace HWD::Probe {

    using namespace Driver::JLink_Types;

    void JLink::PrepareTarget() {
        HWDLOG_PROBE_TRACE("[JLink@{0}] Prepare target", fmt::ptr(this));
        ErrorCode ec;

        // Configure CoreSight
        ec = m_Driver->target_CoreSight_Configure("PerformTIFInit=1");
        if (ec != ErrorCode::OK) {
            HWDLOG_PROBE_ERROR("[JLink@{0}] Failed to configure CoreSight - {1}", fmt::ptr(this), ErrorCodeToString(ec));
        } else {
            HWDLOG_PROBE_TRACE("[JLink@{0}] CoreSight configured", fmt::ptr(this));
        }

        UpdateTargetInfo();
    }

    void JLink::UpdateTargetInfo() {
        HWDLOG_PROBE_TRACE("[JLink@{0}] UpdateTargetInfo:", fmt::ptr(this));

        uint32_t cpu_id                = m_Driver->target_Get_CPU_ID();
        DeviceCore core                = m_Driver->target_GetDeviceCore();
        int avail_sw_ram_breakpoints   = m_Driver->probe_GetAvailableBreakpointCount(Breakpoint::Type::SW_RAM);
        int avail_sw_flash_breakpoints = m_Driver->probe_GetAvailableBreakpointCount(Breakpoint::Type::SW_FLASH);
        int avail_hw_breakpoints       = m_Driver->probe_GetAvailableBreakpointCount(Breakpoint::Type::HW);

        HWDLOG_PROBE_TRACE("{0} Target info", GetModelName());
        HWDLOG_PROBE_TRACE("[CPU]");
        HWDLOG_PROBE_TRACE("\tCore:      {0}", core);
        HWDLOG_PROBE_TRACE("\tCPUID:     {0:#X}", cpu_id);
        HWDLOG_PROBE_TRACE("[Available Breakpoints]");
        HWDLOG_PROBE_TRACE("\tSW RAM:   {0}", avail_sw_ram_breakpoints);
        HWDLOG_PROBE_TRACE("\tSW FLASH: {0}", avail_sw_flash_breakpoints);
        HWDLOG_PROBE_TRACE("\tHW:       {0}", avail_hw_breakpoints);

        uint32_t maxSpeed;
        if (m_Driver->target_SWO_GetCompatibleSpeeds(120000000, 0, &maxSpeed, 1) < 0) {
            HWDLOG_PROBE_ERROR("Failed to get SWO speeds");
        } else {
            HWDLOG_PROBE_TRACE("Max SWO speed: {0}", maxSpeed);
        }

        if (m_Driver->target_SWO_Enable(120000000, maxSpeed, SWO_Interface::UART, 0x00000000) != ErrorCode::OK) {
            HWDLOG_PROBE_ERROR("Failed to enable SWO");
        } else {
            HWDLOG_PROBE_TRACE("SWO enabled");
        }
    }

} // namespace HWD::Probe