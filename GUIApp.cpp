#include "GUIApp.h"
#include "AudioProcessor.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void onFileUploaded(const char* fileName, bool isTarget) {
        if (isTarget) {
            GUIApp::getInstance().setTargetFilePath(fileName, "/target_audio");
        } else {
            GUIApp::getInstance().setRefFilePath(fileName, "/ref_audio");
        }
    }
}
#endif

static GUIApp* g_appInstance = nullptr;

GUIApp::GUIApp() {
    g_appInstance = this;
}

GUIApp::~GUIApp() {
    cleanup();
    if (g_appInstance == this) {
        g_appInstance = nullptr;
    }
}

GUIApp& GUIApp::getInstance() {
    return *g_appInstance;
}

bool GUIApp::initialize(int width, int height, const char* title) {
    if (!glfwInit()) return false;

#ifdef __EMSCRIPTEN__
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) return false;

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 100");

    return true;
}

void GUIApp::renderFrame() {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    renderUI();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.05f, 0.05f, 0.05f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);
}

#ifdef __EMSCRIPTEN__
static void emscriptenMainLoop() {
    GUIApp::getInstance().renderFrame();
}
#endif

void GUIApp::run() {
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(emscriptenMainLoop, 0, 1);
#else
    while (!glfwWindowShouldClose(m_window)) {
        renderFrame();
    }
#endif
}

void GUIApp::setTargetFilePath(const std::string& displayName, const std::string& virtualPath) {
    m_targetFilePath = displayName;
    m_targetVirtualPath = virtualPath;
    m_statusMessage = "Target audio loaded: " + displayName;
}

void GUIApp::setRefFilePath(const std::string& displayName, const std::string& virtualPath) {
    m_refFilePath = displayName;
    m_refVirtualPath = virtualPath;
    m_statusMessage = "Reference audio loaded: " + displayName;
}

void GUIApp::processAudioMatch() {
    if (m_targetVirtualPath.empty() || m_refVirtualPath.empty()) {
        m_statusMessage = "Error: Please select both target and reference audio files.";
        return;
    }

    m_statusMessage = "Processing audio gain matching...";

    AudioProcessor processor;
    // Uses your actual AudioProcessor function signature: processAndMatch
    ProcessingResult result = processor.processAndMatch(
        m_targetVirtualPath, 
        m_refVirtualPath, 
        "/output_matched.wav"
    );

    if (result.success) {
        m_lastResult.success = true;
        m_lastResult.targetDb = result.targetDb;
        m_lastResult.refDb = result.refDb;
        m_lastResult.gainDb = result.gainDb;
        m_statusMessage = "Processing complete! Downloading matched file...";

#ifdef __EMSCRIPTEN__
        EM_ASM({
            var data = Module.FS.readFile('/output_matched.wav');
            var blob = new Blob([data.buffer], { type: 'audio/wav' });
            var link = document.createElement('a');
            link.href = URL.createObjectURL(blob);
            link.download = 'matched_audio.wav';
            link.click();
        });
#endif
    } else {
        m_statusMessage = "Error: " + result.errorMessage;
    }
}

void GUIApp::renderUI() {
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(display_w), static_cast<float>(display_h)), ImGuiCond_Always);

    ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

    ImGui::Text("Audio Loudness Matcher");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Target File: %s", m_targetFilePath.empty() ? "[None selected]" : m_targetFilePath.string().c_str());
    if (ImGui::Button("Browse Target Audio...", ImVec2(-1, 35))) {
#ifdef __EMSCRIPTEN__
        EM_ASM({ openAudioFilePicker('/target_audio', 1); });
#endif
    }

    ImGui::Spacing();

    ImGui::TextWrapped("Reference File: %s", m_refFilePath.empty() ? "[None selected]" : m_refFilePath.string().c_str());
    if (ImGui::Button("Browse Reference Audio...", ImVec2(-1, 35))) {
#ifdef __EMSCRIPTEN__
        EM_ASM({ openAudioFilePicker('/ref_audio', 0); });
#endif
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Match Loudness & Download", ImVec2(-1, 45))) {
        processAudioMatch();
    }

    ImGui::Spacing();
    ImGui::TextWrapped("Status: %s", m_statusMessage.c_str());

    if (m_lastResult.success) {
        ImGui::Spacing();
        ImGui::BulletText("Target RMS: %.2f dBFS", m_lastResult.targetDb);
        ImGui::BulletText("Reference RMS: %.2f dBFS", m_lastResult.refDb);
        ImGui::BulletText("Gain Applied: %+.2f dB", m_lastResult.gainDb);
    }

    ImGui::End();
}

void GUIApp::cleanup() {
    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
    }
}