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

#include <Core/Probe/I_Probe.hpp>
#include "Driver/JLink_Driver.hpp"

namespace L0::Probe {

    class JLink final : public I_Probe {
        friend struct ProbeCallbackEntry;

        using MessageCallback       = Driver::JLink_Types::LogCallback;
        using FlashProgressCallback = Driver::JLink_Types::FlashProgressCallback;

    public:
        /// HWD load all probe types on app init
        static void L0_Load();

        // HWD unload all probe types before app exit
        static void L0_Unload();

    public:
        virtual ~JLink();

        /// Select working device by serial number
        /// \param serialNumber serial number of probe. default 0 = first detected probe
        void L0_SelectDevice(const QString& serialNumber = "0");

        /////////////////////////////////////////

        void Process() override;

        /////////////////////////////////////////
        // Probe overrides

        bool Probe_IsReady() const override;
        bool Probe_Connect() override;
        bool Probe_Disconnect() override;
        bool Probe_IsConnected() const override;

        const QString& GetModelName() const override;
        const QString& GetSerialNumberString() const override;

        /////////////////////////////////////////
        // Probe target overrides

        bool Target_SelectDevice(const Target::DeviceDescription& device) override;
        bool Target_SelectDebugInterface(DebugInterface interface) override;
        bool Target_IsConnected() const override;
        bool Target_Connect() override;
        bool Target_Erase() override;
        bool Target_WriteProgram(const uint8_t* data, uint32_t size) override;
        bool Target_Reset(bool haltAfterReset = true) override;

        uint8_t Target_ReadMemory_8(uint32_t address, bool* success = nullptr) override;
        uint16_t Target_ReadMemory_16(uint32_t address, bool* success = nullptr) override;
        uint32_t Target_ReadMemory_32(uint32_t address, bool* success = nullptr) override;
        uint64_t Target_ReadMemory_64(uint32_t address, bool* success = nullptr) override;
        int Target_ReadMemoryTo(uint32_t address, void* to, uint32_t bytesToRead, AccessWidth accessWidth) override;
        bool Target_WriteMemory_8(uint32_t address, uint8_t val) override;
        bool Target_WriteMemory_16(uint32_t address, uint16_t val) override;
        bool Target_WriteMemory_32(uint32_t address, uint32_t val) override;
        bool Target_WriteMemory_64(uint32_t address, uint64_t val) override;
        bool Target_Halt(uint32_t waitHaltTimeout = 0) override;
        bool Target_Run() override;
        bool Target_IsRunning() override;
        float Target_GetFlashProgress() override;
        uint64_t Target_ReadPC(bool* success = nullptr) override;

        /////////////////////////////////////////

    public:
        JLink();
        static std::vector<Driver::JLink_Types::ProbeInfo> GetConnectedProbes();

    private:
        static Driver::JLink_Driver* GetDriver() {
            if (!s_Driver)
                LOG_PROBE_ERROR("Driver not initialized");

            return s_Driver;
        }

    private:
        // probe callbacks

        static void Probe_LogCallback(const char* message);
        static void Probe_WarningCallback(const char* message);
        static void Probe_ErrorCallback(const char* message);
        static void Probe_FlashProgressCallback(const char* action, const char* prog, int percentage);

        // specific config

        void Probe_DisableFlashProgressPopup();

        // Private probe stuff

        void UpdateProbeInfo();

        // Private target stuff

        void PrepareTarget();
        void UpdateTargetInfo();

    private:
        static Driver::JLink_Driver* s_Driver;

    private:
        /// True if physical device is assigned to drive
        bool m_DeviceAssigned = false;

        DebugInterface m_DebugInterface;

        Driver::JLink_Types::ProbeCapabilities m_ProbeCapabilities = {}; // initialize to 0

        QString m_ModelName          = "JLink";
        QString m_SerialNumberString = "?";
    };

} // namespace L0::Probe