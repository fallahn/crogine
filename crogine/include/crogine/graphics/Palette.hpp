/*-----------------------------------------------------------------------

Matt Marchant 2022
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
#include <crogine/core/String.hpp>
#include <crogine/graphics/Colour.hpp>

#include <string>
#include <vector>

namespace cro
{
    /*!
    \brief A single entry into a palette
    */
    struct CRO_EXPORT_API Swatch final
    {
        cro::String name;
        std::vector<cro::Colour> colours;
    };

    /*!
    \brief A collection of swatches loaded from an Adobe ASE file
    */
    class CRO_EXPORT_API Palette final
    {
    public:
        /*!
        \brief Default constructor.
        Creates an empty palette
        */
        Palette();

        /*!
        \brief Cosntructor
        \param path Path to ase file to attempt to load
        Constructs a palette and immediately tries to load the ase
        file at the given path. If loading fails an error is logged.
        */
        explicit Palette(const std::string& path);

        /*!
        \brief Attempts to load an ase file from the given path
        \param path Path to the palette file to open
        \param append If true new swatches are appended to the
        existing palette if loading is successful, otherwise all
        existing swatches are cleared. Note that in these cases if
        loading fails the existing palette will be cleared and
        the swatches will now be empty.
        \returns true on success else returns false.
        */
        bool loadFromFile(const std::string& path, bool append = false);

        const std::vector<Swatch>& getSwatches() const { return m_swatches; }

    private:
        std::vector<Swatch> m_swatches;
    };
}
