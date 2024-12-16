/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <functional>
#include <string>
#include <vector>

namespace cro
{
    class ConsoleClient;
    class GuiClient;
    
    /*!
    \brief Console class.
    The console class provides a feedback window and interface
    with the engine. Any class can register a command with the
    console as long as it inherits the ConsoleClient interface.
    */
    class CRO_EXPORT_API Console final
    {
    public:
        /*!
        Command functions are registered with their associated
        string, and executed when that string is entered into the
        console window. Any further characters entered after the
        command are passed into the command function as a parameter.
        If this is a list of parameters it is up to the command
        function implementation to properly parse the given string.
        */
        using Command = std::function<void(const std::string&)>;

        /*!
        \brief Prints the given string to the console window
        */
        static void print(const std::string&);

        /*!
        \brief Toggles the console window visibility
        */
        static void show();

        /*!
        \brief Executes the given command line.
        Allows for programmatically executing arbitrary commands
        */
        static void doCommand(const std::string&);

        /*!
        \brief Adds a convar it if it doesn't exist.
        \param name Name of the variable as it appears in the console.
        \param value the default value of the variable
        \param helpText Optional string describing the variable which appears the console
        when searching for variable names.
        */
        static void addConvar(const std::string& name, const std::string& value, const std::string& helpText = "");

        /*!
        \brief Attempts to retrieve the current value of the given console variable
        \returns Default constructed of type if the variable doesn't exist
        */
        template <typename T>
        static T getConvarValue(const std::string& convar)
        {
            //TODO we want static assertion that T is valid for config files
            if (auto* obj = getConvars().findObjectWithName(convar); obj != nullptr)
            {
                CRO_ASSERT(obj->findProperty("value"), "");
                if (auto val = obj->findProperty("value"); val != nullptr)
                {
                    return val->getValue<T>();
                }
                return T();
            }
            LogE << convar << ": variable doesn't exist" << std::endl;
            return T();
        }

        /*!
        \brief Attempts to set a convar value.
        If the variable does not exist an error is printed to the console.
        Variables should first be created with addConvar()
        \param convar Name of the console variable to set
        \param value Must be of type T compatible with cro::ConfigFile
        */
        template <typename T>
        static void setConvarValue(const std::string& convar, T value)
        {
            if (auto* obj = getConvars().findObjectWithName(convar); obj != nullptr)
            {
                CRO_ASSERT(obj->findProperty("value"), "");
                if (auto val = obj->findProperty("value"); val != nullptr)
                {
                    val->setValue(value);
                    finalise(); //saves convar file.
                }
                return;
            }
            LogE << convar << ": variable doesn't exist" << std::endl;
        }

        /*!
        \brief Prints the given stat and value to the Stats tab in the console window
        */
        static void printStat(const std::string& name, const std::string& value);

        /*!
        \brief Returns true if the console is currently being displayed
        */
        static bool isVisible();

        /*!
        \brief Returns the last line printed to the console output
        */
        static const std::string& getLastOutput();

        /*!
        \brief dumps the entire buffer to a text file
        \param str Optional string to append to file name (eg a timestamp)
        */
        static void dumpBuffer(const std::string& str);

    private:
        friend class App;
        friend class ConsoleClient;

        static std::vector<std::string> m_debugLines;

        static void addCommand(const std::string& name, const Command& cmd, const ConsoleClient* owner);
        static void removeCommands(const ConsoleClient*); //removes all commands belonging to the given client

        static void addConsoleTab(const std::string&, const std::function<void()>&, const GuiClient*);
        static void removeConsoleTab(const GuiClient*);

        static void newFrame();
        static void draw();

        static void init();
        static void finalise();

        static cro::ConfigObject& getConvars();
    };

//#include "Console.inl"

}
