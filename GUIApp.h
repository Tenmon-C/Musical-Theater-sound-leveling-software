#pragma once

#include "AudioProcessor.h"
#include <filesystem>
#include <string>

struct GLFWwindow;

class GUIApp {
public:
    GUIApp() = default;
    ~GUIApp();

    GUIApp(const GUIApp&) = delete;
    GUIApp& operator=(const GUIApp&) = delete;

    bool initialize();
    void run();
    void renderFrame();

    void setTargetFilePath(const std::filesystem::path& path) { m_targetFilePath = path; }
    void setRefFilePath(const std::filesystem::path& path) { m_refFilePath = path; }

private:
    void renderUI();
    void processAudioMatch();

    GLFWwindow* m_window{nullptr};
    std::filesystem::path m_refFilePath;
    std::filesystem::path m_targetFilePath;
    std::filesystem::path m_outputFilePath;
    std::string m_statusMessage{"Ready. Select audio files to begin."};

    ProcessingResult m_lastResult{};
};