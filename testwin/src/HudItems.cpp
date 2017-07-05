/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "HudItems.hpp"
#include "Messages.hpp"

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
    cro::Colour interp(cro::Colour a, cro::Colour b, float amount)
    {
        return cro::Colour(
            (b.getRed() - a.getRed()) * amount + a.getRed(),
            (b.getGreen() - a.getGreen()) * amount + a.getGreen(),
            (b.getBlue() - a.getBlue()) * amount + a.getBlue(),
            (b.getAlpha() - a.getAlpha()) * amount + a.getAlpha()
        );
    }

    //cro::Colour timerColourA(1.f, 1.f, 1.f, 0.7f);
    //cro::Colour timerColourB(0.f, 1.f, 1.f, 0.7f);
}

HudSystem::HudSystem(cro::MessageBus& mb)
    : cro::System       (mb, typeid(HudSystem)),
    m_playerHealth      (100.f),
    m_weaponTime        (0.f),
    m_score             (0)
{
    //requireComponent<cro::Sprite>();
    requireComponent<cro::Transform>();
    requireComponent<HudItem>();
}

//public
void HudSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        switch (data.type)
        {
        default: break;
        case PlayerEvent::HealthChanged:
            m_playerHealth = data.value;
            break;
        case PlayerEvent::Score:
            m_score = static_cast<cro::int32>(data.value);
            break;
        }
    }
    else if (msg.id == MessageID::WeaponMessage)
    {
        const auto& data = msg.getData<WeaponEvent>();
        m_weaponTime = data.downgradeTime;
    }
}

void HudSystem::process(cro::Time dt)
{
    float dtSec = dt.asSeconds();

    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& hudItem = entity.getComponent<HudItem>();
        switch (hudItem.type)
        {
        default: break;
        case HudItem::Type::HealthBar:
        {
            hudItem.value -= (hudItem.value - m_playerHealth) * dtSec;
            float amount = cro::Util::Maths::clamp(hudItem.value / 100.f, 0.f, 1.f);
            entity.getComponent<cro::Transform>().setScale({ amount, 1.f, 1.f });
            entity.getComponent<cro::Sprite>().setColour(interp(cro::Colour(1.f, 0.f, 0.f, 0.1f), cro::Colour::Cyan(), amount));
        }
        break;
        case HudItem::Type::Timer:
            hudItem.value += (m_weaponTime - hudItem.value) * dtSec * 4.f;
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_time", hudItem.value);
            break;

        case HudItem::Type::Score:
            entity.getComponent<cro::Text>().setString(std::to_string(m_score));
            break;
        }
    }
}

//private
void HudSystem::onEntityAdded(cro::Entity entity)
{
    if (entity.getComponent<HudItem>().type == HudItem::Type::Timer)
    {
        entity.getComponent<HudItem>().value = 0.f;
    }
}