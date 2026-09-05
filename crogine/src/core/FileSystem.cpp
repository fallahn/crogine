/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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

#ifndef __ANDROID__
#include "tinyfiledialogs.h"
#endif
#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/Log.hpp>

#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <SDL3/SDL.h>

//TODO check this macro works on all windows compilers
//(only tested in VC right now)
#ifdef _WIN32
#include <Windows.h>
#include <shlobj.h>
#define PATH_SEPARATOR_CHAR '\\'
#define PATH_SEPARATOR_STRING "\\"
#ifdef _MSC_VER
#include <direct.h> //gcc doesn't use this
#endif //_MSC_VER
#else
#include <libgen.h>
#include <dirent.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define PATH_SEPARATOR_CHAR '/'
#define PATH_SEPARATOR_STRING "/"

#ifdef __linux__
#define MAX_PATH 512
#include <string.h>
#include <stdlib.h>

#elif defined(__APPLE__)
#define MAX_PATH PATH_MAX
#endif

#endif //_WIN32

namespace
{
    std::vector<std::string> parseFileFilter(const std::string& filter)
    {
        std::vector<std::string> retVal;

        std::string current;
        std::stringstream ss(filter);
        while (std::getline(ss, current, ','))
        {
            retVal.push_back("*." + current);
        }
        return retVal;
    }

    struct FileDialogueCallbackResult final
    {
        std::filesystem::path result; //TODO this needs to be a vector in the case of multiple files...
        std::atomic_bool hasResult = false;
    };

    static void SDLCALL fileDialogueCallback(void* userData, const char* const* fileList, int /*filterIndex*/)
    {
        auto& callbackResult = *reinterpret_cast<FileDialogueCallbackResult*>(userData);

        //this might be nullptr if there was an error
        if (fileList)
        {
            //TODO push back or concat the list...
            while (*fileList)
            {
                callbackResult.result = *fileList;
            }
            fileList++;
        }

        //TODO filterIndex will return the index of the given
        //file filters selected by the user - not used currently

        callbackResult.hasResult = true;
    }
}

using namespace cro;

std::vector<std::filesystem::path> FileSystem::listFiles(const std::filesystem::path& path)
{
    std::vector<std::filesystem::path> results;

    std::error_code ec;
    std::filesystem::directory_iterator it(path, ec);
    
    if (ec)
    {
        LogW << "List files: " << path << " " << ec.message() << std::endl;
        return results;
    }

    for (const auto& dir : it)
    {
        if (dir.is_regular_file())
        {
            results.push_back(dir.path().filename());
        }
    }
    return results;
}

std::filesystem::path FileSystem::getFileExtension(const std::filesystem::path& path)
{
    //yes this is clearly easier to call immediately instead of this function :)
    return path.extension();
}

std::filesystem::path FileSystem::getFileName(const std::filesystem::path& path)
{
    //same as above :)
    return path.filename();
}

std::filesystem::path FileSystem::getFilePath(const std::filesystem::path& path)
{
    return path.parent_path();
}

bool FileSystem::fileExists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool FileSystem::createDirectory(const std::filesystem::path& path)
{
    //if this throws here check the path passed in.
    std::error_code ec;
    if (!std::filesystem::create_directories(path, ec))
    {
        //this might be 0 if the directory already exists
        if (ec.value() != 0)
        {
            std::stringstream ss;
            ss << ec.message() << " - Error Code: " << ec.value();

            //TODO these are WinAPI error codes - haven't tested
            //other platforms to see if these are what they report.
            switch (ec.value())
            {
            default: ss << " (unknown error)"; break;
            case 1: ss << " (invalid function)"; break;
            case 2: ss << " (file not found)"; break;
            case 3: ss << " (path not found)"; break;
            case 5: ss << " (access denied)"; break;
            case 8: ss << " (not enough memory)"; break;
            case 18: ss << " (no more files)"; break;
            case 32: ss << " (sharing violation)"; break;
            case 50: ss << " (not supproted)"; break;
            case 53: ss << " (bad netpath)"; break;
            case 80: ss << " (file exists)"; break;
            case 87: ss << " (invalid parameter)"; break;
            case 122: ss << " (insufficient buffer)"; break;
            case 123: ss << " (invalid name)"; break;
            case 145: ss << " (directory not empty)"; break;
            case 183: ss << " (already exists)"; break;
            case 206: ss << " (filename exceeds range)"; break;
            case 267: ss << " (invalid directory name)"; break;
            case 4393: ss << " (reparse tag invalid)"; break;
            }

            Logger::log(ss.str(), Logger::Type::Error, Logger::Output::All);
        }
        return false;
    }
    return true;
}

bool FileSystem::directoryExists(const std::filesystem::path& path)
{
    std::filesystem::directory_entry d = std::filesystem::directory_entry(path);
    return d.exists();
}

std::vector<std::filesystem::path> FileSystem::listDirectories(const std::filesystem::path& path)
{
    std::vector<std::filesystem::path> retVal;

    std::error_code ec;
    std::filesystem::directory_iterator it(path, ec);

    if (ec)
    {
        LogW << "List directories: " << path << " " <<ec.message() << std::endl;
        return retVal;
    }

    for (const auto& dir : it)
    {
        if (dir.is_directory())
        {
            retVal.push_back(dir.path().filename());
        }
    }
    return retVal;
}

std::filesystem::path FileSystem::getCurrentDirectory()
{
    std::error_code ec;
    return std::filesystem::current_path(ec);
//#ifdef _WIN32
//    TCHAR output[FILENAME_MAX];
//    if (GetCurrentDirectory(FILENAME_MAX, output) == 0)
//    {
//        Logger::log("Failed to find the current working directory, error: " + std::to_string(GetLastError()), Logger::Type::Error);
//        return{};
//    }
//    std::string retVal(output);
//    std::replace(retVal.begin(), retVal.end(), '\\', '/');
//    return retVal;
//#else //this may not work on macOS
//    char output[FILENAME_MAX];
//    if (getcwd(output, FILENAME_MAX) == 0)
//    {
//        Logger::log("Failed to find the current working directory, error: " + std::to_string(errno), Logger::Type::Error);
//        return{};
//    }
//    return{ output };
//#endif //_WIN32
}

bool FileSystem::setCurrentDirectory(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::current_path(path, ec);

    return !ec;
//#ifdef _WIN32
//    auto windowsPath = path;
//    std::replace(windowsPath.begin(), windowsPath.end(), '/', '\\');
//    return _chdir(windowsPath.c_str()) == 0;
//#else
//    return chdir(path.c_str()) == 0;
//#endif
}

void FileSystem::removeDirectory(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec);

    if (ec)
    {
        LogE << "unable to remove directory " << path << ": error code " << ec.value() << " " << ec.message() << std::endl;
    }
}

std::filesystem::path FileSystem::getRelativePath(const std::filesystem::path& path, const std::filesystem::path& root)
{
    std::error_code ec;
    return std::filesystem::relative(path, root, ec);
    //auto currentPath = root;
    //std::replace(std::begin(path), std::end(path), '\\', '/');
    //std::replace(std::begin(currentPath), std::end(currentPath), '\\', '/');
    //
    //int i = -1;
    //auto pos = std::string::npos;
    //std::size_t length = 0;
    //auto currentPos = std::string::npos;

    //do
    //{
    //    pos = path.find(currentPath);
    //    length = currentPath.size();

    //    currentPos = currentPath.find_last_of('/');
    //    if (currentPos != std::string::npos)
    //    {
    //        currentPath = currentPath.substr(0, currentPos);
    //    }
    //    i++;
    //} while (pos == std::string::npos && currentPos != std::string::npos);

    //std::string retVal;
    //while (i-- > 0)
    //{
    //    retVal += "../";
    //}
    //retVal += path.substr(pos + length + 1); //extra 1 for trailing '/'
    //return retVal;
}

std::filesystem::path FileSystem::openFileDialogue(const std::filesystem::path& defaultDir, const std::string& filter, bool selectMultiple)
{
#ifdef __ANDROID__
    Logger::log("File Dialogues are not supported", Logger::Type::Error);
    return {};
#else
    //SDL is actually much more flexible with file filters, but we're (currently)
    //bound by the rules of backwards compatibility. Note that filters must exist
    //until the file dialogue box is complete, hence the weird pointing of chars
    //static const std::string defaultName = "All Files";
    //static const std::string defaultFilter = "*";
    //static const std::string filterName = "Files";

    //std::string filterList = filter;
    //std::replace(filterList.begin(), filterList.end(), ',', ';');

    //SDL_DialogFileFilter filters = {};
    //if (filter.empty())
    //{
    //    filters.name = defaultName.c_str();
    //    filters.pattern = defaultFilter.c_str();
    //}
    //else
    //{
    //    filters.name = filterName.c_str();
    //    filters.pattern = filterList.c_str();
    //}

    //FileDialogueCallbackResult callbackResult;

    //const auto threadFunc = [&]() {
    //    SDL_ShowOpenFileDialog(fileDialogueCallback, &callbackResult, /*App::getWindow().m_window*/nullptr,
    //        &filters, 1, U8PATH_CAST(defaultDir), selectMultiple);
    //    };
    //std::thread t(std::bind(threadFunc));

    //t.join();

    //while (!callbackResult.hasResult)
    //{
    //    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    //}
    //return callbackResult.result;


    //filter is comma delimited list
    const auto filters = parseFileFilter(filter);
    std::vector<const char*> filterArray;
    for (const auto& str : filters)
    {
        filterArray.push_back(str.c_str());
    }    

    const auto path = tinyfd_openFileDialog("Open File", U8PATH_CAST(defaultDir), static_cast<std::int32_t>(filterArray.size()), filterArray.data(), nullptr, selectMultiple ? 1 : 0);

    return path ? path : std::filesystem::path();
#endif //__ANDROID__
}

std::future<std::vector<std::filesystem::path>> FileSystem::openFileDialogueAsync(const std::filesystem::path& defaultDir, const std::string& filter, bool selectMultiple)
{
    assert(false); //not implemented!
    return {};
}

std::filesystem::path FileSystem::openFolderDialogue(const std::filesystem::path& defPath)
{
#ifdef __ANDROID__
    Logger::log("File Dialogues are not supported", Logger::Type::Error);
    return {};
#else
    const auto path = tinyfd_selectFolderDialog("Select Folder", U8PATH_CAST(defPath));
    return path ? path : std::filesystem::path();
#endif //__ANDROID__
}

std::filesystem::path FileSystem::saveFileDialogue(const std::filesystem::path& defaultDir, const std::string& filter)
{
#ifdef __ANDROID__
    Logger::log("File Dialogues are not supported", Logger::Type::Error);
    return {};
#else
    //filter is comma delimited list
    const auto filters = parseFileFilter(filter);

    std::vector<const char*> filterArray;
    for (const auto& str : filters)
    {
        filterArray.push_back(str.c_str());
    }

    const auto path = tinyfd_saveFileDialog("Save File", U8PATH_CAST(defaultDir), static_cast<int>(filterArray.size()), filterArray.data(), nullptr);

    return path ? path : std::filesystem::path();
#endif //__ANDROID__
}

std::filesystem::path FileSystem::getResourcePath()
{
#ifdef __APPLE__
    //ugh - cwd when using bundles is a pain, so at least add some
    //checks to make sure we're not concatinating an existing part of the path
    
    //TODO this will throw if the resource dir path contains characters which need utf8 conversion
    auto rpath = SDL_GetBasePath(); ;// resourcePath();
    if (m_resourceDirectory.string().find(rpath) == std::string::npos)
    {
        return rpath / m_resourceDirectory;
    }

    return m_resourceDirectory;
#endif
    return m_resourceDirectory;
}

void FileSystem::setResourceDirectory(const std::filesystem::path& path)
{
    m_resourceDirectory = path;
    //std::replace(m_resourceDirectory.begin(), m_resourceDirectory.end(), '\\','/');

    //if (!path.empty())
    //{
    //    //strip preceeding slashes
    //    if(m_resourceDirectory[0] == '/')
    //    {
    //        m_resourceDirectory = m_resourceDirectory.substr(1);
    //    }

    //    //and add post slashes if missing
    //    if (m_resourceDirectory.back() != '/')
    //    {
    //        m_resourceDirectory.push_back('/');
    //    }
    //}

    LogI << "Resource directory set to " << m_resourceDirectory << std::endl;
}

bool FileSystem::showMessageBox(const std::string& title, const std::string& message, ButtonType buttonType, IconType iconType)
{
    SDL_MessageBoxData data = {};
    data.window = App::getWindow().m_window;
    data.title = title.c_str();
    data.message = message.c_str();
    data.flags = SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;

    std::array<SDL_MessageBoxButtonData, 3> buttons = {};
    data.buttons = buttons.data();
    data.colorScheme = nullptr;

    for (auto i = 0; i < 3; ++i)
    {
        buttons[i].buttonID = i;
    }

    switch (buttonType)
    {
    default:
    case ButtonType::OK:
        data.numbuttons = 1;
        buttons[0].text = "OK";
        break;
    case ButtonType::OKCancel:
        data.numbuttons = 2;
        buttons[0].text = "OK";
        buttons[1].text = "Cancel";
        break;
    case ButtonType::YesNo:
        data.numbuttons = 2;
        buttons[0].text = "Yes";
        buttons[1].text = "No";
        break;
    case ButtonType::YesNoCancel:
        data.numbuttons = 3;
        buttons[0].text = "Yes";
        buttons[1].text = "No";
        buttons[2].text = "Cancel";
        break;
    }

    std::string icon;
    switch (iconType)
    {
    default:
    case IconType::Error:
        data.flags |= SDL_MESSAGEBOX_ERROR;
        break;
    case IconType::Info:
    case IconType::Question:
        //hmm no question type in SDL
        data.flags |= SDL_MESSAGEBOX_INFORMATION;
        break;
    case IconType::Warning:
        data.flags |= SDL_MESSAGEBOX_WARNING;
        break;
    }

    std::int32_t resultID = -1;
    if (SDL_ShowMessageBox(&data, &resultID))
    {
        return resultID == 0;
    }

    LogE << "Message Box: " << SDL_GetError() << std::endl;
    return false;
}

void FileSystem::showNotification(const std::string& title, const std::string& message, IconType iconType)
{
    std::string icon;
    switch (iconType)
    {
    default:
    case IconType::Question:
        [[fallthrough]];
    case IconType::Info:
        icon = "info";
        break;
    case IconType::Error:
        icon = "error";
        break;
    case IconType::Warning:
        icon = "warning";
        break;
    }

    //tinyfd_notifyPopup(title.c_str(), message.c_str(), icon.c_str());
#if SDL_VERSIONNUM_MINOR >= 6
    SDL_ShowNotification(title.c_str(), message.c_str());
#endif
}

//private
std::filesystem::path FileSystem::m_resourceDirectory = std::filesystem::path();
