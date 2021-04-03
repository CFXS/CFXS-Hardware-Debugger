#pragma once

#include <Probe/IProbe.hpp>
#include "Driver/JLink_Driver.hpp"

namespace HWD::Probe {

    class JLink_Probe final : public IProbe {
        friend struct ProbeCallbackEntry;

        using MessageCallback       = Driver::JLink_Types::LogCallback;
        using FlashProgressCallback = Driver::JLink_Types::FlashProgressCallback;

    public:
        // !!! UPDATE InitializeProbeCallbackArray IF CHANGED !!!
        static constexpr auto MAX_DISCOVERABLE_PROBE_COUNT = 8; // limit max discoverable probe count

    public:
        virtual ~JLink_Probe();

        /////////////////////////////////////////
        // Probe overrides
        bool Probe_IsReady() const override;
        bool Probe_Connect() override;
        bool Probe_Disconnect() override;
        bool Probe_IsConnected() const override;

        const std::string& JLink_Probe::GetModelName() const override {
            return m_ModelName;
        }

        const std::string& JLink_Probe::GetSerialNumberString() const override {
            return m_SerialNumberString;
        }
        /////////////////////////////////////////
        // Probe target overrides
        virtual bool Target_SelectDevice(const TargetDeviceDescription& device) override;
        virtual bool Target_SelectDebugInterface(DebugInterface interface) override;
        virtual bool Target_IsConnected() const override;
        virtual bool Target_Connect() override;
        virtual bool Target_Erase() override;
        virtual bool Target_WriteProgram(const uint8_t* data, uint32_t size) override;
        virtual bool Target_Reset(bool haltAfterReset = true) override;
        virtual bool Target_StartTerminal(void* params = nullptr) override;
        /////////////////////////////////////////

    public:
        static std::vector<JLink_Probe*> s_GetConnectedProbes();

    private:
        static void s_InitializeProbeCallbackArray();

    private:
        /// \param probeInfo probe data
        /// \param probeIndex index to use for mapping callbacks (probe index in s_Probes)
        JLink_Probe(const Driver::JLink_Types::ProbeInfo& probeInfo, int probeIndex);

        std::shared_ptr<Driver::JLink_Driver> JLink_Probe::GetDriver() const {
            return m_Driver;
        }

        uint32_t JLink_Probe::GetRawSerialNumber() const {
            return m_RawSerialNumber;
        }

        // probe callbacks
        void Probe_LogCallback(const char* message);
        void Probe_WarningCallback(const char* message);
        void Probe_ErrorCallback(const char* message);
        void Probe_FlashProgressCallback(const char* action, const char* prog, int percentage);

        // specific config
        void Probe_DisableFlashProgressPopup();

    private:
        // An array is used instead of a vector to be able to set error/warning/log handlers for each instance safely (because of threads)
        static std::array<JLink_Probe*, MAX_DISCOVERABLE_PROBE_COUNT> s_Probes; // list of all JLink probes

    private:
        std::shared_ptr<Driver::JLink_Driver> m_Driver;
        int m_ProbeIndex;

    private: // generic properties
        uint32_t m_RawSerialNumber       = 0;
        std::string m_ModelName          = "Unknown";
        std::string m_SerialNumberString = "Unknown";
    };

} // namespace HWD::Probe