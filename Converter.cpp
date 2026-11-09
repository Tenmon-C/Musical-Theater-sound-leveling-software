#include "GUIApp.h"
#include <iostream>

int main(int argc, char* argv[]) {
    GUIApp app;

    if (!app.initialize()) {
        std::cerr << "Fatal Error: Failed to initialize GUI application window.\n";
        return -1;
    }
    app.run();
    return 0;
}