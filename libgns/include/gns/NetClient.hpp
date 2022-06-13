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

#include <gns/Config.hpp>

/*
Note that in the open source version each client connection will be calling RunCallbacks() when
the event queue is polled. This is fine for a single connection (the general use case) but
multiple connections will call RunCallbacks() multiple times per frame. Although this is not
the case when using steamworks, not that in all cases pollEvent *doesn't* return events specific
to one connection, rather it returns all events. Again not a problem with a single connection
but if multiple connections really are needed then perhaps pollEvent() should be made static and
used only once per frame.
*/
