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

#include "../detail/json.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/util/String.hpp>
#include <crogine/graphics/Colour.hpp>

#include <SDL_rwops.h>

#include <sstream>
#include <fstream>
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
#ifdef OLD_PARSER
    m_value(value), m_isStringValue(false)
{
    m_isStringValue = !value.empty()
        && ((value.front() == '\"' && value.back() == '\"')
        || value.find(' ') != std::string::npos);
}
#else
    m_boolValue(false)
{
    if (!value.empty())
    {
        setValue(value);
    }
}
#endif

void ConfigProperty::setValue(const std::string& value)
{
#ifdef OLD_PARSER
    m_value = value;
    m_isStringValue = true;
#else
    //new version
    resetValue();
    auto& v = m_utf8Values.emplace_back();
    v.resize(value.size());
    std::memcpy(v.data(), value.data(), value.size());
#endif
}
void ConfigProperty::setValue(const cro::String& value)
{
    auto utf = value.toUtf8();
#ifdef OLD_PARSER
    m_value = std::string(utf.begin(), utf.end());
    m_isStringValue = true;
#else
    //new version
    resetValue();
    m_utf8Values.emplace_back(std::move(utf));
#endif
}

void ConfigProperty::setValue(std::int32_t value)
{
#ifdef OLD_PARSER
    m_value = std::to_string(value);
    m_isStringValue = false;
#else
    //new version
    resetValue();
    m_floatValues.emplace_back(static_cast<double>(value));
#endif
}

void ConfigProperty::setValue(std::uint32_t value)
{
#ifdef OLD_PARSER
    m_value = std::to_string(value);
    m_isStringValue = false;
#else
    //new version
    resetValue();
    m_floatValues.emplace_back(static_cast<double>(value));
#endif
}

void ConfigProperty::setValue(float value)
{
#ifdef OLD_PARSER
    m_value = std::to_string(value);
    m_isStringValue = false;
#else
    //new version
    resetValue();
    m_floatValues.push_back(value);
#endif
}

void ConfigProperty::setValue(bool value)
{
#ifdef OLD_PARSER
    m_value = (value) ? "true" : "false";
    m_isStringValue = false;
#else
    //new version
    resetValue();
    m_boolValue = value;
#endif
}

void ConfigProperty::setValue(const glm::vec2& v)
{
#ifdef OLD_PARSER
    m_value = std::to_string(v.x) + "," + std::to_string(v.y);
    m_isStringValue = false;
#else
    //new version
    resetValue();
    m_floatValues.push_back(v.x);
    m_floatValues.push_back(v.y);
#endif
}

void ConfigProperty::setValue(const glm::vec3& v)
{
#ifdef OLD_PARSER
    m_value = std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z);
    m_isStringValue = false;
#else
    //new version
    resetValue();
    m_floatValues.push_back(v.x);
    m_floatValues.push_back(v.y);
    m_floatValues.push_back(v.z);
#endif
}

void ConfigProperty::setValue(const glm::vec4& v)
{
#ifdef OLD_PARSER
    m_value = std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "," + std::to_string(v.w);
    m_isStringValue = false;
#else
    //new version
    resetValue();
    m_floatValues.push_back(v.x);
    m_floatValues.push_back(v.y);
    m_floatValues.push_back(v.z);
    m_floatValues.push_back(v.w);
#endif
}

void ConfigProperty::setValue(const FloatRect& r)
{
#ifdef OLD_PARSER
    m_value = std::to_string(r.left) + "," + std::to_string(r.bottom) + "," + std::to_string(r.width) + "," + std::to_string(r.height);
    m_isStringValue = false;
#else
    //new version
    resetValue();
    m_floatValues.push_back(r.left);
    m_floatValues.push_back(r.bottom);
    m_floatValues.push_back(r.width);
    m_floatValues.push_back(r.height);
#endif
}

void ConfigProperty::setValue(const Colour& v)
{
    setValue(v.getVec4());
}

#ifndef OLD_PARSER
void ConfigProperty::resetValue()
{
    m_utf8Values.clear();
    m_floatValues.clear();
    m_boolValue = false;
}
#else

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
#endif

//-------------------------------------

ConfigObject::ConfigObject(const std::string& name, const std::string& id)
    : ConfigItem    (name)
{
    //use this rather than the init list as
    //it performs some sanity checks on the string
    setId(id);
}

bool ConfigObject::loadFromFile(const std::string& filePath, bool relative)
{
    currentLine = 0; //well this has code smell to it...

    m_id = "";
    setName("");
    m_properties.clear();
    m_objects.clear();


#ifdef OLD_PARSER
    return loadFromFile1(relative ? FileSystem::getResourcePath() + filePath : filePath);
#else
    return loadFromFile2(relative ? FileSystem::getResourcePath() + filePath : filePath);
#endif
}

const std::string& ConfigObject::getId() const
{
    return m_id;
}

void ConfigObject::setId(const std::string& id)
{
    m_id = id;

    //replace spaces with underscore and remove non-alphanum
    Util::String::replace(m_id, " ", "_");

    m_id.erase(std::remove_if(m_id.begin(), m_id.end(),
        [](char c)
        {
            return !std::isalnum(c) && c != '_';
        }), m_id.end());
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
    //cro::String cLine = cro::String::fromUtf8(line.begin(), line.end());
    //auto result = cLine.find("=");
    //auto firstArr = cLine.substr(0, result).toUtf8();
    //firstArr.erase(std::remove(firstArr.begin(), firstArr.end(), ' '), firstArr.end());
    //const std::string first = reinterpret_cast<const char*>(firstArr.c_str());

    //auto secondArr = cLine.substr(result + 1).toUtf8();
    //auto r1 = std::find(secondArr.begin(), secondArr.end(), '\"');
    //if (r1 != secondArr.end())
    //{
    //    auto r2 = std::find(secondArr.rbegin(), secondArr.rend(), '\"');
    //    if (r2 != secondArr.rend())
    //    {
    //        /*auto r2Pos = secondArr.rend() - r2 - 1;
    //        std::basic_string<std::uint8_t> temp;
    //        temp.resize(r2Pos - std::distance(secondArr.begin(), r1));
    //        std::copy(r1, r1 + temp.size(), temp.begin());
    //        secondArr.swap(temp);*/
    //        secondArr.erase(std::remove(secondArr.begin(), secondArr.end(), '\"'), secondArr.end());

    //    }
    //    else
    //    {
    //        Logger::log("String property \'" + first + "\' has missing \'\"\', value may not be as expected", Logger::Type::Warning);
    //    }
    //}
    //else
    //{
    //    secondArr.erase(std::remove(secondArr.begin(), secondArr.end(), ' '), secondArr.end());
    //}
    //std::string second;
    //if (secondArr[0] == '/')
    //{
    //    second = reinterpret_cast<const char*>(&secondArr[1]);
    //}
    //else
    //{
    //    second = reinterpret_cast<const char*>(secondArr.c_str());
    //}


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
#ifdef OLD_PARSER
    out.file = SDL_RWFromFile(path.c_str(), "w");
#else
    out.file = SDL_RWFromFile(path.c_str(), "wb");
#endif

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

    std::size_t written = 0;

#ifdef OLD_PARSER
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
#else
    //write object header
    constexpr char space = ' ';
    constexpr char openBracket[] = "{\n";
    constexpr char closeBracket[] = "}\n";
    constexpr char newline = '\n';

    written += SDL_RWwrite(file, indent.data(), 1, indent.size());
    written += SDL_RWwrite(file, getName().data(), 1, getName().size());
    written += SDL_RWwrite(file, &space, 1, 1);
    written += SDL_RWwrite(file, getId().data(), 1, getId().size());
    written += SDL_RWwrite(file, &newline, 1, 1);
    written += SDL_RWwrite(file, indent.data(), 1, indent.size());
    written += SDL_RWwrite(file, openBracket, 1, sizeof(openBracket) - 1);

    constexpr char trueString[] = "true";
    constexpr char falseString[] = "false";
    constexpr char equals[] = " = ";
    constexpr char separator[] = ", ";
    constexpr char quote = '"';
    
    //write out properties
    for (const auto& p : m_properties)
    {
        written += SDL_RWwrite(file, indent.data(), 1, indent.size());
        written += SDL_RWwrite(file, indentBlock.data(), 1, indentBlock.size());

        written += SDL_RWwrite(file, p.getName().data(), 1, p.getName().size());
        written += SDL_RWwrite(file, equals, 1, sizeof(equals) - 1);

        if (!p.m_utf8Values.empty())
        {
            for (auto i = 0u; i < p.m_utf8Values.size(); ++i)
            {
                const auto& v = p.m_utf8Values[i];

                //always write enclosing quotes
                written += SDL_RWwrite(file, &quote, 1, 1);
                written += SDL_RWwrite(file, v.data(), 1, v.size());
                written += SDL_RWwrite(file, &quote, 1, 1);

                if (i < p.m_utf8Values.size() - 1)
                {
                    written += SDL_RWwrite(file, separator, 1, sizeof(separator) - 1);
                }
            }
        }
        else if (!p.m_floatValues.empty())
        {
            for (auto i = 0u; i < p.m_floatValues.size(); ++i)
            {
                const std::string str = std::to_string(p.m_floatValues[i]);
                written += SDL_RWwrite(file, str.data(), 1, str.size());

                if (i < p.m_floatValues.size() - 1)
                {
                    written += SDL_RWwrite(file, separator, 1, sizeof(separator) - 1);
                }
            }
        }
        else
        {
            //fall back to the bool value
            if (p.m_boolValue)
            {
                written += SDL_RWwrite(file, trueString, 1, sizeof(trueString) - 1);
            }
            else
            {
                written += SDL_RWwrite(file, falseString, 1, sizeof(falseString) - 1);
            }
        }

        written += SDL_RWwrite(file, &newline, 1, 1);
    }

    //write nested objects
    for (auto& o : m_objects)
    {
        written += SDL_RWwrite(file, &newline, 1, 1);
        written += o.write(file, depth + 1);
    }

    //close current object
    written += SDL_RWwrite(file, indent.data(), 1, indent.size());
    written += SDL_RWwrite(file, closeBracket, 1, sizeof(closeBracket) - 1);
#endif
    return written;
}

#ifdef OLD_PARSER
bool ConfigObject::loadFromFile1(const std::string& path)
{
    RaiiRWops rr;
    rr.file = SDL_RWFromFile(path.c_str(), "r");

    if (!rr.file)
    {
        Logger::log(path + " file invalid or not found.", Logger::Type::Warning);
        return false;
    }

    //fetch file size
    const auto fileSize = SDL_RWsize(rr.file);
    if (fileSize == 0)
    {
        LOG(path + ": file empty", Logger::Type::Warning);
        return false;
    }

    if (rr.file)
    {
        if (FileSystem::getFileExtension(path) == ".json")
        {
            return parseAsJson(rr.file);
        }

        //remove any opening comments
        std::string data;
        char* temp = nullptr;
        std::int64_t readTotal = 0;
        static constexpr std::int32_t DEST_SIZE = 1024;// 256;
        char dest[DEST_SIZE];
        while (data.empty() && readTotal < fileSize)
        {
            temp = Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal);

            if (temp)
            {
                data = std::string(temp);
                removeComment(data);
            }
            else
            {
                LogE << path << ": unexpected EOF" << std::endl;
                return false;
            }
        }
        //check config is not opened with a property
        if (isProperty(data))
        {
            Logger::log(path + ": Cannot start configuration file with a property", Logger::Type::Error);
            return false;
        }

        //make sure next line is a brace to ensure we have an object
        std::string lastLine = data;

        temp = Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal);
        if (temp)
        {
            data = std::string(temp);
            removeComment(data);
        }
        else
        {
            LogE << path << ": unexpected EOF" << std::endl;
            return false;
        }

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
            temp = Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal);
            if (temp)
            {
                data = std::string(temp);
                removeComment(data);
            }
            else
            {
                LogE << path << ": unexpected EOF" << std::endl;
                return false;
            }

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

                    temp = Util::String::rwgets(dest, DEST_SIZE, rr.file, &readTotal);
                    if (temp)
                    {
                        data = std::string(temp);
                        removeComment(data);
                    }
                    else
                    {
                        LogE << path << ": unexpected EOF" << std::endl;
                        return false;
                    }

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
    return false;
}
#else
bool ConfigObject::loadFromFile2(const std::string& path)
{
    RaiiRWops rr;
    rr.file = SDL_RWFromFile(path.c_str(), "rb");

    if (!rr.file)
    {
        Logger::log(path + " file invalid or not found.", Logger::Type::Warning);
        return false;
    }

    //fetch file size
    const auto fileSize = SDL_RWsize(rr.file);
    if (fileSize < 1)
    {
        LOG(path + ": file empty", Logger::Type::Warning);
        return false;
    }

    if (rr.file)
    {
        if (FileSystem::getFileExtension(path) == ".json")
        {
            return parseAsJson(rr.file);
        }

        std::int64_t readCount = 0;

        std::vector<ConfigObject*> objectStack;

        std::basic_string<std::uint8_t> currentLine;
        std::uint8_t currentByte = 0;

        std::int32_t lineNumber = 1;
        std::string objectName;
        std::string objectID;

        while (readCount != fileSize)
        {
            readCount += SDL_RWread(rr.file, &currentByte, 1, 1);
            if (currentByte != '\n')
            {
                currentLine.push_back(currentByte);
            }
            else
            {
                //line is complete, so parse it
                std::size_t lineStart = 0;
                while (currentLine[lineStart] == ' ' || currentLine[lineStart] == '\t')
                {
                    //skip indentation
                    lineStart++;
                }

                //split into tokens
                std::vector<std::basic_string<std::uint8_t>> tokens;
                //std::vector<std::vector<char>> tokens; //use this for debugging as it makes strings visible in the debugger
                tokens.emplace_back();
                auto tokenIndex = 0;

                bool stringOpen = false; //tracks if the token is part of a string value
                bool foundProperty = false; //once we found an assignment allow spaces in property values, eg float arrays

                for (auto i = lineStart; i < currentLine.size(); ++i)
                {

                    //if this is a comment then quit here
                    if (!stringOpen
                        && currentLine[i] == '/'
                        && i < currentLine.size() - 1
                        && currentLine[i + 1] == '/')
                    {
                        break;
                    }

                    //if we hit a space start a new token
                    //as long as it's not part of a string literal
                    //or property array value
                    if (!stringOpen &&
                        !foundProperty &&
                        currentLine[i] == ' ')
                    {
                        tokens.emplace_back();
                        tokenIndex++;
                    }

                    //if we hit an assignment and current token not empty
                    //start a new token because the spaces are missing
                    else if (!stringOpen &&
                        currentLine[i] == '=')
                    {
                        if (!tokens[tokenIndex].empty())
                        {
                            //there was no preceding space to start a new one
                            tokens.emplace_back();
                            tokenIndex++;
                        }

                        //store the assignment in its own token so we know
                        //we have a property, and then create a new token
                        //skipping possible space
                        tokens[tokenIndex].push_back(currentLine[i]);

                        if (i < currentLine.size() - 1
                            /*&& currentLine[i + 1] != ' '*/)
                        {
                            tokens.emplace_back();
                            tokenIndex++;

                            if (currentLine[i + 1] == ' ')
                            {
                                i++;
                            }
                        }

                        foundProperty = true;
                    }


                    //check to see if we open or close a string
                    else if (currentLine[i] == '"')
                    {
                        stringOpen = !stringOpen;
                        
                        //we need to store these so we can identify the value as a string
                        tokens[tokenIndex].push_back(currentLine[i]);
                    }

                    //else push back current char
                    else
                    {
                        //make sure this isn't whitespace like \t or \r
                        if (currentLine[i] != '\t'
                            && currentLine[i] != '\r')
                        {
                            tokens[tokenIndex].push_back(currentLine[i]);
                        }
                    }
                }

                //if there was any white space at the end of a line we'll have created
                //empty tokens, which throws off the token count, so remove them
                tokens.erase(std::remove_if(tokens.begin(), tokens.end(), 
                    [](const std::basic_string<std::uint8_t>& s)
                    {
                        return s.empty();
                    }), tokens.end());


                //examine our list of tokens and decide what to do with them
                //we may have an array here where spaces were placed between
                //components... or we may have single/mixed tokens with comma
                //separated values.....
                if (tokens.size() < 3)
                {
                    //this is an object name/id pair
                    //or an opening/closing brace
                    if (!tokens[0].empty())
                    {
                        //hm, empty lines create an empty token...

                        if (tokens[0][0] == '{')
                        {
                            //this is the first object
                            if (objectStack.empty())
                            {
                                objectStack.push_back(this);
                                setName(objectName);
                                setId(objectID);
                            }
                            else
                            {
                                auto* o = objectStack.back();
                                objectStack.push_back(o->addObject(objectName, objectID));
                            }

                            objectName.clear();
                            objectID.clear();
                        }
                        else if (tokens[0][0] == '}')
                        {
                            objectStack.pop_back();
                        }
                        else
                        {
                            //stash name/id strings so we can add them when creating a new object
                            objectName = { tokens[0].begin(), tokens[0].end() };
                            
                            if (tokens.size() > 1 &&
                                !tokens[1].empty())
                            {
                                objectID = { tokens[1].begin(), tokens[1].end() };
                            }
                        }
                    }
                }
                else
                {
                    if (tokens[1][0] == '=')
                    {
                        //this is a property
                        auto& prop = objectStack.back()->addProperty(std::string(tokens[0].begin(), tokens[0].end()));
                        
                        if (tokens[2].size() > 1
                            && tokens[2][0] == '"')
                        {
                            //this is a string
                            auto tokenEnd = tokens[2].size() - 1;
                            if (tokens[2].back() != '"')
                            {
                                //we're malformed but attempt to copy anyway
                                tokenEnd++;
                            }

                            //TODO we should be further splitting this if it's a string array
                            //but we don't support getter/setter yet
                            auto& utf = prop.m_utf8Values.emplace_back();
                            for (auto i = 1u; i < tokenEnd; ++i)
                            {
                                utf.push_back(tokens[2][i]);
                            }
                        }

                        else
                        {
                            //try parsing tokens[2] as a CSV of floats
                            std::string tmp;
                            const auto parseFloat = [&]()
                                {
                                    Util::String::removeChar(tmp, ' ');

                                    if (prop.m_floatValues.empty())
                                    {
                                        if (tmp == "true")
                                        {
                                            prop.setValue(true);
                                            return;
                                        }
                                        else if (tmp == "false")
                                        {
                                            prop.setValue(false);
                                            return;
                                        }
                                    }
                                    

                                    try
                                    {
                                        prop.m_floatValues.push_back(std::stod(tmp));
                                    }
                                    catch (...)
                                    {
                                        //for backwards compat stash this as a string
                                        auto& utf = prop.m_utf8Values.emplace_back();
                                        for (auto i = 0u; i < tokens[2].size(); ++i)
                                        {
                                            utf.push_back(tokens[2][i]);
                                        }
                                        //but we don't want to encourage this so nag with a warning
                                        LogW << FileSystem::getFileName(path) << " " << lineNumber << " - " << tmp << ": potential unquoted string value" << std::endl;

                                        //for now log these so we know what to edit MUST REMOVE IT THOUGH
                                        //also below for missing quotes
                                        std::ofstream f("cfg_corrections.txt", std::ios::app);
                                        if (f.is_open() && f.good())
                                        {
                                            f << path << " - line: " << lineNumber << "\n";
                                        }
                                    };

                                    tmp.clear();
                                };

                            for (auto c : tokens[2])
                            {
                                if (c != ',')
                                {
                                    tmp.push_back(c);
                                }
                                else
                                {
                                    //attempt to parse to double. 
                                    parseFloat();
                                }
                            }
                            //don't forget the final value!
                            if (!tmp.empty())
                            {
                                parseFloat();
                            }
                        }
                    }
                }

                if (stringOpen)
                {
                    LogW << FileSystem::getFileName(path) << " - Missing \" on line: " << lineNumber << std::endl;
                    /*std::ofstream f("cfg_corrections.txt", std::ios::app);
                    if (f.is_open() && f.good())
                    {
                        f << path << "(missing \") - line: " << lineNumber << "\n";
                    }*/
                }

                stringOpen = false;
                foundProperty = false;
                currentLine.clear();
                lineNumber++;
            }
        }

        if (!objectStack.empty()
            && objectStack.back() != this) //*sigh* if there's no newline at the end of the file the very last } won't get read...
        {
            //we were missing a closing brace somewhere
            //TODO find the line it was missing from (approx) based on indent??
            LogW << FileSystem::getFileName(path) << ": at least one closing brace is missing" << std::endl;

            std::ofstream f("cfg_corrections.txt", std::ios::app);
            if (f.is_open() && f.good())
            {
                f << path << " at least one closing brace is missing" << "\n";
            }
        }

        return readCount == fileSize;
    }

    return false;
}
#endif


//--------------------//
ConfigItem::ConfigItem(const std::string& name)
    : m_parent  (nullptr),
    m_name      (name)
{
    //replace spaces with underscore and remove non-alphanum
    Util::String::replace(m_name, " ", "_");
    
    m_name.erase(std::remove_if(m_name.begin(), m_name.end(), 
        [](char c)
        {
            return !std::isalnum(c) && c != '_';
        }), m_name.end());
}

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
