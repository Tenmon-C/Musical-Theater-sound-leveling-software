#include "GUIApp.h"
#include "AudioProcessor.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

EM_JS(void, js_open_file_dialog, (const char* virtual_path, const char* is_target_flag), {
    var input = document.createElement('input');
    input.type = 'file';
    input.accept = 'audio/*';
    input.onchange = function(e) {
        var file = e.target.files[0];
        if (!file) return;
        var reader = new FileReader();
        reader.onload = function(evt) {
            var data = new Uint8Array(evt.target.result);
            var vpath = UTF8ToString(virtual_path);
            FS.writeFile(vpath, data);
            
            var isTarget = UTF8ToString(is_target_flag);
            if (isTarget === "1") {
                Module.ccall('OnTargetFileLoaded', 'void', ['string'], [vpath]);
            } else {
                Module.ccall('OnRefFileLoaded', 'void', ['string'], [vpath]);
            }
        };
        reader.readAsArrayBuffer(file);
    };
    input.click();
});

EM_JS(void, js_download_file, (const char* virtual_path, const char* download_name), {
    try {
        var data = FS.readFile(UTF8ToString(virtual_path));
        var blob = new Blob([data], { type: 'application/octet-stream' });
        var link = document.createElement('a');
        link.href = URL.createObjectURL(blob);
        link.download = UTF8ToString(download_name);
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    } catch (err) {
        console.error("Download failed:", err);
    }
});

static GUIApp* g_AppInstance = nullptr;

extern "C" {
    EMSCRIPTEN_KEEPALIVE void OnTargetFileLoaded(const char* path) {
        if (g_AppInstance) g_AppInstance->setTargetFilePath(path);
    }
    EMSCRIPTEN_KEEPALIVE void OnRefFileLoaded(const char* path) {
        if (g_AppInstance) g_AppInstance->setRefFilePath(path);
    }
}

static void EmscriptenMainLoopCallback(void* arg) {
    GUIApp* app = static_cast<GUIApp*>(arg);
    app->renderFrame();
}
#endif

GUIApp::~GUIApp() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

bool GUIApp::initialize() {
    if (!glfwInit()) return false;

    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    m_window = glfwCreateWindow(850, 540, "Audio Loudness Matcher", nullptr, nullptr);
    if (!m_window) return false;

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    m_outputFilePath = "matched_output.wav";
#ifdef __EMSCRIPTEN__
    g_AppInstance = this;
#endif

    return true;
}

void GUIApp::run() {
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(EmscriptenMainLoopCallback, this, 0, 1);
#else
    while (!glfwWindowShouldClose(m_window)) {
        renderFrame();
    }
#endif
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
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);
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
        js_open_file_dialog("/target_audio", "1");
#endif
    }

    ImGui::Spacing();

    ImGui::TextWrapped("Reference File: %s", m_refFilePath.empty() ? "[None selected]" : m_refFilePath.string().c_str());
    if (ImGui::Button("Browse Reference Audio...", ImVec2(-1, 35))) {
#ifdef __EMSCRIPTEN__
        js_open_file_dialog("/ref_audio", "0");
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
void GUIApp::processAudioMatch() {
    if (m_targetFilePath.empty() || m_refFilePath.empty()) {
        m_statusMessage = "Error: Please select both a Target and Reference file.";
        return;
    }

    AudioProcessor processor;
    std::filesystem::path virtualOut = "/matched_output.wav";
    m_lastResult = processor.processAndMatch(m_targetFilePath, m_refFilePath, virtualOut);

    if (m_lastResult.success) {
        m_statusMessage = "Success! Downloading file...";
#ifdef __EMSCRIPTEN__
        js_download_file(virtualOut.string().c_str(), "matched_output.wav");
#endif
    } else {
        m_statusMessage = "Error: " + m_lastResult.errorMessage;
    }
}