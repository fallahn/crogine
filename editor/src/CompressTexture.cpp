/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "CompressTexture.hpp"
#include "SharedStateData.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/graphics/ImageArray.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/util/String.hpp>

#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <deque>
#include <filesystem>

namespace
{
    const std::string binName = "/nvtt_export.exe\"";
    const std::string dxt1 = "/assets/compression_presets/ktx2_export_dxt1_no_alpha.dpf";
    const std::string dxt5 = "/assets/compression_presets/ktx2_export_dxt5_alpha.dpf";

    constexpr std::size_t MaxLogEntries = 50;
    std::deque<std::string> logOutput;

    struct Compressor final
    {
        std::string binPath;
        std::string outputPath;
        std::string workingDirectory;

        std::atomic_bool running = false;
        std::mutex mutex;
        std::string logBuffer;

        std::unique_ptr<std::thread> thread;

        void log(const std::string& s)
        {
            std::scoped_lock l(mutex);
            if (!logBuffer.empty())
            {
                logBuffer += "\n";
            }
            logBuffer += s;
        }
    }compressor;
}

static inline void threadFunc()
{
    //TODO these can obviously be refactored to something more sane
    const auto compressDXT = 
        [](const std::string& file, const std::string& presetPath)
        {
            const std::string preset = " -p \"" + compressor.workingDirectory + presetPath + "\"";
            //const std::string preset = " --format bc3 --quality production --mips --mip-filter box --no-mip-gamma-correct --save-flip-y --zcmp 5 --serialized-effects-v1 \"1 20 0 \"";
            const std::string input = " \"" + compressor.outputPath + "/" + file + "\"";
            std::string output = " -o" + input;
            cro::Util::String::replace(output, ".png", ".ktx2");

            const std::string cmd = "\"" + compressor.binPath + preset + output + input;

            std::system(cmd.c_str());

            compressor.log("Compressing " + file + " with " + presetPath);
        };


    const auto files = cro::FileSystem::listFiles(compressor.outputPath);
    for (const auto& file : files)
    {
        if (cro::FileSystem::getFileExtension(file) == ".png")
        {
            static constexpr std::uint32_t MaxTexSize = 4096;
            cro::ImageArray<std::uint8_t> img;
            if (img.loadFromFile(compressor.outputPath + "/" + file))
            {
                const auto size = img.getDimensions();
                if (size.x <= MaxTexSize && size.y <= MaxTexSize)
                {
                    if (img.getChannels() == 4)
                    {
                        //as most of these files will be RGBA even though there's
                        //no transparency the only real way to test if we need DXT5
                        //over DXT1 is to test all the pixels of the image :(
                        bool useDXT5 = false;
                        for (auto y = 0u; y < size.y; ++y)
                        {
                            for (auto x = 0u; x < size.x; x+=4)
                            {
                                const auto index = (y * size.x) + x;
                                if (img[index + 3] < 255)
                                {
                                    useDXT5 = true;
                                    break;
                                }
                            }
                            if (useDXT5)
                            {
                                break;
                            }
                        }

                        if (useDXT5)
                        {
                            compressDXT(file, dxt5);
                        }
                        else
                        {
                            compressDXT(file, dxt1);
                        }
                    }
                    else if (img.getChannels() == 3)
                    {
                        compressDXT(file, dxt1);
                    }
                }
                else
                {
                    compressor.log("Skipping " + file + ": image exceeds 4096 size limit");
                }
            }
        }
    }

    compressor.running = false;
}

static inline void toolTip(const char* desc)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static inline void browseDirectory(std::string& path, const std::string& id)
{
    ImGui::SetNextItemWidth(520.f);
    ImGui::InputText(id.c_str(), &path, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();

    const std::string label = "Browse" + id;
    if (ImGui::Button(label.c_str()))
    {
        const auto newPath = cro::FileSystem::openFolderDialogue(path);
        if (!newPath.empty())
        {
            path = newPath;
        }
    }
}

void compressTextureWindow(SharedStateData& sharedData)
{
    ImGui::SetNextWindowSize({ 640.f, 600.f });
    if (ImGui::Begin("Compress Texture Directory", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
        //TODO we wcould use the nvtt lib directly - but I have no idea what the licensing
        //allows for an open source project, so I'm just going to call the bin from the command line
        browseDirectory(sharedData.nvttPath, "##0");
        toolTip("Path the Nvidia Texture Tools");
        browseDirectory(sharedData.compressionDirectory, "##1");
        toolTip("Path to directory of images to compress");

        if (ImGui::Button("Compress")
            && !compressor.running)
        {
            compressor.binPath = "\"" + sharedData.nvttPath + binName;
            compressor.outputPath = sharedData.compressionDirectory;
            compressor.workingDirectory = cro::FileSystem::getCurrentDirectory();

            compressor.running = true;
            compressor.thread = std::make_unique<std::thread>(threadFunc);
            compressor.thread->detach();
        }

        std::string temp;
        {
            std::scoped_lock l(compressor.mutex);
            if (!compressor.logBuffer.empty())
            {
                temp.swap(compressor.logBuffer);
            }
        }

        if (!temp.empty())
        {
            logOutput.push_back(temp);
            if (logOutput.size() > MaxLogEntries)
            {
                logOutput.pop_front();
            }
        }

        std::string displayBuffer;
        for (const auto& e : logOutput)
        {
            displayBuffer += e + "\n";
        }
        if (!displayBuffer.empty())
        {
            //remove final newline
            displayBuffer.pop_back();
        }

        ImGui::BeginChild("Output", {0.f, 0.f}, ImGuiChildFlags_Border);
        ImGui::TextWrapped(displayBuffer.c_str());
        ImGui::EndChild();
    }
    ImGui::End();
}