// ImGui SDL2 binding with OpenGL3
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#ifndef IMGUI_GL3_HPP_
#define IMGUI_GL3_HPP_

#define IMGUI_API

//#ifdef __ANDROID__
//#define IMGUI_INIT(x) 
//#define IMGUI_EVENTS(x)
//#define IMGUI_UPDATE
//#define IMGUI_RENDER
//#define IMGUI_UNINIT
//#else
#ifdef USE_IMGUI
#define IMGUI_INIT(x) ImGui_ImplSdlGL3_Init(x)
//#define IMGUI_EVENTS(x) if(!ImGui_ImplSdlGL3_ProcessEvent(x))
#define IMGUI_EVENTS(x) ImGui_ImplSdlGL3_ProcessEvent(x);
#define IMGUI_UPDATE doImGui();
#define IMGUI_RENDER ImGui::Render();
#define IMGUI_UNINIT ImGui_ImplSdlGL3_Shutdown()
#else
#define IMGUI_INIT(x) 
#define IMGUI_EVENTS(x)
#define IMGUI_UPDATE
#define IMGUI_RENDER
#define IMGUI_UNINIT
#endif //USE_IMGUI
//#endif //__ANDROID__

struct SDL_Window;
typedef union SDL_Event SDL_Event;

IMGUI_API bool        ImGui_ImplSdlGL3_Init(SDL_Window* window);
IMGUI_API void        ImGui_ImplSdlGL3_Shutdown();
IMGUI_API void        ImGui_ImplSdlGL3_NewFrame(SDL_Window* window);
IMGUI_API bool        ImGui_ImplSdlGL3_ProcessEvent(const SDL_Event& event);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void        ImGui_ImplSdlGL3_InvalidateDeviceObjects();
IMGUI_API bool        ImGui_ImplSdlGL3_CreateDeviceObjects();

#endif //IMGUI_GL3_HPP_
