/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include <cstdint>

namespace ui
{
    static const float TitleHeight = 22.f;
    static const float InspectorWidth = 0.25f; //percent of window width
    static const float BrowserHeight = 0.32f;
    static const float ThumbnailHeight = 0.5f; //percent of browser height
    static const float MaterialPreviewWidth = 0.45f;
    static const float TexturePreviewWidth = 0.65f;
    static const float TextBoxWidth = 0.915f;

    static const std::size_t MaxMaterials = 99;
}