/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

template <typename T, typename... Args>
T* Scene::addSystem(Args&&... args)
{
//#ifdef CRO_DEBUG_
    if constexpr (std::is_same<T, CameraSystem>::value)
    {
        if (!m_renderables.empty())
        {
            LogW << "Adding CameraSystem after a Renderable system type... this may cause undesired effects." << std::endl;
        }
    }
//#endif

    static_assert(std::is_base_of<System, T>::value, "Must be a system type");
    auto& system = m_systemManager.addSystem<T>(std::forward<Args>(args)...);
    if constexpr (std::is_base_of<Renderable, T>::value)
    {
        m_renderables.push_back(static_cast<Renderable*>(&system));
    }
    return &system;
}

template <typename T>
T* Scene::getSystem()
{
    return m_systemManager.getSystem<T>();
}

template <typename T>
const T* Scene::getSystem() const
{
    return m_systemManager.getSystem<T>();
}

template <typename T>
void Scene::setSystemActive(bool active)
{
    m_systemManager.setSystemActive<T>(active);
}

template <typename T, typename... Args>
T* Scene::addDirector(Args&&... args)
{
    static_assert(std::is_base_of<Director, T>::value, "Must be a director type");
    m_directors.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));

    //add a command system if it doesn't exist
    if (!m_systemManager.hasSystem<CommandSystem>())
    {
        addSystem<CommandSystem>(m_messageBus);
    }

    m_directors.back()->m_commandSystem = m_systemManager.getSystem<CommandSystem>();
    m_directors.back()->m_messageBus = &m_messageBus;
    m_directors.back()->m_scene = this;

    return static_cast<T*>(m_directors.back().get());
}

template <typename T>
T* Scene::getDirector()
{
    auto result = std::find_if(m_directors.begin(), m_directors.end(),
        [](const std::unique_ptr<Director>& ptr)
        {
            return typeid(*ptr.get()) == typeid(T);
        });

    if (result == m_directors.end())
    {
        return nullptr;
    }
    return static_cast<T*>(result->get());
}

template <typename T, typename... Args>
T& Scene::addPostProcess(Args&&... args)
{
    static_assert(std::is_base_of<PostProcess, T>::value, "Must be a post process type");
    auto size = App::getWindow().getSize();
    if (!m_sceneBuffer.available())
    {
        if (m_sceneBuffer.create(size.x, size.y))
        {
            //set render path
            using namespace std::placeholders;
            currentRenderPath = std::bind(&Scene::postRenderPath, this, _1, _2, _3);
        }
        else
        {
            Logger::log("Failed settings scene render buffer - post process is disabled", Logger::Type::Error, Logger::Output::All);
        }
    }
    m_postEffects.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
    m_postEffects.back()->resizeBuffer(size.x, size.y);

    //create intermediate buffers if needed
    switch (m_postEffects.size())
    {
    case 2:
        m_postBuffers[0].create(size.x, size.y, false);
        break;
    case 3:
        m_postBuffers[1].create(size.x, size.y, false);
        break;
    default: break;
    }

    return *dynamic_cast<T*>(m_postEffects.back().get());
}

template <typename T>
T* Scene::postMessage(cro::Message::ID id)
{
    return m_messageBus.post<T>(id);
}