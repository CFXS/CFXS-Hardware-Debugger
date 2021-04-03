// [source]
#include "HWD_Application.hpp"

#include "main.hpp"

#include <GL/glew.h>
#include <imgui.h>

namespace HWD {

    HWD_Application::HWD_Application(int argc, char** argv) : Application(argc, argv, "CFXS Hardware Debugger " CFXS_HWD_VERSION_STRING) {
    }

    HWD_Application::~HWD_Application() {
    }

    void HWD_Application::OnCreate() {
        m_RihardsTest = std::make_unique<Test::RVeips_ProbeTest>();
    }

    void HWD_Application::OnDestroy() {
    }

    void HWD_Application::OnUpdate() {
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    float val[4] = {0, 0, 0, 0};
    void HWD_Application::OnImGuiRender() {
        ImGui::Begin("Status Bar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Dedoid Text");
        ImGui::DragFloat4("Test Float4", val);
        ImGui::ProgressBar(0.25f);
        ImGui::End();
    }

    void HWD_Application::OnEvent() {
    }

} // namespace HWD