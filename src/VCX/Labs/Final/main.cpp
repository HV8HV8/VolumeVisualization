#include "App.h"
#include "Engine/app.h"
#include "Assets/bundled.h"

int main() {
    return VCX::Engine::RunApp<VCX::Labs::Final::App>(VCX::Engine::AppContextOptions {
        .Title = "HV8's Final Project",
        .WindowSize = { 1280, 800 },
        .FontSize = 16,
        .IconFileNames = VCX::Assets::DefaultIcons,
        .FontFileNames = VCX::Assets::DefaultFonts,
    });
}