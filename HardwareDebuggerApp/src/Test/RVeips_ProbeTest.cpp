// [source]
#include "RVeips_ProbeTest.hpp"

#include <Probe/JLink/JLink.hpp>
#include <Target/Cortex/ROM_Table.hpp>
#include <Target/Cortex/DWT_Registers.hpp>
#include <Target/Cortex/ITM_Registers.hpp>

#include "TestFirmware.h"

namespace HWD::Test {

    using namespace Probe;
    RVeips_ProbeTest* m_RV = nullptr;

    RVeips_ProbeTest::RVeips_ProbeTest() {
        SupportedDevices::LoadSupportedDevices();
        auto& testDevice = SupportedDevices::GetSupportedDevices().at("TM4C1294NC");

        auto connectedProbes = JLink::s_GetConnectedProbes();
        for (IProbe* probe : connectedProbes) {
            probe->Probe_Connect();

            probe->Target_SelectDebugInterface(IProbe::DebugInterface::SWD);
            probe->Target_SelectDevice(testDevice);
            probe->Target_Connect();

            if (probe->Target_IsConnected()) {
                m_Probe = probe;
                m_RV    = this;

                probe->Target_Erase();

                probe->Target_WriteProgram(firmware, sizeof(firmware));
                probe->Target_Reset(true);

                //////////////////////////////////////////////////////////////////////////////////

                int bytesRead;
                uint32_t rom_table_address;
                rom_table_address = probe->Target_Get_ROM_Table_Address();

                HWDLOG_PROBE_TRACE("Target [Debug]");
                if (rom_table_address) {
                    HWDLOG_PROBE_TRACE("\tROM Table: @ {0:#X}", rom_table_address);
                } else {
                    HWDLOG_PROBE_CRITICAL("\tROM Table: @ {0:#X}", rom_table_address);
                }

                if (rom_table_address) {
                    // ROM TABLE /////////////////////////////////////////////////////////////
                    // [ARM Debug Interface v5 Architecture Specification]
                    // - ROM Tables > ROM Table Overview > "A ROM Table always occupies 4kB of memory" (4000 or 4096?)
                    uint8_t rom_table_data[4096];
                    Cortex::M4::ROM_Table_Offsets rtOffsets;
                    memset(&rtOffsets, 0, sizeof(rtOffsets));

                    bytesRead =
                        probe->Target_ReadMemoryTo(rom_table_address, rom_table_data, sizeof(rom_table_data), IProbe::AccessWidth::_4);
                    HWDLOG_PROBE_INFO("rom table read {0} bytes", bytesRead);

                    if (bytesRead == 4096) {
                        Cortex::M4::ROM_Table_Entry* rtEntry = (Cortex::M4::ROM_Table_Entry*)rom_table_data;
                        int entryIndex                       = 0;
                        do {
                            if (rtEntry->Is8bit()) {
                                HWDLOG_PROBE_CRITICAL("8bit ROM Table entries not supported yet");
                                break;
                            }

                            uint32_t componentAddress = rtEntry->GetComponentAddress(rom_table_address);
                            uint32_t componentBaseAddress;

                            HWDLOG_PROBE_INFO(" - {0} {1}",
                                              Cortex::M4::ROM_Table_EntryName[entryIndex],
                                              rtEntry->IsPresent() ? "Present" : "Not present");

                            if (rtEntry->IsPresent()) {
                                Cortex::M4::PeripheralID4 pid4;
                                pid4._val = probe->Target_ReadMemory_8(componentAddress + Cortex::CoreSight::OFFSET_PERIPHERAL_ID4);
                                componentBaseAddress         = componentAddress - (4096 * (pid4.GetBlockCount() - 1));
                                rtOffsets._table[entryIndex] = componentBaseAddress;

                                HWDLOG_PROBE_INFO("\tComponent Base Address: {0:#X}", componentBaseAddress);

                                uint32_t cs_class =
                                    (probe->Target_ReadMemory_32(componentBaseAddress + Cortex::CoreSight::OFFSET_COMPONENT_ID1) >> 4) &
                                    0x0F;
                                static constexpr auto getClassString = [](uint32_t c) {
                                    switch (c) {
                                        case Cortex::CoreSight::CLASS_CORESIGHT: return "CoreSight";
                                        case Cortex::CoreSight::CLASS_GENERIC: return "Generic";
                                        case Cortex::CoreSight::CLASS_PRIMECELL: return "PrimeCell";
                                        case Cortex::CoreSight::CLASS_ROMTABLE: return "ROM Table";
                                        default: return "Unknown";
                                    }
                                };
                                HWDLOG_PROBE_INFO("\tComponent Class: {0}", getClassString(cs_class));
                            }

                            rtEntry++;
                            entryIndex++;
                        } while (!rtEntry->IsTableEndMarker());

                        if (rtOffsets.DWT) {
                            if (probe->Target_WriteMemory_32(rtOffsets.DWT + Cortex::CoreSight::OFFSET_LOCK_ACCESS,
                                                             Cortex::CoreSight::VAL_UNLOCK_KEY)) {
                                HWDLOG_PROBE_TRACE("Unlocked DWT");
                            } else {
                                HWDLOG_PROBE_ERROR("Failed to unlock DWT");
                            }
                        }
                        if (rtOffsets.ITM) {
                            if (probe->Target_WriteMemory_32(rtOffsets.ITM + Cortex::CoreSight::OFFSET_LOCK_ACCESS,
                                                             Cortex::CoreSight::VAL_UNLOCK_KEY)) {
                                HWDLOG_PROBE_TRACE("Unlocked ITM");
                            } else {
                                HWDLOG_PROBE_ERROR("Failed to unlock ITM");
                            }
                        }
                        if (rtOffsets.TPIU) {
                            if (probe->Target_WriteMemory_32(rtOffsets.TPIU + Cortex::CoreSight::OFFSET_LOCK_ACCESS,
                                                             Cortex::CoreSight::VAL_UNLOCK_KEY)) {
                                HWDLOG_PROBE_TRACE("Unlocked TPIU");
                            } else {
                                HWDLOG_PROBE_ERROR("Failed to unlock TPIU");
                            }
                        }

                        if (rtOffsets.DWT) {
                            // configure pc sampling
                            auto srate = Cortex::M4::DWT::REG_CTRL::SampleRateDivider::_15360;
                            Cortex::M4::DWT::REG_CTRL dwtReg(probe, rtOffsets.DWT);

                            HWDLOG_PROBE_WARN("DWT: {0:#X}", dwtReg.GetRaw());
                            HWDLOG_PROBE_TRACE("Enable PC Sampling");
                            dwtReg.Enable_PC_Sampling(srate);
                            HWDLOG_PROBE_TRACE("Sampling frequency: {0}kHz", decltype(dwtReg)::SampleRateDividerToSampleRate(120e6, srate));
                            HWDLOG_PROBE_WARN("DWT: {0:#X}", dwtReg.GetRaw());
                        }
                    } else {
                        HWDLOG_PROBE_CRITICAL("Failed to read ROM Table");
                    }
                }

                //////////////////////////////////////////////////////////////////////////////////

                probe->Target_Run();
                probe->Target_StartTerminal();
            }

            auto thread = new std::thread([=]() {
                while (1 < 2) {
                    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    probe->Process();
                }
            });
            thread->detach();
        }
    }

    RVeips_ProbeTest::~RVeips_ProbeTest() {
        auto connectedProbes = JLink::s_GetConnectedProbes();
        for (IProbe* probe : connectedProbes) {
            delete probe;
        }
    }

    //

    uint64_t RVeips_ProbeTest::ReadMilliseconds() {
        /*uint64_t var = 0;

        if (m_Probe) {
            bool ret;
            var = m_Probe->Target_ReadMemory_64(536874024, &ret);
            if (!ret)
                var = 0;
        }

        return var;*/
        return 0;
    }

    uint32_t RVeips_ProbeTest::Read32(uint32_t addr, bool halt) {
        /*uint32_t var = 0;

        if (m_Probe) {
            if (halt)
                m_Probe->Target_Halt();
            bool ret = m_Probe->Target_ReadMemory_32(addr, &var);
            if (halt)
                m_Probe->Target_Run();
            if (!ret)
                var = 0;
        }

        return var;*/
        return 0;
    }

    uint32_t RVeips_ProbeTest::Write32(uint32_t addr, uint32_t value) {
        //m_Probe->Target_WriteMemory_32(addr, value);
        return 0;
    }

    float RVeips_ProbeTest::FlashProgress() {
        return m_Probe->Target_GetFlashProgress();
    }

    const char* RVeips_ProbeTest::GetTerminalText() {
        if (m_Probe)
            return m_Probe->Target_GetTerminalBuffer();
        else
            return "Waiting for target...";
    }

} // namespace HWD::Test