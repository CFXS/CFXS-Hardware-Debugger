// [source]
#include "HWD_Application.hpp"

#include <set>

#include "main.hpp"

namespace HWD {
    namespace Probe {
        extern std::map<uint32_t, uint64_t> s_PC_Map;
        extern std::map<uint32_t, uint64_t> s_ExecMap;
    } // namespace Probe
    using namespace Probe;

    // TM4C
    std::map<uint32_t, const char*> s_DecodeEx = {
        {0, "MainLoop"},   //
        {15, "SysTick"},   //
        {16, "GPIO A"},    //
        {21, "UART 0"},    //
        {22, "UART 1"},    //
        {39, "Timer 2A"},  //
        {49, "UART 2"},    //
        {51, "Timer 3A"},  //
        {52, "Timer 3B"},  //
        {72, "UART 3"},    //
        {73, "UART 4"},    //
        {74, "UART 5"},    //
        {76, "UART 7"},    //
        {79, "Timer 4A"},  //
        {80, "Timer 4B"},  //
        {81, "Timer 5A"},  //
        {82, "Timer 5B"},  //
        {114, "Timer 6A"}, //
        {115, "Timer 6B"}, //
        {116, "Timer 7A"}, //
    };

    ///////////////////////////////////////////////////////////////////////////////// !Bodge alert! - SDL SetWindowTitle implementation "no prefix" tag
    HWD_Application::HWD_Application(int argc, char** argv) :
        Application(argc, argv, CFXS_HWD_WINDOW_TITLE_NO_PREFIX CFXS_HWD_PROGRAM_NAME) {
    }

    HWD_Application::~HWD_Application() {
    }

    namespace Test {
        extern RVeips_ProbeTest* m_RV;
    }

    void HWD_Application::OnCreate() {
        (new std::thread([=]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            m_RihardsTest = std::make_unique<Test::RVeips_ProbeTest>();
        }))->detach();
    }

    void HWD_Application::OnDestroy() {
        HWDLOG_CORE_INFO("Destroying App");
    }

    void HWD_Application::OnUpdate() {
    }

} // namespace HWD