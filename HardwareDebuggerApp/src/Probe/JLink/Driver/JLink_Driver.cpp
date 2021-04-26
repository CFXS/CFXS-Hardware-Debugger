#include "JLink_Driver.hpp"

namespace HWD::Probe::Driver {

    JLink_Driver::JLink_Driver() {
        m_Library = std::make_unique<DynamicLibrary>("C:/CFXS/JLink_x64.dll");

        if (m_Library->IsLoaded()) {
            LoadFunctionPointers();
        } else {
            HWDLOG_PROBE_ERROR("JLink_Driver[{0}] Driver initialization failed", fmt::ptr(this));
        }
    }

} // namespace HWD::Probe::Driver