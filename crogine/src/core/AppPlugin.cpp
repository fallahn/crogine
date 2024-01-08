/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/StateStack.hpp>

using namespace cro;

#ifdef _WIN32
typedef int(__cdecl* Entry)(StateStack*, std::any*);
typedef void(__cdecl* Exit)(StateStack*);

void App::loadPlugin(const std::string& path, StateStack& stateStack)
{
    if (m_pluginHandle)
    {
        unloadPlugin(stateStack);
    }

    auto fullPath = path;
    std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
    if (fullPath.back() != '/')
    {
        fullPath += '/';
    }
    fullPath += "croplug.dll";

    m_pluginHandle = LoadLibrary(TEXT(fullPath.c_str()));

    if (m_pluginHandle)
    {
        auto entry = (Entry)GetProcAddress(m_pluginHandle, "begin");
        auto exit = (Exit)GetProcAddress(m_pluginHandle, "end");
        if (entry && exit)
        {
            //update current directory for resources
            FileSystem::setResourceDirectory(path);

            int reqState = entry(&stateStack, &m_pluginSharedData);

            if (reqState != std::numeric_limits<std::int32_t>::max())
            {
                stateStack.clearStates();
                stateStack.pushState(reqState);
            }
            else
            {
                exit(&stateStack);
                m_pluginSharedData.reset();
                FreeLibrary(m_pluginHandle);
                m_pluginHandle = nullptr;
            }
        }
        else
        {
            LogE << "Entry and exit point not found in " << path << std::endl;
            FreeLibrary(m_pluginHandle);
            m_pluginHandle = nullptr;
        }
    }
    else
    {
        LogE << "Unable to load plugin at " << path << std::endl;
    }
}

void App::unloadPlugin(StateStack& stateStack)
{
    //reset this in case any plugin set it true
    m_window.setMouseCaptured(false);

    if (m_pluginHandle)
    {
        auto exit = (Exit)GetProcAddress(m_pluginHandle, "end");
        exit(&stateStack);

        m_pluginSharedData.reset();

        //set current directory to default
        FileSystem::setResourceDirectory("");

        auto result = FreeLibrary(m_pluginHandle);
        if (!result)
        {
            LogE << "Unable to correctly unload current plugin!" << std::endl;
        }
        else
        {
            m_pluginHandle = nullptr;
        }
    }
}

#else
#include <dlfcn.h>

void App::loadPlugin(const std::string& path, StateStack& stateStack)
{
    if (m_pluginHandle)
    {
        unloadPlugin(stateStack);
    }

    std::string fullPath = path;
#ifdef __APPLE__
    fullPath = FileSystem::getResourcePath() + fullPath;
    fullPath += "/libcroplug.dylib";
#else
    fullPath += "/libcroplug.so";
#endif

    m_pluginHandle = dlopen(fullPath.c_str(), RTLD_LAZY);

    if (m_pluginHandle)
    {
        int(*entryPoint)(StateStack*, std::any*);
        void(*exitPoint)(StateStack*);

        *(int**)(&entryPoint) = (int*)dlsym(m_pluginHandle, "begin");
        *(void**)(&exitPoint) = dlsym(m_pluginHandle, "end");

        if (entryPoint && exitPoint)
        {
            //update current directory for resources
            FileSystem::setResourceDirectory(path);

            auto reqState = entryPoint(&stateStack, &m_pluginSharedData);
            
            if (reqState != std::numeric_limits<std::int32_t>::max())
            {
                stateStack.clearStates();
                stateStack.pushState(reqState);
            }
            else
            {
                exitPoint(&stateStack);
                m_pluginSharedData.reset();
                dlclose(m_pluginHandle);
                m_pluginHandle = nullptr;
            }
        }
        else
        {
            LogE << "Entry and exit point not found in " << path << std::endl;
            dlclose(m_pluginHandle);
            m_pluginHandle = nullptr;
        }
    }
    else
    {
        LogE << "Unable to open plugin at " << fullPath << std::endl;
        LogE << dlerror() << std::endl;
    }
}

void App::unloadPlugin(StateStack& stateStack)
{
    if (m_pluginHandle)
    {
        void(*exitPoint)(StateStack*);
        *(void**)(&exitPoint) = dlsym(m_pluginHandle, "end");
        exitPoint(&stateStack);
        m_pluginSharedData.reset();

        //set current directory to default
        FileSystem::setResourceDirectory("");

        dlclose(m_pluginHandle);
        m_pluginHandle = nullptr;
    }
}

#endif //win32