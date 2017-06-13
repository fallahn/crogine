/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_CONSOLE_HPP_
#define CRO_CONSOLE_HPP_

#include <crogine/Config.hpp>

#include <functional>

namespace cro
{
    class ConsoleClient;
    
    /*!
    \brief Console class.
    The console class provides a feddback window and interface
    with the engine. Any class can register a command with the
    console as long as it inherits the ConsoleClient interface.
    Note: the console is only available when crogine is built
    with USE_IMGUI defined.
    */
    class CRO_EXPORT_API Console final
    {
    public:
        /*!
        Command functions are registered with their associated
        string, and executed when that string is entered into the
        consle window. Any further characters entered after the
        command are passed into the command function as a paramter.
        If this is a list of paramters it is up to the command
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
        Allows for programatically executing arbitrary commands
        */
        static void doCommand(const std::string&);

    private:
        friend class App;
        friend class ConsoleClient;

        static void init();

        static void addCommand(const std::string& name, const Command& cmd, const ConsoleClient* owner);
        static void removeCommands(const ConsoleClient*); //removes all commands belonging to the given client

        static void draw();
    };
}

#endif //CRO_CONSOLE_HPP_