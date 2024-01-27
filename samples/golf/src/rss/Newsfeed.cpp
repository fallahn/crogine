/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "Newsfeed.hpp"
#include "pugixml.hpp"

#ifdef USE_GNS
#include <Social.hpp>
#else
#include <curl/curl.h>
#endif
#include <crogine/core/Log.hpp>
#include <crogine/util/String.hpp>

#include <cstring>

namespace
{
    std::size_t writeCallback(char* contents, std::size_t size, std::size_t numMem, void* outStr)
    {
        //hmm, surely there's a bettre way to concat this?
        for (auto i = 0u; i < size * numMem; ++i)
        {
            static_cast<std::vector<std::uint8_t>*>(outStr)->push_back(contents[i]);
        }
        return size * numMem;
    }
}

//public
bool RSSFeed::fetch(const std::string& url)
{
    m_items.clear();
    m_fetchComplete = false;

#ifdef USE_GNS
    Social::requestRSSFeed();
    
    cro::Clock timeout;
    std::vector<std::uint8_t> buffer;
    while (!Social::getRSSFeed(&buffer) && timeout.elapsed().asSeconds() < 5.f)
    {

    }
    return parseFeed(buffer);
#else
    auto* curl = curl_easy_init();
    if (curl)
    {
        std::vector<std::uint8_t> buffer;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        if (auto code = curl_easy_perform(curl); code != CURLE_OK)
        {
            LogE << "cURL fetch failed with code " << code << std::endl;
        }

        return parseFeed(buffer);
    }
#endif

    m_fetchComplete = true;
    return false;
}

void RSSFeed::fetchAsync(const std::string& url)
{
    m_thread = std::thread(&RSSFeed::fetch, this, url);
    m_thread.detach();
}

//private
bool RSSFeed::parseFeed(const std::vector<std::uint8_t>& src)
{
    if (src.empty())
    {
        LogW << "RSS source is empty" << std::endl;
        m_fetchComplete = true;
        return false;
    }

    pugi::xml_document doc;
    if (!doc.load_buffer(src.data(), src.size(), 116u, pugi::encoding_utf8))
    {
        LogE << "Failed to parse returned string to XML" << std::endl;
        m_fetchComplete = true;
        return false;
    }

    auto channel = doc.child("rss").child("channel");

    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        for (const auto item : channel.children("item"))
        {
            auto& i = m_items.emplace_back();

            const auto* s = item.child("title").child_value();

            i.title = cro::String::fromUtf8(s, s + std::strlen(s));
            i.url = item.child("link").child_value();
            i.date = item.child("pubDate").child_value();
            i.date = i.date.substr(0, 16);

            s = item.child("description").child_value();
            i.description = cro::String::fromUtf8(s, s + std::strlen(s));

            cro::Util::String::replace(i.description, "&#039;", "'");
            cro::Util::String::replace(i.description, "<p>", "");
            cro::Util::String::replace(i.description, "</p>", "");
            cro::Util::String::replace(i.description, "<br>", " ");
            cro::Util::String::replace(i.description, "<ul class=\"bb_ul\">", " "); //sigh... wish this didn't have to be so specific...
            cro::Util::String::replace(i.description, "<li>", "\n - ");
            cro::Util::String::replace(i.description, "</li>", " ");
        }
    }
    m_fetchComplete = true;
    return !m_items.empty();
}