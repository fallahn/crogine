/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#include <crogine/core/Console.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/audio/AudioMixer.hpp>
#include <crogine/gui/detail/imgui.h>

#include <list>
#include <unordered_map>
#include <algorithm>
#include <array>

using namespace cro;
namespace ui = ImGui;

namespace
{
    using ConsoleTab = std::tuple<std::string, std::function<void()>, const GuiClient*>;
    std::vector<ConsoleTab> m_consoleTabs;


    std::vector<glm::uvec2> resolutions;
    int currentResolution = 0;
    std::array<char, 1024> resolutionNames{};
    

    constexpr std::size_t MAX_INPUT_CHARS = 400;
    char input[MAX_INPUT_CHARS];
    std::list<std::pair<std::string, ImVec4>> buffer;
    const std::size_t MAX_BUFFER = 50;

    std::list<std::string> history;
    const std::size_t MAX_HISTORY = 10;
    std::int32_t historyIndex = -1;

    bool visible = false;

    std::unordered_map<std::string, std::pair<Console::Command, const ConsoleClient*>> commands;

    ConfigFile convars;
    const std::string convarName("convars.cfg");

    const ImVec4 WarningColour(1.f, 0.6f, 0.f, 1.f);
    const ImVec4 ErrorColour(1.f, 0.f, 0.f, 1.f);
    const ImVec4 DefaultColour(1.f, 1.f, 1.f, 1.f);

    bool isNewFrame = true;
    bool showDemoWindow = false;
}
int textEditCallback(ImGuiInputTextCallbackData* data);

std::vector<std::string> Console::m_debugLines;

//public
void Console::print(const std::string& line)
{
    if (line.empty()) return;

    std::string timestamp(/*"<" + */SysTime::timeString() + " ");
    Message::ConsoleEvent::Level level = Message::ConsoleEvent::Info;

    auto colour = DefaultColour;
    if (line.find("ERROR") != std::string::npos)
    {
        colour = ErrorColour;
        level = Message::ConsoleEvent::Error;
    }
    else if (line.find("WARNING") != std::string::npos)
    {
        colour = WarningColour;
        level = Message::ConsoleEvent::Warning;
    }

    buffer.push_back(std::make_pair(timestamp + line, colour));
    if (buffer.size() > MAX_BUFFER)
    {
        buffer.pop_front();
    }

    if (isNewFrame)
    {
        auto* msg = App::getInstance().getMessageBus().post<Message::ConsoleEvent>(Message::ConsoleMessage);
        msg->type = Message::ConsoleEvent::LinePrinted;
        msg->level = level;
        isNewFrame = false;
    }
}

void Console::show()
{
    visible = !visible;

    auto* msg = App::getInstance().getMessageBus().post<Message::ConsoleEvent>(Message::ConsoleMessage);
    msg->type = visible ? Message::ConsoleEvent::Opened : Message::ConsoleEvent::Closed;
}

void Console::doCommand(const std::string& str)
{
    //store in history
    history.push_back(str);
    if (history.size() > MAX_HISTORY)
    {
        history.pop_front();
    }
    historyIndex = -1;

    //parse the command from the string
    std::string command(str);
    std::string params;
    //shave off parameters if they exist
    std::size_t pos = command.find_first_of(' ');
    if (pos != std::string::npos)
    {
        if (pos < command.size())
        {
            params = command.substr(pos + 1);
        }
        command.resize(pos);
    }

    //locate command and execute it passing in params
    auto cmd = commands.find(command);
    if (cmd != commands.end())
    {
        cmd->second.first(params);
    }
    else
    {
        //check to see if we have a convar
        if (auto* convar = convars.findObjectWithName(command); convar)
        {
            if (!params.empty())
            {
                if (auto* value = convar->findProperty("value"); value)
                {
                    value->setValue(params);
                    
                    //raise a message pointing to convar so we can act on it if needed
                    auto* msg = App::getInstance().getMessageBus().post<Message::ConsoleEvent>(Message::ConsoleMessage);
                    msg->type = Message::ConsoleEvent::ConvarSet;
                    msg->convar = convar;
                    
                    print(command + " set to " + params);
                }
            }
            else
            {
                
                if (auto* help = convar->findProperty("help"); help)
                {
                    print(help->getValue<std::string>());
                }
            }
        }
        else
        {
            print(command + ": command not found!");
        }
    }

    input[0] = '\0';
}

void Console::addConvar(const std::string& name, const std::string& defaultValue, const std::string& helpStr)
{
    if (!convars.findObjectWithName(name))
    {
        auto* obj = convars.addObject(name);
        obj->addProperty("value", defaultValue);
        obj->addProperty("help", helpStr);
    }
}

void Console::printStat(const std::string& name, const std::string& value)
{
    m_debugLines.push_back(name + ":" + value);
}

bool Console::isVisible()
{
    return visible;
}

const std::string& Console::getLastOutput()
{
    return buffer.back().first;
}

//private
void Console::addCommand(const std::string& name, const Command& command, const ConsoleClient* client = nullptr)
{
    CRO_ASSERT(!name.empty(), "Command cannot have an empty string");
    if (commands.count(name) != 0)
    {
        Logger::log("Command \"" + name + "\" exists! Command has been overwritten!", Logger::Type::Warning, Logger::Output::All);
        commands[name] = std::make_pair(command, client);
    }
    else
    {
        commands.insert(std::make_pair(name, std::make_pair(command, client)));
    }
}

void Console::removeCommands(const ConsoleClient* client)
{
    //make sure this isn't nullptr else most if not all commands will get removed..
    CRO_ASSERT(client, "You really don't want to do that");

    for (auto i = commands.begin(); i != commands.end();)
    {
        if (i->second.second == client)
        {
            i = commands.erase(i);
        }
        else
        {
            ++i;
        }
    }
}

void Console::newFrame()
{
    isNewFrame = true;
}

void Console::draw()
{
#ifdef CRO_DEBUG_
    if (showDemoWindow)
    {
        ImGui::ShowDemoWindow();
    }
#endif

    if (visible)
    {
        ui::SetNextWindowSizeConstraints({ 640, 480 }, { 1024.f, 768.f });
        if (ui::Begin("Console", &visible, ImGuiWindowFlags_NoScrollbar))
        {
            if (ui::BeginTabBar("Tabs"))
            {
                //console
                if (ui::BeginTabItem("Console"))
                {
                    ui::BeginChild("ScrollingRegion", ImVec2(0, -ui::GetFrameHeightWithSpacing()), false/*, ImGuiWindowFlags_HorizontalScrollbar*/);
                    static bool scrollToEnd = false;

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
                    for (const auto& [str, colour] : buffer)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, colour);
                        ImGui::TextWrapped("%s", str.c_str());
                        ImGui::PopStyleColor();
                    }
                    ImGui::PopStyleVar();

                    if (scrollToEnd ||
                        ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    {
                        ImGui::SetScrollHereY(1.0f);
                        scrollToEnd = false;
                    }
                    ui::EndChild();

                    ui::Separator();

                    ui::PushItemWidth(620.f);

                    bool focus = false;
                    if ((focus = ImGui::InputText(" ", input, MAX_INPUT_CHARS,
                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                        &textEditCallback)))
                    {
                        scrollToEnd = true;
                        doCommand(input);
                    }

                    ui::PopItemWidth();

                    ImGui::SetItemDefaultFocus();
                    if (focus || ImGui::IsItemHovered()
                        || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                            && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                    {
                        ImGui::SetKeyboardFocusHere(-1);
                    }

                    ui::EndTabItem();
                }

                //video options
                if (ui::BeginTabItem("Video"))
                {
                    ui::Combo("Resolution", &currentResolution, resolutionNames.data());

                    static bool fullScreen = App::getWindow().isFullscreen();
                    ui::Checkbox("Full Screen", &fullScreen);

                    bool vsync = App::getWindow().getVsyncEnabled();
                    if (ImGui::Checkbox("Vsync", &vsync))
                    {
                        App::getWindow().setVsyncEnabled(vsync);
                    }

                    bool aa = App::getWindow().getMultisamplingEnabled();
                    if (ImGui::Checkbox("Multisampling", &aa))
                    {
                        App::getWindow().setMultisamplingEnabled(aa);
                    }
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                    ImGui::Text("Note not all platforms support this. Doesn't affect post processes or RenderTextures.");
                    ImGui::PopStyleColor();

                    if (ui::Button("Apply", { 50.f, 20.f }))
                    {
                        //apply settings
                        if (fullScreen != App::getWindow().isFullscreen())
                        {
                            App::getWindow().setFullScreen(fullScreen);
                        }
                        else
                        {
                            App::getWindow().setSize(resolutions[currentResolution]);
                        }
                    }
                    ui::EndTabItem();
                }

                //audio
                if (ui::BeginTabItem("Audio"))
                {
                    ui::Text("NOTE: only AudioSystem sounds are affected.");

                    static float maxVol = AudioMixer::getMasterVolume();
                    ui::SliderFloat("Master", &maxVol, 0.f, 1.f);
                    AudioMixer::setMasterVolume(maxVol);

                    static std::array<float, AudioMixer::MaxChannels> channelVol;
                    for (auto i = 0u; i < AudioMixer::MaxChannels; ++i)
                    {
                        channelVol[i] = AudioMixer::getVolume(i);
                        if (ui::SliderFloat(AudioMixer::getLabel(i).c_str(), &channelVol[i], 0.f, 1.f))
                        {
                            AudioMixer::setVolume(channelVol[i], i);
                        }
                    }
                    ui::EndTabItem();
                }

                //stats
                if (ui::BeginTabItem("Stats"))
                {
                    ui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                    ui::NewLine();
                    for (auto& line : m_debugLines)
                    {
                        ImGui::TextUnformatted(line.c_str());
                    }
                    ui::EndTabItem();
                }
                m_debugLines.clear();
                m_debugLines.reserve(10);


                //display registered tabs
                for (const auto& tab : m_consoleTabs)
                {
                    const auto& [name, func, c] = tab;
                    if (ImGui::BeginTabItem(name.c_str()))
                    {
                        func();
                        ui::EndTabItem();
                    }
                }

                ui::EndTabBar();
            }
        }

        ui::End();
    }
    else
    {
        //read current active values
        const auto& size = App::getWindow().getSize();
        for (auto i = 0u; i < resolutions.size(); ++i)
        {
            if (resolutions[i].x == size.x && resolutions[i].y == size.y)
            {
                currentResolution = i;
                break;
            }
        }
        return;
    }

    //separate clause - this may have been changed by closing the window
    if (!visible)
    {
        App::getInstance().saveSettings();
        auto* msg = App::getInstance().getMessageBus().post<Message::ConsoleEvent>(Message::ConsoleMessage);
        msg->type = Message::ConsoleEvent::Closed;
    }
}

void Console::addConsoleTab(const std::string& name, const std::function<void()>& f, const GuiClient* c)
{
    m_consoleTabs.push_back(std::make_tuple(name, f, c));
}

void Console::removeConsoleTab(const GuiClient* c)
{
    if (m_consoleTabs.empty()) return;

    m_consoleTabs.erase(std::remove_if(std::begin(m_consoleTabs),
        std::end(m_consoleTabs),
        [c](const ConsoleTab& tab)
        {
            const auto& [name, f, p] = tab;
            return c == p;
        }), std::end(m_consoleTabs));
}

void Console::init()
{
    resolutions = App::getWindow().getAvailableResolutions();
    std::reverse(std::begin(resolutions), std::end(resolutions));

    int i = 0;
    for (auto r = resolutions.begin(); r != resolutions.end(); ++r)
    {
        std::string width = std::to_string(r->x);
        std::string height = std::to_string(r->y);

        for (char c : width)
        {
            resolutionNames[i++] = c;
        }
        resolutionNames[i++] = ' ';
        resolutionNames[i++] = 'x';
        resolutionNames[i++] = ' ';
        for (char c : height)
        {
            resolutionNames[i++] = c;
        }
        resolutionNames[i++] = '\0';
    }

    //------default commands------//
    //list all available commands to the console
    addCommand("help",
        [](const std::string&)
    {
        Console::print("Available Commands:");
        for (const auto& c : commands)
        {
            Console::print(c.first);
        }

        Console::print("Available Variables:");
        const auto& objects = convars.getObjects();
        for (const auto& o : objects)
        {
            std::string str = o.getName();
            const auto& properties = o.getProperties();
            for (const auto& p : properties)
            {
                if (p.getName() == "help")
                {
                    str += " " + p.getValue<std::string>();
                }
            }
            Console::print(str);
        }
    });

    //search for a command
    addCommand("find",
        [](const std::string& param)
    {
        if (param.empty())
        {
            Console::print("Usage: find <command> where <command> is the name or part of the name of a command to find");
        }
        else
        {           
            auto term = param;
            std::size_t p = term.find_first_of(" ");
            if (p != std::string::npos)
            {
                term = term.substr(0, p);
            }
            auto searchterm = term;
            auto len = term.size();

            std::vector<std::string> results;
            while (len > 0)
            {
                for (const auto& c : commands)
                {
                    if (c.first.find(term) != std::string::npos
                        && std::find(results.begin(), results.end(), c.first) == results.end())
                    {
                        results.push_back(c.first);
                    }
                }
                term = term.substr(0, len--);
            }

            if (!results.empty())
            {
                Console::print("Results for: " + searchterm);
                for (const auto& str : results)
                {
                    Console::print(str);
                }
            }
            else
            {
                Console::print("No results for: " + searchterm);
            }
        }
    });

#ifdef CRO_DEBUG_
    //shows the imgui debug window
    addCommand("r_drawDemoWindow",
        [](const std::string& param) 
        {
            if (param == "0")
            {
                showDemoWindow = false;
            }
            else if (param == "1")
            {
                showDemoWindow = true;
            }
            else
            {
                print("Usage: r_drawDemoWindow <0|1>. Toggles visibility of ImGui demo window.");
            }
        });
#endif

    //opens the screenshot directory
    addCommand("show_screenshots",
        [](const std::string&)
        {
            auto outPath = cro::App::getInstance().getPreferencePath() + "screenshots/";
            std::replace(outPath.begin(), outPath.end(), '\\', '/');

            if (!cro::FileSystem::directoryExists(outPath))
            {
                cro::FileSystem::createDirectory(outPath);
            }

            cro::Util::String::parseURL(outPath);
        });

    //quits
    addCommand("quit",
        [](const std::string&)
    {
        App::quit();
    });

    //loads any convars which may have been saved
    convars.loadFromFile(App::getPreferencePath() + convarName, false);
    //TODO execute callback for each to make sure values are applied (? can always add a command which executes something while updating value)
}

void Console::finalise()
{
    //TODO if we add anything else here make sure
    //to modify setConvarValue as it calls this to
    //update the file with new values.
    convars.save(App::getPreferencePath() + convarName);
}

ConfigFile& Console::getConvars()
{
    return convars;
}

int textEditCallback(ImGuiInputTextCallbackData* data)
{
    //use this to scroll up and down through command history
    //TODO create an auto complete thinger

    switch (data->EventFlag)
    {
    default: break;
    case ImGuiInputTextFlags_CallbackCompletion: //user pressed tab to complete
    case ImGuiInputTextFlags_CallbackHistory:
    {
        const int prev_history_pos = historyIndex;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (historyIndex == -1)
            {
                historyIndex = static_cast<std::int32_t>(history.size()) - 1;
            }
            else if (historyIndex > 0)
            {
                historyIndex--;
            }
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (historyIndex != -1)
            {
                if (++historyIndex >= history.size())
                {
                    historyIndex = -1;
                }
            }
        }

        //a better implementation would preserve the data on the current input line along with cursor position.
        if (prev_history_pos != historyIndex)
        {
            data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen =
                (int)snprintf(data->Buf, (size_t)data->BufSize, "%s", (historyIndex >= 0) ? std::next(history.begin(), historyIndex)->c_str() : "");
            data->BufDirty = true;
        }
    }
    break;
    }

    return 0;
}