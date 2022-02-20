#pragma once

#include <QApplication>

#include "UI/Windows/MainWindow/MainWindow.hpp"

int main(int argc, char** argv);

namespace L0 {

    class Application {
    public:
        Application(int argc, char** argv, const std::string& name = "App");
        virtual ~Application();

        virtual void OnCreate()  = 0;
        virtual void OnDestroy() = 0;

        inline UI::MainWindow* GetMainWindow() {
            return m_MainWindow.get();
        }

    public:
        static inline Application& Get() {
            return *s_Instance;
        }

    private:
        void Run();

    private:
        std::unique_ptr<QApplication> m_QtApplication;
        std::unique_ptr<UI::MainWindow> m_MainWindow;

    private:
        static Application* s_Instance;
        friend int ::main(int argc, char** argv);
    };

} // namespace L0