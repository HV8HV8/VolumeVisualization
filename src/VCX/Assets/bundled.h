#pragma once

#include <array>
#include <string_view>

namespace VCX::Assets {
    inline constexpr auto DefaultIcons {
        std::to_array<std::string_view>({
            "assets/images/HV8-32x32.png",
            "assets/images/HV8-48x48.png",
        })
    };

    inline constexpr auto DefaultFonts {
        std::to_array<std::string_view>({
            "assets/fonts/Audiowide-Regular.ttf",
            "assets/fonts/SairaStencilOne-Regular.ttf",
        })
    };
}
