#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_ENABLE_FREETYPE
#define IMGUI_USE_WCHAR32
#include <crogine/Config.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec4.hpp>
#define IM_VEC2_CLASS_EXTRA \
        ImVec2(const glm::vec2& f) { x = f.x; y = f.y; } \
        operator glm::vec2() const { return glm::vec2(x,y); }

#define IM_VEC4_CLASS_EXTRA \
        ImVec4(const glm::vec4& f) { x = f.x; y = f.y; z = f.z; w = f.w; } \
        operator glm::vec4() const { return glm::vec4(x,y,z,w); } \
        ImVec4(const cro::Colour& c) { x = c.getRed(); y = c.getGreen(); z = c.getBlue(); w = c.getAlpha(); }





