/*
MIT License

Copyright (c) 2019-2022 Zhuang Guan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "../detail/IconsFontAwesome6.h"

#include <crogine/gui/detail/imgui.h>
#include <crogine/gui/detail/imfilebrowser.h>

#include <cassert>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string_view>

using namespace ImGui;

FileBrowser::FileBrowser(ImGuiFileBrowserFlags flags)
    : width_(700), height_(450), posX_(0), posY_(0), flags_(flags),
    openFlag_(false), closeFlag_(false), isOpened_(false), ok_(false), posIsSet_(false),
    rangeSelectionStart_(0), inputNameBuf_(std::make_unique<std::array<char, INPUT_NAME_BUF_SIZE>>())
{
    if (flags_ & ImGuiFileBrowserFlags_CreateNewDir)
    {
        newDirNameBuf_ =
            std::make_unique<std::array<char, INPUT_NAME_BUF_SIZE>>();
    }

    inputNameBuf_->front() = '\0';
    inputNameBuf_->back() = '\0';
    SetTitle("file browser");
    SetPwd(std::filesystem::current_path());

    typeFilters_.clear();
    typeFilterIndex_ = 0;
    hasAllFilter_ = false;

#ifdef _WIN32
    drives_ = GetDrivesBitMask();
#endif
}

FileBrowser::FileBrowser(const FileBrowser& copyFrom)
    : FileBrowser()
{
    *this = copyFrom;
}

FileBrowser& ImGui::FileBrowser::operator=(
    const FileBrowser& copyFrom)
{
    width_ = copyFrom.width_;
    height_ = copyFrom.height_;

    posX_ = copyFrom.posX_;
    posY_ = copyFrom.posY_;

    flags_ = copyFrom.flags_;
    SetTitle(copyFrom.title_);

    openFlag_ = copyFrom.openFlag_;
    closeFlag_ = copyFrom.closeFlag_;
    isOpened_ = copyFrom.isOpened_;
    ok_ = copyFrom.ok_;
    posIsSet_ = copyFrom.posIsSet_;

    statusStr_ = "";

    typeFilters_ = copyFrom.typeFilters_;
    typeFilterIndex_ = copyFrom.typeFilterIndex_;
    hasAllFilter_ = copyFrom.hasAllFilter_;

    pwd_ = copyFrom.pwd_;
    selectedFilenames_ = copyFrom.selectedFilenames_;
    rangeSelectionStart_ = copyFrom.rangeSelectionStart_;

    fileRecords_ = copyFrom.fileRecords_;

    *inputNameBuf_ = *copyFrom.inputNameBuf_;

    openNewDirLabel_ = copyFrom.openNewDirLabel_;
    if (flags_ & ImGuiFileBrowserFlags_CreateNewDir)
    {
        newDirNameBuf_ = std::make_unique<
            std::array<char, INPUT_NAME_BUF_SIZE>>();
        *newDirNameBuf_ = *copyFrom.newDirNameBuf_;
    }

#ifdef _WIN32
    drives_ = copyFrom.drives_;
#endif

    return *this;
}

void FileBrowser::SetWindowPos(int posx, int posy) noexcept
{
    posX_ = posx;
    posY_ = posy;
    posIsSet_ = true;
}

void FileBrowser::SetWindowSize(int width, int height) noexcept
{
    assert(width > 0 && height > 0);
    width_ = width;
    height_ = height;
}

void FileBrowser::SetTitle(std::string title)
{
    title_ = std::move(title);
    openLabel_ = title_ + "##filebrowser_" +
        std::to_string(reinterpret_cast<size_t>(this));
    openNewDirLabel_ = "new dir##new_dir_" +
        std::to_string(reinterpret_cast<size_t>(this));
}

void FileBrowser::Open()
{
    UpdateFileRecords();
    ClearSelected();
    statusStr_ = std::string();
    openFlag_ = true;
    closeFlag_ = false;
}

void FileBrowser::Close()
{
    ClearSelected();
    statusStr_ = std::string();
    closeFlag_ = true;
    openFlag_ = false;
}

bool FileBrowser::IsOpened() const noexcept
{
    return isOpened_;
}

void FileBrowser::Display()
{
    PushID(this);
    ScopeGuard exitThis([this]
        {
            openFlag_ = false;
            closeFlag_ = false;
            PopID();
        });

    if (openFlag_)
    {
        OpenPopup(openLabel_.c_str());
    }
    isOpened_ = false;

    // open the popup window

    if (openFlag_ && (flags_ & ImGuiFileBrowserFlags_NoModal))
    {
        if (posIsSet_)
        {
            SetNextWindowPos(
                ImVec2(static_cast<float>(posX_), static_cast<float>(posY_)));
        }
        SetNextWindowSize(
            ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    }
    else
    {
        if (posIsSet_)
        {
            SetNextWindowPos(
                ImVec2(static_cast<float>(posX_), static_cast<float>(posY_)),
                ImGuiCond_FirstUseEver);
        }
        SetNextWindowSize(
            ImVec2(static_cast<float>(width_), static_cast<float>(height_)),
            ImGuiCond_FirstUseEver);
    }
    if (flags_ & ImGuiFileBrowserFlags_NoModal)
    {
        if (!BeginPopup(openLabel_.c_str()))
        {
            return;
        }
    }
    else if (!BeginPopupModal(openLabel_.c_str(), nullptr,
        flags_ & ImGuiFileBrowserFlags_NoTitleBar ?
        ImGuiWindowFlags_NoTitleBar : 0))
    {
        return;
    }

    isOpened_ = true;
    ScopeGuard endPopup([] { EndPopup(); });

    // display elements in pwd

#ifdef _WIN32
    char currentDrive = static_cast<char>(pwd_.c_str()[0]);
    char driveStr[] = { currentDrive, ':', '\0' };

    PushItemWidth(4 * GetFontSize());
    if (BeginCombo("##select_drive", driveStr))
    {
        ScopeGuard guard([&] { EndCombo(); });

        for (int i = 0; i < 26; ++i)
        {
            if (!(drives_ & (1 << i)))
            {
                continue;
            }

            char driveCh = static_cast<char>('A' + i);
            char selectableStr[] = { driveCh, ':', '\0' };
            bool selected = currentDrive == driveCh;

            if (Selectable(selectableStr, selected) && !selected)
            {
                char newPwd[] = { driveCh, ':', '\\', '\0' };
                SetPwd(newPwd);
            }
        }
    }
    PopItemWidth();

    SameLine();
#endif

    int secIdx = 0, newPwdLastSecIdx = -1;
    for (const auto& sec : pwd_)
    {
#ifdef _WIN32
        if (secIdx == 1)
        {
            ++secIdx;
            continue;
        }
#endif

        PushID(secIdx);
        if (secIdx > 0)
        {
            SameLine();
        }
        if (SmallButton(u8StrToStr(sec.u8string()).c_str()))
        {
            newPwdLastSecIdx = secIdx;
        }
        PopID();

        ++secIdx;
    }

    if (newPwdLastSecIdx >= 0)
    {
        int i = 0;
        std::filesystem::path newPwd;
        for (const auto& sec : pwd_)
        {
            if (i++ > newPwdLastSecIdx)
            {
                break;
            }
            newPwd /= sec;
        }

#ifdef _WIN32
        if (newPwdLastSecIdx == 0)
        {
            newPwd /= "\\";
        }
#endif

        SetPwd(newPwd);
    }

    SameLine();

    if (SmallButton("*"))
    {
        UpdateFileRecords();

        std::set<std::filesystem::path> newSelectedFilenames;
        for (auto& name : selectedFilenames_)
        {
            auto it = std::find_if(
                fileRecords_.begin(), fileRecords_.end(),
                [&](const FileRecord& record)
                {
                    return name == record.name;
                });
            if (it != fileRecords_.end())
            {
                newSelectedFilenames.insert(name);
            }
        }

        if (inputNameBuf_ && (*inputNameBuf_)[0])
        {
            newSelectedFilenames.insert(inputNameBuf_->data());
        }
    }

    bool focusOnInputText = false;
    if (newDirNameBuf_)
    {
        SameLine();
        if (SmallButton("+"))
        {
            OpenPopup(openNewDirLabel_.c_str());
            (*newDirNameBuf_)[0] = '\0';
        }

        if (BeginPopup(openNewDirLabel_.c_str()))
        {
            ScopeGuard endNewDirPopup([] { EndPopup(); });

            InputText("name", newDirNameBuf_->data(), newDirNameBuf_->size());
            focusOnInputText |= IsItemFocused();
            SameLine();

            if (Button("ok") && (*newDirNameBuf_)[0] != '\0')
            {
                ScopeGuard closeNewDirPopup([] { CloseCurrentPopup(); });
                if (create_directory(pwd_ / newDirNameBuf_->data()))
                {
                    UpdateFileRecords();
                }
                else
                {
                    statusStr_ = "failed to create " +
                        std::string(newDirNameBuf_->data());
                }
            }
        }
    }

    // browse files in a child window

    float reserveHeight = GetFrameHeightWithSpacing();
    std::filesystem::path newPwd; bool setNewPwd = false;
    if (!(flags_ & ImGuiFileBrowserFlags_SelectDirectory) &&
        (flags_ & ImGuiFileBrowserFlags_EnterNewFilename))
        reserveHeight += GetFrameHeightWithSpacing();

    {
        BeginChild("ch", ImVec2(0, -reserveHeight), true,
            (flags_ & ImGuiFileBrowserFlags_NoModal) ?
            ImGuiWindowFlags_AlwaysHorizontalScrollbar : 0);
        ScopeGuard endChild([] { EndChild(); });

        const bool shouldHideRegularFiles =
            (flags_ & ImGuiFileBrowserFlags_HideRegularFiles) &&
            (flags_ & ImGuiFileBrowserFlags_SelectDirectory);

        for (unsigned int rscIndex = 0; rscIndex < fileRecords_.size(); ++rscIndex)
        {
            auto& rsc = fileRecords_[rscIndex];
            if (!rsc.isDir && shouldHideRegularFiles)
            {
                continue;
            }
            if (!rsc.isDir && !IsExtensionMatched(rsc.extension))
            {
                continue;
            }
            if (!rsc.name.empty() && rsc.name.c_str()[0] == '$')
            {
                continue;
            }

            const bool selected = selectedFilenames_.find(rsc.name)
                != selectedFilenames_.end();
            if (Selectable(rsc.showName.c_str(), selected,
                ImGuiSelectableFlags_DontClosePopups))
            {
                const bool wantDir = flags_ & ImGuiFileBrowserFlags_SelectDirectory;
                const bool canSelect = rsc.name != ".." && rsc.isDir == wantDir;
                const bool rangeSelect =
                    canSelect && GetIO().KeyShift &&
                    rangeSelectionStart_ < fileRecords_.size() &&
                    (flags_ & ImGuiFileBrowserFlags_MultipleSelection) &&
                    IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                const bool multiSelect =
                    !rangeSelect && GetIO().KeyCtrl &&
                    (flags_ & ImGuiFileBrowserFlags_MultipleSelection) &&
                    IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                if (rangeSelect)
                {
                    const unsigned int first = (std::min)(rangeSelectionStart_, rscIndex);
                    const unsigned int last = (std::max)(rangeSelectionStart_, rscIndex);
                    selectedFilenames_.clear();
                    for (unsigned int i = first; i <= last; ++i)
                    {
                        if (fileRecords_[i].isDir != wantDir)
                        {
                            continue;
                        }
                        if (!wantDir && !IsExtensionMatched(fileRecords_[i].extension))
                        {
                            continue;
                        }
                        selectedFilenames_.insert(fileRecords_[i].name);
                    }
                }
                else if (selected)
                {
                    if (!multiSelect)
                    {
                        selectedFilenames_ = { rsc.name };
                        rangeSelectionStart_ = rscIndex;
                    }
                    else
                    {
                        selectedFilenames_.erase(rsc.name);
                    }
                    (*inputNameBuf_)[0] = '\0';
                }
                else if (canSelect)
                {
                    if (multiSelect)
                    {
                        selectedFilenames_.insert(rsc.name);
                    }
                    else
                    {
                        selectedFilenames_ = { rsc.name };
                    }
                    if (!(flags_ & ImGuiFileBrowserFlags_SelectDirectory))
                    {
#ifdef _MSC_VER
                        strcpy_s(
                            inputNameBuf_->data(), inputNameBuf_->size(),
                            u8StrToStr(rsc.name.u8string()).c_str());
#else
                        std::strncpy(inputNameBuf_->data(),
                            u8StrToStr(rsc.name.u8string()).c_str(),
                            inputNameBuf_->size() - 1);
#endif
                    }
                    rangeSelectionStart_ = rscIndex;
                }
                else
                {
                    if (!multiSelect)
                    {
                        selectedFilenames_.clear();
                    }
                }
            }

            if (IsItemClicked(0) && IsMouseDoubleClicked(0))
            {
                if (rsc.isDir)
                {
                    setNewPwd = true;
                    newPwd = (rsc.name != "..") ? (pwd_ / rsc.name) :
                        pwd_.parent_path();
                }
                else if (!(flags_ & ImGuiFileBrowserFlags_SelectDirectory))
                {
                    selectedFilenames_ = { rsc.name };
                    ok_ = true;
                    CloseCurrentPopup();
                }
            }
        }
    }

    if (setNewPwd)
    {
        SetPwd(newPwd);
    }

    if (!(flags_ & ImGuiFileBrowserFlags_SelectDirectory) &&
        (flags_ & ImGuiFileBrowserFlags_EnterNewFilename))
    {
        PushID(this);
        ScopeGuard popTextID([] { PopID(); });

        PushItemWidth(-1);
        if (InputText("", inputNameBuf_->data(), inputNameBuf_->size()) &&
            inputNameBuf_->at(0) != '\0')
        {
            selectedFilenames_ = { inputNameBuf_->data() };
        }
        focusOnInputText |= IsItemFocused();
        PopItemWidth();
    }

    if (!focusOnInputText)
    {
        const bool selectAll = (flags_ & ImGuiFileBrowserFlags_MultipleSelection) &&
            IsKeyPressed(ImGuiKey_A) && (IsKeyDown(ImGuiKey_LeftCtrl) ||
                IsKeyDown(ImGuiKey_RightCtrl));
        if (selectAll)
        {
            const bool needDir = flags_ & ImGuiFileBrowserFlags_SelectDirectory;
            selectedFilenames_.clear();
            for (size_t i = 1; i < fileRecords_.size(); ++i)
            {
                auto& record = fileRecords_[i];
                if (record.isDir == needDir &&
                    (needDir || IsExtensionMatched(record.extension)))
                {
                    selectedFilenames_.insert(record.name);
                }
            }
        }
    }

    const bool enter =
        (flags_ & ImGuiFileBrowserFlags_ConfirmOnEnter) &&
        IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        IsKeyPressed(ImGuiKey_Enter);
    if (!(flags_ & ImGuiFileBrowserFlags_SelectDirectory))
    {
        if ((Button(" ok ") || enter) && !selectedFilenames_.empty())
        {
            ok_ = true;
            CloseCurrentPopup();
        }
    }
    else
    {
        if (Button(" ok ") || enter)
        {
            ok_ = true;
            CloseCurrentPopup();
        }
    }

    SameLine();

    bool shouldExit =
        Button("cancel") || closeFlag_ ||
        ((flags_ & ImGuiFileBrowserFlags_CloseOnEsc) &&
            IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            IsKeyPressed(ImGuiKey_Escape));
    if (shouldExit)
    {
        CloseCurrentPopup();
    }

    if (!statusStr_.empty() && !(flags_ & ImGuiFileBrowserFlags_NoStatusBar))
    {
        SameLine();
        Text("%s", statusStr_.c_str());
    }

    if (!typeFilters_.empty())
    {
        SameLine();
        PushItemWidth(8 * GetFontSize());
        if (BeginCombo(
            "##type_filters", typeFilters_[typeFilterIndex_].c_str()))
        {
            ScopeGuard guard([&] { EndCombo(); });

            for (size_t i = 0; i < typeFilters_.size(); ++i)
            {
                bool selected = i == typeFilterIndex_;
                if (Selectable(typeFilters_[i].c_str(), selected) && !selected)
                {
                    typeFilterIndex_ = static_cast<unsigned int>(i);
                }
            }
        }
        PopItemWidth();
    }
}

bool FileBrowser::HasSelected() const noexcept
{
    return ok_;
}

bool FileBrowser::SetPwd(const std::filesystem::path& pwd)
{
    try
    {
        SetPwdUncatched(pwd);
        return true;
    }
    catch (const std::exception& err)
    {
        statusStr_ = std::string("last error: ") + err.what();
    }
    catch (...)
    {
        statusStr_ = "last error: unknown";
    }

    SetPwdUncatched(std::filesystem::current_path());
    return false;
}

const class std::filesystem::path& FileBrowser::GetPwd() const noexcept
{
    return pwd_;
}

 std::filesystem::path FileBrowser::GetSelected() const
{
    // when ok_ is true, selectedFilenames_ may be empty if SelectDirectory
    // is enabled. return pwd in that case.
    if (selectedFilenames_.empty())
    {
        return pwd_;
    }
    return pwd_ / *selectedFilenames_.begin();
}

 std::vector<std::filesystem::path> FileBrowser::GetMultiSelected() const
{
    if (selectedFilenames_.empty())
    {
        return { pwd_ };
    }

    std::vector<std::filesystem::path> ret;
    ret.reserve(selectedFilenames_.size());
    for (auto& s : selectedFilenames_)
    {
        ret.push_back(pwd_ / s);
    }

    return ret;
}

void FileBrowser::ClearSelected()
{
    selectedFilenames_.clear();
    (*inputNameBuf_)[0] = '\0';
    ok_ = false;
}

void FileBrowser::SetTypeFilters(const std::vector<std::string>& _typeFilters)
{
    typeFilters_.clear();

    // remove duplicate filter names due to case unsensitivity on windows

#ifdef _WIN32

    std::vector<std::string> typeFilters;
    for (auto& rawFilter : _typeFilters)
    {
        std::string lowerFilter = ToLower(rawFilter);
        auto it = std::find(typeFilters.begin(), typeFilters.end(), lowerFilter);
        if (it == typeFilters.end())
        {
            typeFilters.push_back(std::move(lowerFilter));
        }
    }

#else

    auto& typeFilters = _typeFilters;

#endif

    // insert auto-generated filter
    hasAllFilter_ = false;
    if (typeFilters.size() > 1)
    {
        hasAllFilter_ = true;
        std::string allFiltersName = std::string();
        for (size_t i = 0; i < typeFilters.size(); ++i)
        {
            if (typeFilters[i] == std::string_view(".*"))
            {
                hasAllFilter_ = false;
                break;
            }

            if (i > 0)
            {
                allFiltersName += ",";
            }
            allFiltersName += typeFilters[i];
        }

        if (hasAllFilter_)
        {
            typeFilters_.push_back(std::move(allFiltersName));
        }
    }

    std::copy(
        typeFilters.begin(), typeFilters.end(),
        std::back_inserter(typeFilters_));

    typeFilterIndex_ = 0;
}

void FileBrowser::SetCurrentTypeFilterIndex(int index)
{
    typeFilterIndex_ = static_cast<unsigned int>(index);
}

void FileBrowser::SetInputName(std::string_view input)
{
    if (flags_ & ImGuiFileBrowserFlags_EnterNewFilename)
    {
        if (input.size() >= static_cast<size_t>(INPUT_NAME_BUF_SIZE))
        {
            // If input doesn't fit trim off characters
            input = input.substr(0, INPUT_NAME_BUF_SIZE - 1);
        }
        std::copy(input.begin(), input.end(), inputNameBuf_->begin());
        inputNameBuf_->at(input.size()) = '\0';
        selectedFilenames_ = { inputNameBuf_->data() };
    }
}

 std::string FileBrowser::ToLower(const std::string& s)
{
    std::string ret = s;
    for (char& c : ret)
    {
        c = static_cast<char>(std::tolower(c));
    }
    return ret;
}

void FileBrowser::UpdateFileRecords()
{
    fileRecords_ = { FileRecord{ true, "..", ICON_FA_FOLDER " ..", "" } };

    for (auto& p : std::filesystem::directory_iterator(pwd_))
    {
        FileRecord rcd;

        if (p.is_regular_file())
        {
            rcd.isDir = false;
        }
        else if (p.is_directory())
        {
            rcd.isDir = true;
        }
        else
        {
            continue;
        }

        rcd.name = p.path().filename();
        if (rcd.name.empty())
        {
            continue;
        }

        rcd.extension = p.path().filename().extension();

        rcd.showName = (rcd.isDir ? /*"[D] " : "[F] "*/ICON_FA_FOLDER " " : ICON_FA_FILE " ") +
            u8StrToStr(p.path().filename().u8string());
        fileRecords_.push_back(rcd);
    }

    std::sort(fileRecords_.begin(), fileRecords_.end(),
        [](const FileRecord& L, const FileRecord& R)
        {
            return (L.isDir ^ R.isDir) ? L.isDir : (L.name < R.name);
        });

    ClearRangeSelectionState();
}

void FileBrowser::SetPwdUncatched(const std::filesystem::path& pwd)
{
    pwd_ = absolute(pwd);
    UpdateFileRecords();
    selectedFilenames_.clear();
    (*inputNameBuf_)[0] = '\0';
}

bool FileBrowser::IsExtensionMatched(const std::filesystem::path& _extension) const
{
#ifdef _WIN32
    std::filesystem::path extension = ToLower(u8StrToStr(_extension.u8string()));
#else
    auto& extension = _extension;
#endif

    // no type filters
    if (typeFilters_.empty())
    {
        return true;
    }

    // invalid type filter index
    if (static_cast<size_t>(typeFilterIndex_) >= typeFilters_.size())
    {
        return true;
    }

    // all type filters
    if (hasAllFilter_ && typeFilterIndex_ == 0)
    {
        for (size_t i = 1; i < typeFilters_.size(); ++i)
        {
            if (extension == typeFilters_[i])
            {
                return true;
            }
        }
        return false;
    }

    // universal filter
    if (typeFilters_[typeFilterIndex_] == std::string_view(".*"))
    {
        return true;
    }

    // regular filter
    return extension == typeFilters_[typeFilterIndex_];
}

void FileBrowser::ClearRangeSelectionState()
{
    rangeSelectionStart_ = 9999999;
    const bool dir = flags_ & ImGuiFileBrowserFlags_SelectDirectory;
    for (unsigned int i = 1; i < fileRecords_.size(); ++i)
    {
        if (fileRecords_[i].isDir == dir)
        {
            if (!dir && !IsExtensionMatched(fileRecords_[i].extension))
            {
                continue;
            }
            rangeSelectionStart_ = i;
            break;
        }
    }
}

#if defined(__cpp_lib_char8_t)
std::string FileBrowser::u8StrToStr(std::u8string s)
{
    return std::string(s.begin(), s.end());
}
#endif

std::string FileBrowser::u8StrToStr(std::string s)
{
    return s;
}

#ifdef _WIN32

#ifndef _INC_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN

#define IMGUI_FILEBROWSER_UNDEF_WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN

#endif // #ifndef WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <windows.h>

#ifdef IMGUI_FILEBROWSER_UNDEF_WIN32_LEAN_AND_MEAN
#undef IMGUI_FILEBROWSER_UNDEF_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif // #ifdef IMGUI_FILEBROWSER_UNDEF_WIN32_LEAN_AND_MEAN

#endif // #ifdef _INC_WINDOWS

std::uint32_t FileBrowser::GetDrivesBitMask()
{
    DWORD mask = GetLogicalDrives();
    uint32_t ret = 0;
    for (int i = 0; i < 26; ++i)
    {
        if (!(mask & (1 << i)))
        {
            continue;
        }
        char rootName[4] = { static_cast<char>('A' + i), ':', '\\', '\0' };
        UINT type = GetDriveTypeA(rootName);
        if (type == DRIVE_REMOVABLE || type == DRIVE_FIXED || type == DRIVE_REMOTE)
        {
            ret |= (1 << i);
        }
    }
    return ret;
}
#endif