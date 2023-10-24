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

//static functions for cross platform file handling

#pragma once

#include <crogine/Config.hpp>

#include <string>
#include <vector>

namespace cro
{
    /*!
    \brief Utilities for manipulating the current file system
    Where possible these functions try to maintain utf8 file paths
    using std::filesystem - although this isn't true on macOS < 10.15
    */
    class CRO_EXPORT_API FileSystem final
    {
    public:
        /*!
        \brief Lists all the files in the given directory
        Note that when concatinating string literals make sure
        to include the utf-8 prefix
        EG: path += u8"/subdir";
        */
        static std::vector<std::string> listFiles(std::string path);
        /*!
        \brief Attempts to return a string containing the file extension
        of a given path, including the period (.)
        */
        static std::string getFileExtension(const std::string& path);
        /*!
        \brief Attempts to return the name of a file at the end of
        a given file path
        */
        static std::string getFileName(const std::string& path);
        /*!
        \brief Attempts to return the path of a given file path without
        the file name, including trailing separator char.
        */
        static std::string getFilePath(const std::string& path);
        /*!
        \brief Returns true if a file exists with the name at the given path
        Note that when calling this from an app running in a macOS bundle
        that the path should be prefixed with a call to getResourcePath()
        
        Note that when concatinating string literals make sure
        to include the utf-8 prefix
        EG: path += u8"/subdir";
        */
        static bool fileExists(const std::string& path);
        /*!
        \brief Tries to create a directory relative to the executable
        or via an absolute path.
        \returns false if creation fails and attempts to log the reason,
        else returns true.
        \param std::string path Path to create.

        Note that when concatinating string literals make sure
        to include the utf-8 prefix
        EG: path += u8"/subdir";
        */
        static bool createDirectory(const std::string& path);
        /*!
        \brief Attempts to determine if a directory at the given path exists.
        Note that when calling this from an app running in a macOS bundle
        that the path should be prefixed with a call to getResourcePath()
        \returns true if the directory exists, else false. Attempts to log any
        errors to the console.

        Note that when concatinating string literals make sure
        to include the utf-8 prefix
        EG: path += u8"/subdir";
        */
        static bool directoryExists(const std::string& path);
        /*!
        \brief Returns a vector of strings containing the names of directories
        found in the given path.
        Note that when calling this from an app running in a macOS bundle
        that the path should be prefixed with a call to getResourcePath()

        Note that when concatinating string literals make sure
        to include the utf-8 prefix
        EG: path += u8"/subdir";
        */
        static std::vector<std::string> listDirectories(const std::string& path);
        /*!
        \brief Returns the absolute path of the current working directory
        */
        static std::string getCurrentDirectory();
        /*!
        \brief Sets the current working directory to the given absolute path
        \param path String containing the path to attempt to set to cwd
        \returns false on failure
        */
        static bool setCurrentDirectory(std::string path);

        /*!
        \brief Removes the given directory and recursively removes all content
        Equivalent of rm -rf BE WARNED THIS IS A ONE WAY TRIP
        */
        static void removeDirectory(const std::string& path);

        /*! 
        \brief Attempts to convert the given absolute path to a path relative to the given root directory
        \param path Absolute path to convert
        \param root Absolute path to root directory to which the result should be relative
        */
        static std::string getRelativePath(std::string path, const std::string& root);

        /*!
        \brief Returns a path to the current user's config directory.
        DEPRECATED Prefer cro::App::getPreferencePath() instead.
        Config files should generally be written to this directory, rather than the
        current working directory. Output is usually in the form of the following:
        \code
        Windows: C:\Users\squidward\AppData\Roaming\appname\
        Linux: /home/squidward/.config/appname/
        Mac: /Users/squidward/Library/Application Support/appname/
        \endcode

        WARNING some linux distros are known to return the current working directory
        instead. This should be considered when using configuration files with the same
        name as another which is expected to be stored in a unique directory.

        \param appName Name of the current application used to create the appname directory
        \returns Above formatted string, or an empty string if something went wrong
        */
        [[deprecated("Use cro::App::getPreferencePath() instead")]]
        static std::string getConfigDirectory(const std::string& appName);

        /*!
        \brief Show a native file dialogue to open a file
        \param defaultDir Default path *and file* to open (optional)
        \param filter File extension filter in the format "png,jpg,bmp"
        \param selectMultiple If true then allows selecting multiple files
        \returns path the path selected by the user
        */
        static std::string openFileDialogue(const std::string& defaultDir = "", const std::string& filter = "", bool selectMultiple = false);

        /*!
        \brief Show a native file dialogue to open a folder
        \param defaultPath String containing the default path to browse to
        \returns path the path selected by the user
        */
        static std::string openFolderDialogue(const std::string& path = "");
        
        /*!
        \brief Show a platform native file dialogue for saving files.
        \param defaultDir Default directory *and file* to save to - optional
        \param filter String containing file extension filter in the format "png,jpg,bmp"
        \returns string containing the selected file path
        */
        static std::string saveFileDialogue(const std::string& defaultDir = "", const std::string& filter = "");

        /*!
         \brief Currently only relevant on macOS when creating an app bundle.
         Basically a wrapper around the SFML resourcePath() function.
         \returns path to the resource directory
         \see fileExists()
         \see directoryExists()
         \see listDirectories()
         */
        static std::string getResourcePath();

        /*!
        \brief Sets the resource directory relative to the working directory.
        When using getResourcePath() this path will be appended to the working directory.
        Used, for example, when setting a sub-directory as a resource directory
        */
        static void setResourceDirectory(const std::string& path);

        enum ButtonType
        {
            OK, OKCancel, YesNo, YesNoCancel
        };

        enum IconType
        {
            Info, Warning, Error, Question
        };

        /*!
        \brief Shows a pop up message box to the user.
        Can be used to express an error or ask a question.
        \param title The title string to display in the message box
        \param message The message string to display in the message box
        \param buttonType Button type to display. Defaults to OK
        \param iconType Type of icon to display. Defaults to Info
        \returns true on OK or Yes, false on No or Cancel
        \see ButtonType, IconType
        */
        static bool showMessageBox(const std::string& title, const std::string& message, ButtonType buttonType = ButtonType::OK, IconType iconType = IconType::Info);

        /*!
        \brief Shows a notification in the tray area.
        \param title String containing title
        \param message String containing message to display
        \param iconType Type of icon to display. Defaults to Info
        */
        static void showNotification(const std::string& title, const std::string& message, IconType = IconType::Info);

    private:
        static std::string m_resourceDirectory;
    };
}
