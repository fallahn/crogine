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

#include "../detail/json.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/util/String.hpp>
#include <crogine/graphics/Colour.hpp>

#include <SDL_rwops.h>

#include <sstream>
#include <algorithm>

using namespace cro;
using json = nlohmann::json;

namespace
{
    const std::string indentBlock("    ");
    std::size_t currentLine = 0;

    template <typename T>
    void addToObject(ConfigObject* dst, const std::string& key, json& value)
    {
        T v = value;
        dst->addProperty(key).setValue(v);
    }
}

//--------------------//
ConfigProperty::ConfigProperty(const std::string& name, const std::string& value)
    : ConfigItem(name),
    m_value(value), m_isStringValue(false)
{
    m_isStringValue = !value.empty()
        && ((value.front() == '\"' && value.back() == '\"')
        || value.find(' ') != std::string::npos);
}

void ConfigProperty::setValue(const std::string& value)
{
    m_value = value;
    m_isStringValue = true;
}

void ConfigProperty::setValue(std::int32_t value)
{
    m_value = std::to_string(value);
    m_isStringValue = false;
}

void ConfigProperty::setValue(std::uint32_t value)
{
    m_value = std::to_string(value);
    m_isStringValue = false;
}

void ConfigProperty::setValue(float value)
{
    m_value = std::to_string(value);
    m_isStringValue = false;
}

void ConfigProperty::setValue(bool value)
{
    m_value = (value) ? "true" : "false";
    m_isStringValue = false;
}

void ConfigProperty::setValue(const glm::vec2& v)
{
    m_value = std::to_string(v.x) + "," + std::to_string(v.y);
    m_isStringValue = false;
}

void ConfigProperty::setValue(const glm::vec3& v)
{
    m_value = std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z);
    m_isStringValue = false;
}

void ConfigProperty::setValue(const glm::vec4& v)
{
    m_value = std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "," + std::to_string(v.w);
    m_isStringValue = false;
}

void ConfigProperty::setValue(const cro::FloatRect& r)
{
    m_value = std::to_string(r.left) + "," + std::to_string(r.bottom) + "," + std::to_string(r.width) + "," + std::to_string(r.height);
    m_isStringValue = false;
}

void ConfigProperty::setValue(const cro::Colour& v)
{
    setValue(v.getVec4());
    m_isStringValue = false;
}

std::vector<float> ConfigProperty::valueAsArray() const
{
    std::vector<float> retval;
    std::size_t start = 0u;
    std::size_t next = m_value.find_first_of(',');
    std::size_t end = std::min(m_value.length(), std::size_t(200));

    while (next != std::string::npos && start < end)
    {
        float val;
        std::istringstream is(m_value.substr(start, next));
        if (is >> val) retval.push_back(val);
        else retval.push_back(0.f);

        start = ++next;
        next = m_value.find_first_of(',', start);
        if (next > m_value.length()) next = m_value.length();
    }
    
    //pack out to max supported size
    while (retval.size() < 4)
    {
        retval.push_back(0.f);
    }

    return retval;
}

//-------------------------------------

ConfigObject::ConfigObject(const std::string& name, const std::string& id)
    : ConfigItem    (name), m_id(id){}

bool ConfigObject::loadFromFile(const std::string& filePath, bool relative)
{
    auto path = relative ? FileSystem::getResourcePath() + filePath : filePath;
    currentLine = 0;

    m_id = "";
    setName("");
    m_properties.clear();
    m_objects.clear();

    RaiiRWops rr;
    rr.file = SDL_RWFromFile(path.c_str(), "r");

    if (!rr.file)
    {
        Logger::log(path + " file invalid or not found.", Logger::Type::Warning);
        return false;
    }
    
    //fetch file size
    auto fileSize = SDL_RWsize(rr.file);
    if (fileSize == 0)
    {
        LOG(path + ": file empty", Logger::Type::Warning);
        return false;
    }

    if (rr.file)
    {
        if (cro::FileSystem::getFileExtension(path) == ".json")
        {
            return parseAsJson(rr.file);
        }

        //remove any opening comments
        std::string data;
        std::int64_t readTotal = 0;
        static const std::int32_t DEST_SIZE = 256;
        char dest[DEST_SIZE];
        while (data.empty() && readTotal < fileSize)
        {
            data = std::string(Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal));
            removeComment(data);       
        }
        //check config is not opened with a property
        if (isProperty(data))
        {
            Logger::log(path + ": Cannot start configuration file with a property", Logger::Type::Error);
            return false;
        }
        
        //make sure next line is a brace to ensure we have an object
        std::string lastLine = data;
        data = std::string(Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal));
        removeComment(data);

        //tracks brace balance
        std::vector<ConfigObject*> objStack;

        if (data[0] == '{')
        {
            //we have our opening object
            auto objectName = getObjectName(lastLine);
            setName(objectName.first);
            m_id = objectName.second;
            
            objStack.push_back(this);
        }
        else
        {
            Logger::log(path + " Invalid configuration header (missing '{' ?)", Logger::Type::Error);
            return false;
        }


        while (readTotal < fileSize)
        {
            data = std::string(Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal));
            removeComment(data);
            if (!data.empty())
            {
                if (data[0] == '}')
                {
                    //close current object and move to parent
                    objStack.pop_back();
                }
                else if (isProperty(data))
                {           
                    //insert name / value property into current object
                    auto prop = getPropertyName(data);
                    //TODO need to reinstate this and create a property
                    //capable of storing arrays
                    /*if (currentObject->findProperty(prop.first))
                    {
                        Logger::log("Property \'" + prop.first + "\' already exists in \'" + currentObject->getName() + "\', skipping entry...", Logger::Type::Warning);
                        continue;
                    }*/

                    if (prop.second.empty())
                    {
                        Logger::log("\'" + objStack.back()->getName() + "\' property \'" + prop.first + "\' has no valid value", Logger::Type::Warning);
                        continue;
                    }
                    objStack.back()->addProperty(prop.first, prop.second);
                }
                else
                {
                    //add a new object and make it current
                    std::string prevLine = data;
                    data = std::string(Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal));
                    removeComment(data);
                    if (data[0] == '{')
                    {
                        //TODO we have to allow mutliple objects with the same name in this instance
                        //as a model may have multiple material defs.
                        auto name = getObjectName(prevLine);
                        //if (name.second.empty() || objStack.back()->findObjectWithId(name.second) == nullptr)
                        //{
                        //    //safe to add new object as the name doesn't exist
                        //    objStack.push_back(objStack.back()->addObject(name.first, name.second));
                        //}
                        //else
                        //{
                        //    Logger::log("Object with ID " + name.second + " has already been added, duplicate is skipped...", Logger::Type::Warning);

                        //    //fast forward to closing brace
                        //    while (data[0] != '}')
                        //    {
                        //        data = std::string(Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal));
                        //        removeComment(data);
                        //    }
                        //}

                        objStack.push_back(objStack.back()->addObject(name.first, name.second));
                    }
                    else //last line was probably garbage or nothing but spaces
                    {
                        //LogW << filePath << ": Missing line or incorrect syntax at " << currentLine << std::endl;
                        continue;
                    }
                }
            }       
        }

        if (!objStack.empty())
        {
            Logger::log("Brace count not at 0 after parsing \'" + path + "\'. Config data may not be correct.", Logger::Type::Warning);
        }
        return true;
    }
    
    Logger::log(filePath + " file invalid or not found.", Logger::Type::Error);
    return false;
}

const std::string& ConfigObject::getId() const
{
    return m_id;
}

ConfigProperty* ConfigObject::findProperty(const std::string& name) const
{
    auto result = std::find_if(m_properties.begin(), m_properties.end(),
        [&name](const ConfigProperty& p)
    {
        return (p.getName() == name);
    });

    if (result != m_properties.end())
    {
        return const_cast<ConfigProperty*>(&*result);
    }
    //recurse
    for (auto& o : m_objects)
    {
        auto p = o.findProperty(name);
        if (p) return p;
    }

    return nullptr;
}

ConfigObject* ConfigObject::findObjectWithId(const std::string& id) const
{
    if (id.empty())
    {
        return nullptr;
    }

    auto result = std::find_if(m_objects.begin(), m_objects.end(),
        [&id](const ConfigObject& p)
    {
        auto s = p.getId();
        return (!s.empty() && s == id);
    });

    if (result != m_objects.end())
    {
        return const_cast<ConfigObject*>(&*result);
    }
    
    //recurse
    for (auto& o : m_objects)
    {
        auto p = o.findObjectWithId(id);
        if (p) return p;
    }

    return nullptr;
}

ConfigObject* ConfigObject::findObjectWithName(const std::string& name) const
{
    auto result = std::find_if(m_objects.begin(), m_objects.end(),
        [&name](const ConfigObject& p)
    {
        return (p.getName() == name);
    });

    if (result != m_objects.end())
    {
        return const_cast<ConfigObject*>(&*result);
    }

    //recurse
    for (auto& o : m_objects)
    {
        auto p = o.findObjectWithName(name);
        if (p) return p;
    }

    return nullptr;
}

const std::vector<ConfigProperty>& ConfigObject::getProperties() const
{
    return m_properties;
}

std::vector<ConfigProperty>& ConfigObject::getProperties()
{
    return m_properties;
}

const std::vector<ConfigObject>& ConfigObject::getObjects() const
{
    return m_objects;
}

std::vector<ConfigObject>& ConfigObject::getObjects()
{
    return m_objects;
}

ConfigProperty& ConfigObject::addProperty(const std::string& name, const std::string& value)
{
    m_properties.emplace_back(name, value);
    m_properties.back().setParent(this);
    return m_properties.back();
}

void ConfigObject::addProperty(const ConfigProperty& prop)
{
    m_properties.push_back(prop);
}

ConfigObject* ConfigObject::addObject(const std::string& name, const std::string& id)
{
    m_objects.emplace_back(name, id);
    m_objects.back().setParent(this);
    //return a reference to newly added object so we can start adding properties / objects to it
    return &m_objects.back();
}

void ConfigObject::removeProperty(const std::string& name)
{
    auto result = std::find_if(m_properties.begin(), m_properties.end(),
        [&name](const ConfigProperty& p)
    {
        return (p.getName() == name);
    });

    if (result != m_properties.end()) m_properties.erase(result);
}

ConfigObject ConfigObject::removeObject(const std::string& name)
{
    auto result = std::find_if(m_objects.begin(), m_objects.end(),
        [&name](const ConfigObject& p)
    {
        return (p.getName() == name);
    });

    if (result != m_objects.end())
    {
        auto p = std::move(*result);
        p.setParent(nullptr);
        m_objects.erase(result);
        return p;
    }

    return {};
}

ConfigObject::NameValue ConfigObject::getObjectName(const std::string& line)
{
    auto result = line.find_first_of(' ');
    if (result != std::string::npos)
    {
        std::string first = line.substr(0, result);
        std::string second = line.substr(result + 1);
        //make sure id has no spaces by truncating it
        result = second.find_first_of(' ');
        if (result != std::string::npos)
            second = second.substr(0, result);

        return std::make_pair(first, second);
    }
    return std::make_pair(line, "");
}

ConfigObject::NameValue ConfigObject::getPropertyName(const std::string& line)
{
    auto result = line.find_first_of("=");
    assert(result != std::string::npos);

    std::string first = line.substr(0, result);
    Util::String::removeChar(first, ' ');

    std::string second = line.substr(result + 1);
    
    //check for string literal
    result = second.find_first_of("\"");
    if (result != std::string::npos)
    {
        auto otherResult = second.find_last_of("\"");
        if (otherResult != std::string::npos
            && otherResult != result)
        {
            second = second.substr(result, otherResult);
            Util::String::removeChar(second, '\"');
            if (second[0] == '/') second = second.substr(1);
        }
        else
        {
            Logger::log("String property \'" + first + "\' has missing \'\"\', value may not be as expected", Logger::Type::Warning);
        }
    }
    else
    {
        Util::String::removeChar(second, ' ');
    }

    return std::make_pair(first, second);
}

bool ConfigObject::isProperty(const std::string& line)
{
    auto pos = line.find('=');
    return(pos != std::string::npos && pos > 1 && line.length() > 5);
}

void ConfigObject::removeComment(std::string& line)
{
    auto result = line.find_first_of("//");

    //make sure to only crop comments outside of string literals
    //for some reason it appears result also matches '/' so unquoted paths get truncated
    if (result < line.size() - 1 && line[result + 1] == '/')
    {
        auto otherResult = line.find_last_of('\"');
        if (result != std::string::npos && (result > otherResult || otherResult == std::string::npos))
        {
            line = line.substr(0, result);
        }
    }

    //remove any tabs while we're at it
    Util::String::removeChar(line, '\t');
    //Util::String::removeChar(line, ' ');

    //and preceding spaces
    auto start = line.find_first_not_of(' ');
    if (start != std::string::npos)
    {
        line = line.substr(start);
    }

    if (line.find(';') != std::string::npos)
    {
        LogW << "Line " << currentLine << " contains semi-colon, is this intentional?" << std::endl;
    }
    currentLine++;
}

bool ConfigObject::parseAsJson(SDL_RWops* file)
{
    json j;

    {
        auto fileSize = SDL_RWsize(file);
        std::vector<char> jsonData(fileSize);
        SDL_RWread(file, jsonData.data(), fileSize, 1);

        if (jsonData.empty())
        {
            LogE << "no data was read from json file" << std::endl;
            return false;
        }
        jsonData.push_back(0);

        try 
        {
            j = json::parse(jsonData.data());
        }
        catch (...)
        {
            LogE << "Failed parsing json string" << std::endl;
            return false;
        }
    }

    std::vector<ConfigObject*> objStack;
    objStack.push_back(this);

    std::function<void(const decltype(j.items())&)> parseItemList = 
        [&](const decltype(j.items())& items)
    {
        for (auto& [key, value] : items)
        {
            if (value.is_object())
            {
                objStack.push_back(objStack.back()->addObject(key));
                parseItemList(value.items());
                objStack.pop_back();
            }
            else
            {
                //parse regular key/val
                if (value.is_string())
                {
                    addToObject<std::string>(objStack.back(), key, value);
                }
                else if (value.is_number())
                {
                    if (value.is_number_float())
                    {
                        addToObject<float>(objStack.back(), key, value);
                    }
                    else if (value.is_number_unsigned())
                    {
                        addToObject<std::uint32_t>(objStack.back(), key, value);
                    }
                    else
                    {
                        addToObject<std::int32_t>(objStack.back(), key, value);
                    }
                }
                else if (value.is_boolean())
                {
                    addToObject<bool>(objStack.back(), key, value);
                }
                else if (value.is_array())
                {
                    std::vector<float> numVals;

                    for (auto& [arKey, arVal] : value.items())
                    {
                        //only accept numeric arrays which represent vec2/3/4/colour/rect
                        if (arVal.is_number())
                        {
                            numVals.push_back(arVal);
                        }
                        else if(arVal.is_object())
                        {
                            //else parse each into its own object
                            objStack.push_back(objStack.back()->addObject(key));
                            parseItemList(arVal.items());
                            objStack.pop_back();
                        }
                        else
                        {
                            LogW << "Arrays of " << arVal.type_name() << " are not supported." << std::endl;
                        }
                    }

                    if (!numVals.empty())
                    {
                        switch (numVals.size())
                        {
                        case 1:
                            objStack.back()->addProperty(key).setValue(numVals[0]);
                            break;
                        case 2:
                            objStack.back()->addProperty(key).setValue(glm::vec2(numVals[0], numVals[1]));
                            break;
                        case 3:
                            objStack.back()->addProperty(key).setValue(glm::vec3(numVals[0], numVals[1], numVals[2]));
                            break;
                        default:
                            objStack.back()->addProperty(key).setValue(glm::vec4(numVals[0], numVals[1], numVals[2], numVals[3]));
                            break;
                        }
                    }
                }
                else
                {
                    LogW << key << ": was skipped. " << value.type_name() << " is not a supported type." << std::endl;
                }
            }
        }
    };

    parseItemList(j.items());
    return true;
}

bool ConfigObject::save(const std::string& path)
{
    RaiiRWops out;
    out.file = SDL_RWFromFile(path.c_str(), "w");
    if (out.file)
    {
        auto written = write(out.file);
        Logger::log("Wrote " + std::to_string(written) + " bytes to " + path, Logger::Type::Info);
        return true;
    }

    Logger::log("failed to write configuration to: \'" + path + "\'", Logger::Type::Error);
    return false;
}

std::size_t ConfigObject::write(SDL_RWops* file, std::uint16_t depth)
{
    //add the correct amount of indenting based on this objects's depth
    std::string indent;
    for (auto i = 0u; i < depth; ++i)
    {
        indent += indentBlock;
    }

    std::stringstream stream;
    stream.precision(3);
    stream << indent << getName() << " " << getId() << std::endl;
    stream << indent << "{" << std::endl;
    for (const auto& p : m_properties)
    {
        auto str = p.getValue<std::string>();
        if (str.empty())
        {
            LogW << p.getName() << ": property has no value and will be skipped..." << std::endl;
        }
        else
        {
            stream << indent << indentBlock << p.getName() << " = ";
            if (!p.m_isStringValue
                || (str.front() == '\"' && str.back() == '\"'))
            {
                stream << std::fixed << str;
            }
            else
            {
                stream << "\"" << str << "\"";
            }

            stream << std::endl;
        }
    }
    stream << "\n";

    std::size_t written = 0;
    std::string str = stream.str();
    written += SDL_RWwrite(file, str.data(), sizeof(char), str.size());

    for (auto& o : m_objects)
    {
        written += o.write(file, depth + 1);
    }

    stream = std::stringstream();
    stream << indent << "}" << std::endl;
    str = stream.str();
    written += SDL_RWwrite(file, str.data(), sizeof(char), str.size());

    return written;
}

//--------------------//
ConfigItem::ConfigItem(const std::string& name)
    : m_parent  (nullptr),
    m_name      (name){}

ConfigItem* ConfigItem::getParent() const
{
    return m_parent;
}

const std::string& ConfigItem::getName() const
{
    return m_name;
}

void ConfigItem::setParent(ConfigItem* parent)
{
    m_parent = parent;
}

void ConfigItem::setName(const std::string& name)
{
    m_name = name;
}
