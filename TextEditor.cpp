#include "TextEditor.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <regex>
#include <string>
#include <utility>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h" // sadly seems to be needed for PlatformImeData
#include "Palette.h"

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML

template<class InputIt1, class InputIt2, class BinaryPredicate>
static bool equals(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p)
{
    for (; first1 != last1 && first2 != last2; ++first1, ++first2)
    {
        if (!p(*first1, *first2))
        {
            return false;
        }
    }
    return first1 == last1 && first2 == last2;
}

TextEditor::TextEditor():
    mLineSpacing(1.0f),
    mUndoIndex(0),
    mTabSize(4),
    mOverwrite(false),
    mReadOnly(false),
    mWithinRender(false),
    mScrollToCursor(false),
    mScrollToTop(false),
    mTextChanged(false),
    mColorizerEnabled(true),
    mTextStart(20.0f),
    mLeftMargin(10),
    mCursorPositionChanged(false),
    mColorRangeMin(0),
    mColorRangeMax(0),
    mSelectionMode(SelectionMode::Normal),
    mHandleKeyboardInputs(true),
    mHandleMouseInputs(true),
    mIgnoreImGuiChild(false),
    mShowWhitespaces(true),
    mCheckComments(true),
    mStartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()
                                                                             .time_since_epoch())
                       .count()),
    mLastClick(-1.0f)
{
    SetPalette(GetDarkPalette());
    SetLanguageDefinition(LanguageDefinition::GLSL());
    mLines.emplace_back();
}

void TextEditor::SetLanguageDefinition(const LanguageDefinition &aLanguageDef)
{
    mLanguageDefinition = aLanguageDef;
    mRegexList.clear();

    for (auto &r: mLanguageDefinition.mTokenRegexStrings)
    {
        mRegexList.emplace_back(std::regex(r.first, std::regex_constants::optimize), r.second);
    }

    Colorize();
}

void TextEditor::SetPalette(const Palette &aValue)
{
    mPaletteBase = aValue;
}

std::string TextEditor::GetText(const Coordinates &aStart, const Coordinates &aEnd) const
{
    std::string result;

    auto lstart = aStart.mLine;
    const auto lend = aEnd.mLine;
    auto istart = GetCharacterIndex(aStart);
    auto iend = GetCharacterIndex(aEnd);
    size_t s = 0;

    for (size_t i = lstart; std::cmp_less(i, lend); i++)
    {
        s += mLines.at(i).size();
    }

    result.reserve(s + s / 8);

    while (istart < iend || lstart < lend)
    {
        if (lstart >= static_cast<int>(mLines.size()))
        {
            break;
        }

        const auto &line = mLines.at(lstart);
        if (istart < static_cast<int>(line.size()))
        {
            result += line.at(istart).mChar;
            istart++;
        } else
        {
            istart = 0;
            ++lstart;
            result += '\n';
        }
    }

    return result;
}

Coordinates TextEditor::GetActualCursorCoordinates() const
{
    return SanitizeCoordinates(mState.mCursorPosition);
}

ImVec2 TextEditor::GetCursorScreenPosition() const
{
    const Coordinates cursor_position = GetActualCursorCoordinates();

    const ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    const ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x,
                                             cursorScreenPos.y +
                                                     static_cast<float>(cursor_position.mLine) * mCharAdvance.y);
    const ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);

    const float cx = TextDistanceToLineStart(mState.mCursorPosition);
    return {textScreenPos.x + cx, lineStartScreenPos.y};
}

Coordinates TextEditor::SanitizeCoordinates(const Coordinates &aValue) const
{
    auto line = aValue.mLine;
    auto column = aValue.mColumn;
    if (line >= static_cast<int>(mLines.size()))
    {
        if (mLines.empty())
        {
            line = 0;
            column = 0;
        } else
        {
            line = static_cast<int>(mLines.size()) - 1;
            column = GetLineMaxColumn(line);
        }
        return {line, column};
    }
    column = mLines.empty() ? 0 : std::min(column, GetLineMaxColumn(line));
    return {line, column};
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(const Char c)
{
    if ((c & 0xFE) == 0xFC)
    {
        return 6;
    }
    if ((c & 0xFC) == 0xF8)
    {
        return 5;
    }
    if ((c & 0xF8) == 0xF0)
    {
        return 4;
    }
    if ((c & 0xF0) == 0xE0)
    {
        return 3;
    }
    if ((c & 0xE0) == 0xC0)
    {
        return 2;
    }
    return 1;
}

// "Borrowed" from ImGui source
static int ImTextCharToUtf8(char *buf, const int buf_size, const unsigned int c)
{
    if (c < 0x80)
    {
        buf[0] = static_cast<char>(c);
        return 1;
    }
    if (c < 0x800)
    {
        if (buf_size < 2)
        {
            return 0;
        }
        buf[0] = static_cast<char>(0xc0 + (c >> 6));
        buf[1] = static_cast<char>(0x80 + (c & 0x3f));
        return 2;
    }
    if (c >= 0xdc00 && c < 0xe000)
    {
        return 0;
    }
    if (c >= 0xd800 && c < 0xdc00)
    {
        if (buf_size < 4)
        {
            return 0;
        }
        buf[0] = static_cast<char>(0xf0 + (c >> 18));
        buf[1] = static_cast<char>(0x80 + ((c >> 12) & 0x3f));
        buf[2] = static_cast<char>(0x80 + ((c >> 6) & 0x3f));
        buf[3] = static_cast<char>(0x80 + ((c) & 0x3f));
        return 4;
    }
    //else if (c < 0x10000)
    {
        if (buf_size < 3)
        {
            return 0;
        }
        buf[0] = static_cast<char>(0xe0 + (c >> 12));
        buf[1] = static_cast<char>(0x80 + ((c >> 6) & 0x3f));
        buf[2] = static_cast<char>(0x80 + ((c) & 0x3f));
        return 3;
    }
}

void TextEditor::Advance(Coordinates &aCoordinates) const
{
    if (aCoordinates.mLine < static_cast<int>(mLines.size()))
    {
        const auto &line = mLines.at(aCoordinates.mLine);
        auto cindex = GetCharacterIndex(aCoordinates);

        if (cindex + 1 < static_cast<int>(line.size()))
        {
            const auto delta = UTF8CharLength(line.at(cindex).mChar);
            cindex = std::min(cindex + delta, static_cast<int>(line.size()) - 1);
        } else
        {
            ++aCoordinates.mLine;
            cindex = 0;
        }
        aCoordinates.mColumn = GetCharacterColumn(aCoordinates.mLine, cindex);
    }
}

void TextEditor::DeleteRange(const Coordinates &aStart, const Coordinates &aEnd)
{
    assert(aEnd >= aStart);
    assert(!mReadOnly);

    //printf("D(%d.%d)-(%d.%d)\n", aStart.mLine, aStart.mColumn, aEnd.mLine, aEnd.mColumn);

    if (aEnd == aStart)
    {
        return;
    }

    const auto start = GetCharacterIndex(aStart);
    const auto end = GetCharacterIndex(aEnd);

    if (aStart.mLine == aEnd.mLine)
    {
        auto &line = mLines.at(aStart.mLine);
        const auto n = GetLineMaxColumn(aStart.mLine);
        if (aEnd.mColumn >= n)
        {
            line.erase(line.begin() + start, line.end());
        } else
        {
            line.erase(line.begin() + start, line.begin() + end);
        }
    } else
    {
        auto &firstLine = mLines.at(aStart.mLine);
        auto &lastLine = mLines.at(aEnd.mLine);

        firstLine.erase(firstLine.begin() + start, firstLine.end());
        lastLine.erase(lastLine.begin(), lastLine.begin() + end);

        if (aStart.mLine < aEnd.mLine)
        {
            firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());
        }

        if (aStart.mLine < aEnd.mLine)
        {
            RemoveLine(aStart.mLine + 1, aEnd.mLine + 1);
        }
    }

    mTextChanged = true;
}

int TextEditor::InsertTextAt(Coordinates & /* inout */ aWhere, const char *aValue)
{
    assert(!mReadOnly);

    int cindex = GetCharacterIndex(aWhere);
    int totalLines = 0;
    while (*aValue != '\0')
    {
        assert(!mLines.empty());

        if (*aValue == '\r')
        {
            // skip
            ++aValue;
        } else if (*aValue == '\n')
        {
            if (cindex < static_cast<int>(mLines.at(aWhere.mLine).size()))
            {
                auto &newLine = InsertLine(aWhere.mLine + 1);
                auto &line = mLines.at(aWhere.mLine);
                newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
                line.erase(line.begin() + cindex, line.end());
            } else
            {
                (void)InsertLine(aWhere.mLine + 1);
            }
            ++aWhere.mLine;
            aWhere.mColumn = 0;
            cindex = 0;
            ++totalLines;
            ++aValue;
        } else
        {
            auto &line = mLines.at(aWhere.mLine);
            auto d = UTF8CharLength(*aValue);
            while (d-- > 0 && *aValue != '\0')
            {
                line.insert(line.begin() + cindex++, Glyph(*aValue++, PaletteIndex::Default));
            }
            ++aWhere.mColumn;
        }

        mTextChanged = true;
    }

    return totalLines;
}

void TextEditor::AddUndo(const UndoRecord &aValue)
{
    assert(!mReadOnly);
    //printf("AddUndo: (@%d.%d) +\'%s' [%d.%d .. %d.%d], -\'%s', [%d.%d .. %d.%d] (@%d.%d)\n",
    //	aValue.mBefore.mCursorPosition.mLine, aValue.mBefore.mCursorPosition.mColumn,
    //	aValue.mAdded.c_str(), aValue.mAddedStart.mLine, aValue.mAddedStart.mColumn, aValue.mAddedEnd.mLine, aValue.mAddedEnd.mColumn,
    //	aValue.mRemoved.c_str(), aValue.mRemovedStart.mLine, aValue.mRemovedStart.mColumn, aValue.mRemovedEnd.mLine, aValue.mRemovedEnd.mColumn,
    //	aValue.mAfter.mCursorPosition.mLine, aValue.mAfter.mCursorPosition.mColumn
    //	);

    mUndoBuffer.resize(static_cast<size_t>(mUndoIndex) + 1);
    mUndoBuffer.back() = aValue;
    ++mUndoIndex;
}

Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2 &aPosition) const
{
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

    const int lineNo = std::max(0, static_cast<int>(floor(local.y / mCharAdvance.y)));

    int columnCoord = 0;

    if (lineNo >= 0 && lineNo < static_cast<int>(mLines.size()))
    {
        const auto &line = mLines.at(lineNo);

        int columnIndex = 0;
        float columnX = 0.0f;

        while (static_cast<size_t>(columnIndex) < line.size())
        {
            float columnWidth = 0.0f;

            if (line.at(columnIndex).mChar == '\t')
            {
                const float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
                const float oldX = columnX;
                const float newColumnX = (1.0f +
                                          std::floor((1.0f + columnX) / (static_cast<float>(mTabSize) * spaceSize))) *
                                         (static_cast<float>(mTabSize) * spaceSize);
                columnWidth = newColumnX - oldX;
                if (mTextStart + columnX + columnWidth * 0.5f > local.x)
                {
                    break;
                }
                columnX = newColumnX;
                columnCoord = (columnCoord / mTabSize) * mTabSize + mTabSize;
                columnIndex++;
            } else
            {
                std::array<char, 7> buf{};
                auto d = UTF8CharLength(line.at(columnIndex).mChar);
                int i = 0;
                while (i < 6 && d-- > 0)
                {
                    buf.at(i++) = line.at(columnIndex++).mChar;
                }
                buf.at(i) = '\0';
                columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf.data()).x;
                if (mTextStart + columnX + columnWidth * 0.5f > local.x)
                {
                    break;
                }
                columnX += columnWidth;
                columnCoord++;
            }
        }
    }

    return SanitizeCoordinates(Coordinates(lineNo, columnCoord));
}

Coordinates TextEditor::FindWordStart(const Coordinates &aFrom) const
{
    const Coordinates at = aFrom;
    if (at.mLine >= static_cast<int>(mLines.size()))
    {
        return at;
    }

    const auto &line = mLines.at(at.mLine);
    auto cindex = GetCharacterIndex(at);

    if (cindex >= static_cast<int>(line.size()))
    {
        return at;
    }

    while (cindex > 0 && isspace(line.at(cindex).mChar) != 0)
    {
        --cindex;
    }

    auto cstart = static_cast<PaletteIndex>(line.at(cindex).mColorIndex);
    while (cindex > 0)
    {
        auto c = line.at(cindex).mChar;
        if ((c & 0xC0) != 0x80) // not UTF code sequence 10xxxxxx
        {
            if (c <= 32 && isspace(c) != 0)
            {
                cindex++;
                break;
            }
            if (cstart != static_cast<PaletteIndex>(line.at(static_cast<size_t>(cindex - 1)).mColorIndex))
            {
                break;
            }
        }
        --cindex;
    }
    return {at.mLine, GetCharacterColumn(at.mLine, cindex)};
}

Coordinates TextEditor::FindWordEnd(const Coordinates &aFrom) const
{
    const Coordinates at = aFrom;
    if (at.mLine >= static_cast<int>(mLines.size()))
    {
        return at;
    }

    const auto &line = mLines.at(at.mLine);
    auto cindex = GetCharacterIndex(at);

    if (cindex >= static_cast<int>(line.size()))
    {
        return at;
    }

    const bool prevspace = isspace(line.at(cindex).mChar) != 0;
    auto cstart = static_cast<PaletteIndex>(line.at(cindex).mColorIndex);
    while (cindex < static_cast<int>(line.size()))
    {
        const auto c = line.at(cindex).mChar;
        const auto d = UTF8CharLength(c);
        if (cstart != static_cast<PaletteIndex>(line.at(cindex).mColorIndex))
        {
            break;
        }

        if (prevspace != isspace(c) != 0)
        {
            if (isspace(c) != 0)
            {
                while (cindex < static_cast<int>(line.size()) && isspace(line.at(cindex).mChar) != 0)
                {
                    ++cindex;
                }
            }
            break;
        }
        cindex += d;
    }
    return {aFrom.mLine, GetCharacterColumn(aFrom.mLine, cindex)};
}

Coordinates TextEditor::FindNextWord(const Coordinates &aFrom) const
{
    Coordinates at = aFrom;
    if (at.mLine >= static_cast<int>(mLines.size()))
    {
        return at;
    }

    // skip to the next non-word character
    auto cindex = GetCharacterIndex(aFrom);
    bool isword = false;
    bool skip = false;
    if (cindex < static_cast<int>(mLines.at(at.mLine).size()))
    {
        const auto &line = mLines.at(at.mLine);
        isword = isalnum(line.at(cindex).mChar) != 0;
        skip = isword;
    }

    while (!isword || skip)
    {
        if (static_cast<size_t>(at.mLine) >= mLines.size())
        {
            const auto l = std::max(0, static_cast<int>(mLines.size()) - 1);
            return {l, GetLineMaxColumn(l)};
        }

        const auto &line = mLines.at(at.mLine);
        if (cindex < static_cast<int>(line.size()))
        {
            isword = isalnum(line.at(cindex).mChar) != 0;

            if (isword && !skip)
            {
                return {at.mLine, GetCharacterColumn(at.mLine, cindex)};
            }

            if (!isword)
            {
                skip = false;
            }

            cindex++;
        } else
        {
            cindex = 0;
            ++at.mLine;
            skip = false;
            isword = false;
        }
    }

    return at;
}

int TextEditor::GetCharacterIndex(const Coordinates &aCoordinates) const
{
    if (static_cast<size_t>(aCoordinates.mLine) >= mLines.size())
    {
        return -1;
    }
    const auto &line = mLines.at(aCoordinates.mLine);
    int c = 0;
    int i = 0;
    while (static_cast<size_t>(i) < line.size() && c < aCoordinates.mColumn)
    {
        if (line.at(i).mChar == '\t')
        {
            c = (c / mTabSize) * mTabSize + mTabSize;
        } else
        {
            ++c;
        }
        i += UTF8CharLength(line.at(i).mChar);
    }
    return i;
}

int TextEditor::GetCharacterColumn(const int aLine, const int aIndex) const
{
    if (static_cast<size_t>(aLine) >= mLines.size())
    {
        return 0;
    }
    const auto &line = mLines.at(aLine);
    int col = 0;
    int i = 0;
    while (i < aIndex && i < static_cast<int>(line.size()))
    {
        auto c = line.at(i).mChar;
        i += UTF8CharLength(c);
        if (c == '\t')
        {
            col = (col / mTabSize) * mTabSize + mTabSize;
        } else
        {
            col++;
        }
    }
    return col;
}

int TextEditor::GetLineCharacterCount(const int aLine) const
{
    if (static_cast<size_t>(aLine) >= mLines.size())
    {
        return 0;
    }
    const auto &line = mLines.at(aLine);
    int c = 0;
    for (unsigned i = 0; i < line.size(); c++)
    {
        i += UTF8CharLength(line.at(i).mChar);
    }
    return c;
}

int TextEditor::GetLineMaxColumn(const int aLine) const
{
    if (static_cast<size_t>(aLine) >= mLines.size())
    {
        return 0;
    }
    const auto &line = mLines.at(aLine);
    int col = 0;
    for (unsigned i = 0; i < line.size();)
    {
        const auto c = line.at(i).mChar;
        if (c == '\t')
        {
            col = (col / mTabSize) * mTabSize + mTabSize;
        } else
        {
            col++;
        }
        i += UTF8CharLength(c);
    }
    return col;
}

bool TextEditor::IsOnWordBoundary(const Coordinates &aAt) const
{
    if (aAt.mLine >= static_cast<int>(mLines.size()) || aAt.mColumn == 0)
    {
        return true;
    }

    const auto &line = mLines.at(aAt.mLine);
    auto cindex = GetCharacterIndex(aAt);
    if (cindex >= static_cast<int>(line.size()))
    {
        return true;
    }

    if (mColorizerEnabled)
    {
        return line.at(cindex).mColorIndex != line.at(static_cast<size_t>(cindex - 1)).mColorIndex;
    }

    return isspace(line.at(cindex).mChar) != isspace(line.at(cindex - 1).mChar);
}

void TextEditor::RemoveLine(const int aStart, const int aEnd)
{
    assert(!mReadOnly);
    assert(aEnd >= aStart);
    assert(mLines.size() > static_cast<size_t>(aEnd - aStart));

    ErrorMarkers etmp;
    for (auto &i: mErrorMarkers)
    {
        const ErrorMarkers::value_type e(i.first >= aStart ? i.first - 1 : i.first, i.second);
        if (e.first >= aStart && e.first <= aEnd)
        {
            continue;
        }
        etmp.insert(e);
    }
    mErrorMarkers = std::move(etmp);

    Breakpoints btmp;
    for (auto i: mBreakpoints)
    {
        if (i >= aStart && i <= aEnd)
        {
            continue;
        }
        btmp.insert(i >= aStart ? i - 1 : i);
    }
    mBreakpoints = std::move(btmp);

    mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd);
    assert(!mLines.empty());

    mTextChanged = true;
}

void TextEditor::RemoveLine(const int aIndex)
{
    assert(!mReadOnly);
    assert(mLines.size() > 1);

    ErrorMarkers etmp;
    for (auto &i: mErrorMarkers)
    {
        const ErrorMarkers::value_type e(i.first > aIndex ? i.first - 1 : i.first, i.second);
        if (e.first - 1 == aIndex)
        {
            continue;
        }
        etmp.insert(e);
    }
    mErrorMarkers = std::move(etmp);

    Breakpoints btmp;
    for (auto i: mBreakpoints)
    {
        if (i == aIndex)
        {
            continue;
        }
        btmp.insert(i >= aIndex ? i - 1 : i);
    }
    mBreakpoints = std::move(btmp);

    mLines.erase(mLines.begin() + aIndex);
    assert(!mLines.empty());

    mTextChanged = true;
}

Line &TextEditor::InsertLine(const int aIndex)
{
    assert(!mReadOnly);

    auto &result = *mLines.insert(mLines.begin() + aIndex, Line());

    ErrorMarkers etmp;
    for (auto &i: mErrorMarkers)
    {
        etmp.insert(ErrorMarkers::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
    }
    mErrorMarkers = std::move(etmp);

    Breakpoints btmp;
    for (auto i: mBreakpoints)
    {
        btmp.insert(i >= aIndex ? i + 1 : i);
    }
    mBreakpoints = std::move(btmp);

    return result;
}

std::string TextEditor::GetWordUnderCursor() const
{
    return GetWordAt(GetCursorPosition());
}

std::string TextEditor::GetWordAt(const Coordinates &aCoords) const
{
    const auto start = FindWordStart(aCoords);
    const auto end = FindWordEnd(aCoords);

    std::string r;

    const auto istart = GetCharacterIndex(start);
    const auto iend = GetCharacterIndex(end);

    for (auto it = istart; it < iend; ++it)
    {
        r.push_back(mLines.at(aCoords.mLine).at(it).mChar);
    }

    return r;
}

ImU32 TextEditor::GetGlyphColor(const Glyph &aGlyph) const
{
    if (!mColorizerEnabled)
    {
        return mPalette.at(static_cast<int>(PaletteIndex::Default));
    }
    if (aGlyph.mComment)
    {
        return mPalette.at(static_cast<int>(PaletteIndex::Comment));
    }
    if (aGlyph.mMultiLineComment)
    {
        return mPalette.at(static_cast<int>(PaletteIndex::MultiLineComment));
    }
    const auto color = mPalette.at(static_cast<int>(aGlyph.mColorIndex));
    if (aGlyph.mPreprocessor)
    {
        const auto ppcolor = mPalette.at(static_cast<int>(PaletteIndex::Preprocessor));
        const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
        const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
        const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
        const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
        return static_cast<ImU32>(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
    }
    return color;
}

void TextEditor::HandleKeyboardInputs()
{
    ImGuiIO &io = ImGui::GetIO();
    const auto shift = io.KeyShift;
    const auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    const auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    if (ImGui::IsWindowFocused())
    {
        if (ImGui::IsWindowHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        }
        //ImGui::CaptureKeyboardFromApp(true);

        const ImVec2 cursorPos = GetCursorScreenPosition();
        ImGuiContext *ctx = ImGui::GetCurrentContext();
        ImGuiPlatformImeData *ime_data = &ctx->PlatformImeData;
        ime_data->WantVisible = true;
        ime_data->WantTextInput = true;
        ime_data->InputPos = ImVec2(cursorPos.x - 1.0f, cursorPos.y - ImGui::GetFontSize());
        ime_data->InputLineHeight = 18;
        ime_data->ViewportId = ImGui::GetWindowViewport()->ID;

        io.WantCaptureKeyboard = true;
        io.WantTextInput = true;

        if (!IsReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            Undo();
        } else if (!IsReadOnly() && !ctrl && !shift && alt && ImGui::IsKeyPressed(ImGuiKey_Backspace))
        {
            Undo();
        } else if (!IsReadOnly() && ctrl && shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            Redo();
        } else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        {
            MoveUp(1, shift);
        } else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        {
            MoveDown(1, shift);
        } else if (!alt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        {
            MoveLeft(1, shift, ctrl);
        } else if (!alt && ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            MoveRight(1, shift, ctrl);
        } else if (!alt && ImGui::IsKeyPressed(ImGuiKey_PageUp))
        {
            MoveUp(GetPageSize() - 4, shift);
        } else if (!alt && ImGui::IsKeyPressed(ImGuiKey_PageDown))
        {
            MoveDown(GetPageSize() - 4, shift);
        } else if (!alt && ctrl && ImGui::IsKeyPressed(ImGuiKey_Home))
        {
            MoveTop(shift);
        } else if (ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_End))
        {
            MoveBottom(shift);
        } else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Home))
        {
            MoveHome(shift);
        } else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_End))
        {
            MoveEnd(shift);
        } else if (!IsReadOnly() && !ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            Delete();
        } else if (!IsReadOnly() && !ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Backspace))
        {
            Backspace();
        } else if (!ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Insert))
        {
            mOverwrite ^= 1;
        } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Insert))
        {
            Copy();
        } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_C))
        {
            Copy();
        } else if (!IsReadOnly() && !ctrl && shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Insert))
        {
            Paste();
        } else if (!IsReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_V))
        {
            Paste();
        } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_X))
        {
            Cut();
        } else if (!ctrl && shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            Cut();
        } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_A))
        {
            SelectAll();
        } else if (!IsReadOnly() && !ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            EnterCharacter('\n', false);
        } else if (!IsReadOnly() && !ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Tab))
        {
            EnterCharacter('\t', shift);
        }

        if (!IsReadOnly() && !io.InputQueueCharacters.empty())
        {
            for (int i = 0; i < io.InputQueueCharacters.Size; i++)
            {
                const auto c = io.InputQueueCharacters[i];
                if (c != 0 && (c == '\n' || c >= 32))
                {
                    EnterCharacter(c, shift);
                }
            }
            io.InputQueueCharacters.resize(0);
        }
    }
}

void TextEditor::HandleMouseInputs()
{
    ImGuiIO &io = ImGui::GetIO();
    const auto shift = io.KeyShift;
    const auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    const auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    if (ImGui::IsWindowHovered())
    {
        if (!shift && !alt)
        {
            const auto click = ImGui::IsMouseClicked(0);
            const auto doubleClick = ImGui::IsMouseDoubleClicked(0);
            const auto t = ImGui::GetTime();
            const auto tripleClick = click &&
                                     !doubleClick &&
                                     (mLastClick != -1.0f && (t - mLastClick) < io.MouseDoubleClickTime);

            /*
			Left mouse button triple click
			*/

            if (tripleClick)
            {
                if (!ctrl)
                {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(
                            ImGui::GetMousePos());
                    mSelectionMode = SelectionMode::Line;
                    SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
                }

                mLastClick = -1.0f;
            }

            /*
			Left mouse button double click
			*/

            else if (doubleClick)
            {
                if (!ctrl)
                {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(
                            ImGui::GetMousePos());
                    if (mSelectionMode == SelectionMode::Line)
                    {
                        mSelectionMode = SelectionMode::Normal;
                    } else
                    {
                        mSelectionMode = SelectionMode::Word;
                    }
                    SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
                }

                mLastClick = static_cast<float>(ImGui::GetTime());
            }

            /*
			Left mouse button click
			*/
            else if (click)
            {
                mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(
                        ImGui::GetMousePos());
                if (ctrl)
                {
                    mSelectionMode = SelectionMode::Word;
                } else
                {
                    mSelectionMode = SelectionMode::Normal;
                }
                SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);

                mLastClick = static_cast<float>(ImGui::GetTime());
            }
            // Mouse left button dragging (=> update selection)
            else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0))
            {
                io.WantCaptureMouse = true;
                mState.mCursorPosition = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
            }
        }
    }
}

void TextEditor::Render()
{
    /* Compute mCharAdvance regarding to scaled font size (Ctrl + mouse wheel)*/
    const float
            fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    mCharAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * mLineSpacing);

    /* Update palette with the current alpha from style */
    for (int i = 0; i < static_cast<int>(PaletteIndex::Max); ++i)
    {
        auto color = ImGui::ColorConvertU32ToFloat4(mPaletteBase.at(i));
        color.w *= ImGui::GetStyle().Alpha;
        mPalette.at(i) = ImGui::ColorConvertFloat4ToU32(color);
    }

    assert(mLineBuffer.empty());

    auto contentSize = ImGui::GetWindowContentRegionMax();
    auto *const drawList = ImGui::GetWindowDrawList();
    float longest(mTextStart);

    if (mScrollToTop)
    {
        mScrollToTop = false;
        ImGui::SetScrollY(0.f);
    }

    const ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    auto scrollX = ImGui::GetScrollX();
    auto scrollY = ImGui::GetScrollY();

    auto lineNo = static_cast<int>(floor(scrollY / mCharAdvance.y));
    auto globalLineMax = static_cast<int>(mLines.size());
    auto lineMax = std::max(0,
                            std::min(static_cast<int>(mLines.size()) - 1,
                                     lineNo + static_cast<int>(floor((scrollY + contentSize.y) / mCharAdvance.y))));

    // Deduce mTextStart by evaluating mLines size (global lineMax) plus two spaces as text width
    std::array<char, 16> buf{};
    snprintf(buf.data(), 16, " %d ", globalLineMax);
    mTextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf.data(), nullptr, nullptr).x +
                 static_cast<float>(mLeftMargin);

    if (!mLines.empty())
    {
        const float spaceSize = ImGui::GetFont()
                                        ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr)
                                        .x;

        while (lineNo <= lineMax)
        {
            const ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineNo * mCharAdvance.y);
            const ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);

            auto &line = mLines.at(lineNo);
            longest = std::max(mTextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))),
                               longest);
            const Coordinates lineStartCoord(lineNo, 0);
            const Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

            // Draw selection for the current line
            float sstart = -1.0f;
            float ssend = -1.0f;

            assert(mState.mSelectionStart <= mState.mSelectionEnd);
            if (mState.mSelectionStart <= lineEndCoord)
            {
                sstart = mState.mSelectionStart > lineStartCoord ? TextDistanceToLineStart(mState.mSelectionStart)
                                                                 : 0.0f;
            }
            if (mState.mSelectionEnd > lineStartCoord)
            {
                ssend = TextDistanceToLineStart(mState.mSelectionEnd < lineEndCoord ? mState.mSelectionEnd
                                                                                    : lineEndCoord);
            }

            if (mState.mSelectionEnd.mLine > lineNo)
            {
                ssend += mCharAdvance.x;
            }

            if (sstart != -1 && ssend != -1 && sstart < ssend)
            {
                const ImVec2 vstart(lineStartScreenPos.x + mTextStart + sstart, lineStartScreenPos.y);
                const ImVec2 vend(lineStartScreenPos.x + mTextStart + ssend, lineStartScreenPos.y + mCharAdvance.y);
                drawList->AddRectFilled(vstart, vend, mPalette.at(static_cast<int>(PaletteIndex::Selection)));
            }

            // Draw breakpoints
            auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

            if (mBreakpoints.contains(lineNo + 1))
            {
                auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX,
                                  lineStartScreenPos.y + mCharAdvance.y);
                drawList->AddRectFilled(start, end, mPalette.at(static_cast<int>(PaletteIndex::Breakpoint)));
            }

            // Draw error markers
            auto errorIt = mErrorMarkers.find(lineNo + 1);
            if (errorIt != mErrorMarkers.end())
            {
                auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX,
                                  lineStartScreenPos.y + mCharAdvance.y);
                drawList->AddRectFilled(start, end, mPalette.at(static_cast<int>(PaletteIndex::ErrorMarker)));

                if (ImGui::IsMouseHoveringRect(lineStartScreenPos, end))
                {
                    ImGui::BeginTooltip();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                    ImGui::Text("Error at line %d:", errorIt->first);
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
                    ImGui::Text("%s", errorIt->second.c_str());
                    ImGui::PopStyleColor();
                    ImGui::EndTooltip();
                }
            }

            // Draw line number (right aligned)
            snprintf(buf.data(), 16, "%d  ", lineNo + 1);

            auto lineNoWidth = ImGui::GetFont()
                                       ->CalcTextSizeA(ImGui::GetFontSize(),
                                                       FLT_MAX,
                                                       -1.0f,
                                                       buf.data(),
                                                       nullptr,
                                                       nullptr)
                                       .x;
            drawList->AddText(ImVec2(lineStartScreenPos.x + mTextStart - lineNoWidth, lineStartScreenPos.y),
                              mPalette.at(static_cast<int>(PaletteIndex::LineNumber)),
                              buf.data());

            if (mState.mCursorPosition.mLine == lineNo)
            {
                auto focused = ImGui::IsWindowFocused();

                // Highlight the current line (where the cursor is)
                if (!HasSelection())
                {
                    auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + mCharAdvance.y);
                    drawList->AddRectFilled(start,
                                            end,
                                            mPalette.at(static_cast<
                                                        int>(focused ? PaletteIndex::CurrentLineFill
                                                                     : PaletteIndex::CurrentLineFillInactive)));
                    drawList->AddRect(start, end, mPalette.at(static_cast<int>(PaletteIndex::CurrentLineEdge)), 1.0f);
                }

                // Render the cursor
                if (focused)
                {
                    auto timeEnd = std::chrono::duration_cast<
                                           std::chrono::milliseconds>(std::chrono::system_clock::now()
                                                                              .time_since_epoch())
                                           .count();
                    auto elapsed = timeEnd - mStartTime;
                    if (elapsed > 400)
                    {
                        float width = 1.0f;
                        auto cindex = GetCharacterIndex(mState.mCursorPosition);
                        const float cx = TextDistanceToLineStart(mState.mCursorPosition);

                        if (mOverwrite && cindex < static_cast<int>(line.size()))
                        {
                            auto c = line.at(cindex).mChar;
                            if (c == '\t')
                            {
                                auto x = (1.0f + std::floor((1.0f + cx) / (static_cast<float>(mTabSize) * spaceSize))) *
                                         (static_cast<float>(mTabSize) * spaceSize);
                                width = x - cx;
                            } else
                            {
                                std::array<char, 2> buf2{};
                                buf2.at(0) = line.at(cindex).mChar;
                                buf2.at(1) = '\0';
                                width = ImGui::GetFont()
                                                ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2.data())
                                                .x;
                            }
                        }
                        const ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
                        const ImVec2 cend(textScreenPos.x + cx + width, lineStartScreenPos.y + mCharAdvance.y);
                        drawList->AddRectFilled(cstart, cend, mPalette.at(static_cast<int>(PaletteIndex::Cursor)));
                        if (elapsed > 800)
                        {
                            mStartTime = timeEnd;
                        }
                    }
                }
            }

            // Render colorized text
            auto prevColor = line.empty() ? mPalette.at(static_cast<int>(PaletteIndex::Default))
                                          : GetGlyphColor(line.at(0));
            ImVec2 bufferOffset;

            for (size_t i = 0; i < line.size();)
            {
                auto &glyph = line.at(i);
                auto color = GetGlyphColor(glyph);

                if ((color != prevColor || glyph.mChar == '\t' || glyph.mChar == ' ') && !mLineBuffer.empty())
                {
                    const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                    drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
                    auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(),
                                                                    FLT_MAX,
                                                                    -1.0f,
                                                                    mLineBuffer.c_str(),
                                                                    nullptr,
                                                                    nullptr);
                    bufferOffset.x += textSize.x;
                    mLineBuffer.clear();
                }
                prevColor = color;

                if (glyph.mChar == '\t')
                {
                    auto oldX = bufferOffset.x;
                    bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) /
                                                        (static_cast<float>(mTabSize) * spaceSize))) *
                                     (static_cast<float>(mTabSize) * spaceSize);
                    ++i;

                    if (mShowWhitespaces)
                    {
                        const auto s = ImGui::GetFontSize();
                        const auto x1 = textScreenPos.x + oldX + 1.0f;
                        const auto x2 = textScreenPos.x + bufferOffset.x - 1.0f;
                        const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
                        const ImVec2 p1(x1, y);
                        const ImVec2 p2(x2, y);
                        const ImVec2 p3(x2 - s * 0.2f, y - s * 0.2f);
                        const ImVec2 p4(x2 - s * 0.2f, y + s * 0.2f);
                        drawList->AddLine(p1, p2, 0x90909090);
                        drawList->AddLine(p2, p3, 0x90909090);
                        drawList->AddLine(p2, p4, 0x90909090);
                    }
                } else if (glyph.mChar == ' ')
                {
                    if (mShowWhitespaces)
                    {
                        const auto s = ImGui::GetFontSize();
                        const auto x = textScreenPos.x + bufferOffset.x + spaceSize * 0.5f;
                        const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
                        drawList->AddCircleFilled(ImVec2(x, y), 1.5f, 0x80808080, 4);
                    }
                    bufferOffset.x += spaceSize;
                    i++;
                } else
                {
                    auto l = UTF8CharLength(glyph.mChar);
                    while (l-- > 0)
                    {
                        mLineBuffer.push_back(line.at(i++).mChar);
                    }
                }
            }

            if (!mLineBuffer.empty())
            {
                const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
                mLineBuffer.clear();
            }

            ++lineNo;
        }

        // Draw a tooltip on known identifiers/preprocessor symbols
        if (ImGui::IsMousePosValid())
        {
            auto id = GetWordAt(ScreenPosToCoordinates(ImGui::GetMousePos()));
            if (!id.empty())
            {
                auto it = mLanguageDefinition.mIdentifiers.find(id);
                if (it != mLanguageDefinition.mIdentifiers.end())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(it->second.mDeclaration.c_str());
                    ImGui::EndTooltip();
                } else
                {
                    auto pi = mLanguageDefinition.mPreprocIdentifiers.find(id);
                    if (pi != mLanguageDefinition.mPreprocIdentifiers.end())
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(pi->second.mDeclaration.c_str());
                        ImGui::EndTooltip();
                    }
                }
            }
        }
    }


    ImGui::Dummy(ImVec2((longest + 2), mLines.size() * mCharAdvance.y));

    if (mScrollToCursor)
    {
        EnsureCursorVisible();
        ImGui::SetWindowFocus();
        mScrollToCursor = false;
    }
}

void TextEditor::Render(const char *aTitle, const ImVec2 &aSize, bool aBorder)
{
    mWithinRender = true;
    mTextChanged = false;
    mCursorPositionChanged = false;

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ImGui::ColorConvertU32ToFloat4(mPalette.at(static_cast<int>(PaletteIndex::Background))));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if (!mIgnoreImGuiChild)
    {
        ImGui::BeginChild(aTitle,
                          aSize,
                          aBorder ? ImGuiChildFlags_Borders : ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar |
                                  ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                                  ImGuiWindowFlags_NoMove);
    }

    if (mHandleKeyboardInputs)
    {
        HandleKeyboardInputs();
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, false);
    }

    if (mHandleMouseInputs)
    {
        HandleMouseInputs();
    }

    ColorizeInternal();
    Render();

    if (mHandleKeyboardInputs)
    {
        ImGui::PopItemFlag();
    }

    if (!mIgnoreImGuiChild)
    {
        ImGui::EndChild();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    mWithinRender = false;
}

void TextEditor::SetText(const std::string &aText)
{
    mLines.clear();
    mLines.emplace_back();
    for (const auto chr: aText)
    {
        if (chr == '\r')
        {
            // ignore the carriage return character
        } else if (chr == '\n')
        {
            mLines.emplace_back();
        } else
        {
            mLines.back().emplace_back(chr, PaletteIndex::Default);
        }
    }

    mTextChanged = true;
    mScrollToTop = true;

    mUndoBuffer.clear();
    mUndoIndex = 0;

    Colorize();
}

void TextEditor::SetTextLines(const std::vector<std::string> &aLines)
{
    mLines.clear();

    if (aLines.empty())
    {
        mLines.emplace_back();
    } else
    {
        mLines.resize(aLines.size());

        for (size_t i = 0; i < aLines.size(); ++i)
        {
            const std::string &aLine = aLines.at(i);

            mLines.at(i).reserve(aLine.size());
            for (const char j: aLine)
            {
                mLines.at(i).emplace_back(j, PaletteIndex::Default);
            }
        }
    }

    mTextChanged = true;
    mScrollToTop = true;

    mUndoBuffer.clear();
    mUndoIndex = 0;

    Colorize();
}

void TextEditor::EnterCharacter(ImWchar aChar, bool aShift)
{
    assert(!mReadOnly);

    UndoRecord u;

    u.mBefore = mState;

    if (HasSelection())
    {
        if (aChar == '\t' && mState.mSelectionStart.mLine != mState.mSelectionEnd.mLine)
        {
            auto start = mState.mSelectionStart;
            auto end = mState.mSelectionEnd;
            auto originalEnd = end;

            if (start > end)
            {
                std::swap(start, end);
            }
            start.mColumn = 0;
            //			end.mColumn = end.mLine < mLines.size() ? mLines[end.mLine].size() : 0;
            if (end.mColumn == 0 && end.mLine > 0)
            {
                --end.mLine;
            }
            if (end.mLine >= static_cast<int>(mLines.size()))
            {
                end.mLine = mLines.empty() ? 0 : static_cast<int>(mLines.size()) - 1;
            }
            end.mColumn = GetLineMaxColumn(end.mLine);

            //if (end.mColumn >= GetLineMaxColumn(end.mLine))
            //	end.mColumn = GetLineMaxColumn(end.mLine) - 1;

            u.mRemovedStart = start;
            u.mRemovedEnd = end;
            u.mRemoved = GetText(start, end);

            bool modified = false;

            for (int i = start.mLine; i <= end.mLine; i++)
            {
                auto &line = mLines.at(i);
                if (aShift)
                {
                    if (!line.empty())
                    {
                        if (line.front().mChar == '\t')
                        {
                            line.erase(line.begin());
                            modified = true;
                        } else
                        {
                            for (int j = 0; j < mTabSize && !line.empty() && line.front().mChar == ' '; j++)
                            {
                                line.erase(line.begin());
                                modified = true;
                            }
                        }
                    }
                } else
                {
                    line.insert(line.begin(), Glyph('\t', PaletteIndex::Background));
                    modified = true;
                }
            }

            if (modified)
            {
                start = Coordinates(start.mLine, GetCharacterColumn(start.mLine, 0));
                Coordinates rangeEnd;
                if (originalEnd.mColumn != 0)
                {
                    end = Coordinates(end.mLine, GetLineMaxColumn(end.mLine));
                    rangeEnd = end;
                    u.mAdded = GetText(start, end);
                } else
                {
                    end = Coordinates(originalEnd.mLine, 0);
                    rangeEnd = Coordinates(end.mLine - 1, GetLineMaxColumn(end.mLine - 1));
                    u.mAdded = GetText(start, rangeEnd);
                }

                u.mAddedStart = start;
                u.mAddedEnd = rangeEnd;
                u.mAfter = mState;

                mState.mSelectionStart = start;
                mState.mSelectionEnd = end;
                AddUndo(u);

                mTextChanged = true;

                EnsureCursorVisible();
            }

            return;
        } // c == '\t'

        u.mRemoved = GetSelectedText();
        u.mRemovedStart = mState.mSelectionStart;
        u.mRemovedEnd = mState.mSelectionEnd;
        DeleteSelection();

    } // HasSelection

    auto coord = GetActualCursorCoordinates();
    u.mAddedStart = coord;

    assert(!mLines.empty());

    if (aChar == '\n')
    {
        (void)InsertLine(coord.mLine + 1);
        auto &line = mLines.at(coord.mLine);
        auto &newLine = mLines.at(coord.mLine + 1);

        if (mLanguageDefinition.mAutoIndentation)
        {
            for (size_t it = 0; it < line.size() && isascii(line.at(it).mChar) && isblank(line.at(it).mChar); ++it)
            {
                newLine.push_back(line.at(it));
            }
        }

        const size_t whitespaceSize = newLine.size();
        auto cindex = GetCharacterIndex(coord);
        newLine.insert(newLine.end(), line.begin() + cindex, line.end());
        line.erase(line.begin() + cindex, line.begin() + line.size());
        SetCursorPosition(Coordinates(coord.mLine + 1,
                                      GetCharacterColumn(coord.mLine + 1, static_cast<int>(whitespaceSize))));
        u.mAdded = static_cast<char>(aChar);
    } else
    {
        char buf[7];
        const int e = ImTextCharToUtf8(buf, 7, aChar);
        if (e > 0)
        {
            buf[e] = '\0';
            auto &line = mLines.at(coord.mLine);
            auto cindex = GetCharacterIndex(coord);

            if (mOverwrite && cindex < static_cast<int>(line.size()))
            {
                auto d = UTF8CharLength(line.at(cindex).mChar);

                u.mRemovedStart = mState.mCursorPosition;
                u.mRemovedEnd = Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + d));

                while (d-- > 0 && cindex < static_cast<int>(line.size()))
                {
                    u.mRemoved += line.at(cindex).mChar;
                    line.erase(line.begin() + cindex);
                }
            }

            for (auto *p = buf; *p != '\0'; p++, ++cindex)
            {
                line.insert(line.begin() + cindex, Glyph(*p, PaletteIndex::Default));
            }
            u.mAdded = buf;

            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex)));
        } else
        {
            return;
        }
    }

    mTextChanged = true;

    u.mAddedEnd = GetActualCursorCoordinates();
    u.mAfter = mState;

    AddUndo(u);

    Colorize(coord.mLine - 1, 3);
    EnsureCursorVisible();
}

void TextEditor::SetReadOnly(const bool aValue)
{
    mReadOnly = aValue;
}

void TextEditor::SetColorizerEnable(const bool aValue)
{
    mColorizerEnabled = aValue;
}

void TextEditor::SetCursorPosition(const Coordinates &aPosition)
{
    if (mState.mCursorPosition != aPosition)
    {
        mState.mCursorPosition = aPosition;
        mCursorPositionChanged = true;
        EnsureCursorVisible();
    }
}

void TextEditor::SetSelectionStart(const Coordinates &aPosition)
{
    mState.mSelectionStart = SanitizeCoordinates(aPosition);
    if (mState.mSelectionStart > mState.mSelectionEnd)
    {
        std::swap(mState.mSelectionStart, mState.mSelectionEnd);
    }
}

void TextEditor::SetSelectionEnd(const Coordinates &aPosition)
{
    mState.mSelectionEnd = SanitizeCoordinates(aPosition);
    if (mState.mSelectionStart > mState.mSelectionEnd)
    {
        std::swap(mState.mSelectionStart, mState.mSelectionEnd);
    }
}

void TextEditor::SetSelection(const Coordinates &aStart, const Coordinates &aEnd, const SelectionMode aMode)
{
    const auto oldSelStart = mState.mSelectionStart;
    const auto oldSelEnd = mState.mSelectionEnd;

    mState.mSelectionStart = SanitizeCoordinates(aStart);
    mState.mSelectionEnd = SanitizeCoordinates(aEnd);
    if (mState.mSelectionStart > mState.mSelectionEnd)
    {
        std::swap(mState.mSelectionStart, mState.mSelectionEnd);
    }

    switch (aMode)
    {
        case SelectionMode::Normal:
            break;
        case SelectionMode::Word:
        {
            mState.mSelectionStart = FindWordStart(mState.mSelectionStart);
            if (!IsOnWordBoundary(mState.mSelectionEnd))
            {
                mState.mSelectionEnd = FindWordEnd(FindWordStart(mState.mSelectionEnd));
            }
            break;
        }
        case SelectionMode::Line:
        {
            const auto lineNo = mState.mSelectionEnd.mLine;
            mState.mSelectionStart = Coordinates(mState.mSelectionStart.mLine, 0);
            mState.mSelectionEnd = Coordinates(lineNo, GetLineMaxColumn(lineNo));
            break;
        }
        default:
            break;
    }

    if (mState.mSelectionStart != oldSelStart || mState.mSelectionEnd != oldSelEnd)
    {
        mCursorPositionChanged = true;
    }
}

void TextEditor::SetTabSize(const int aValue)
{
    mTabSize = std::max(0, std::min(32, aValue));
}

void TextEditor::InsertText(const std::string &aValue)
{
    InsertText(aValue.c_str());
}

void TextEditor::InsertText(const char *aValue)
{
    if (aValue == nullptr)
    {
        return;
    }

    auto pos = GetActualCursorCoordinates();
    const auto start = std::min(pos, mState.mSelectionStart);
    int totalLines = pos.mLine - start.mLine;

    totalLines += InsertTextAt(pos, aValue);

    SetSelection(pos, pos);
    SetCursorPosition(pos);
    Colorize(start.mLine - 1, totalLines + 2);
}

void TextEditor::DeleteSelection()
{
    assert(mState.mSelectionEnd >= mState.mSelectionStart);

    if (mState.mSelectionEnd == mState.mSelectionStart)
    {
        return;
    }

    DeleteRange(mState.mSelectionStart, mState.mSelectionEnd);

    SetSelection(mState.mSelectionStart, mState.mSelectionStart);
    SetCursorPosition(mState.mSelectionStart);
    Colorize(mState.mSelectionStart.mLine, 1);
}

void TextEditor::MoveUp(const int aAmount, const bool aSelect)
{
    const auto oldPos = mState.mCursorPosition;
    mState.mCursorPosition.mLine = std::max(0, mState.mCursorPosition.mLine - aAmount);
    if (oldPos != mState.mCursorPosition)
    {
        if (aSelect)
        {
            if (oldPos == mInteractiveStart)
            {
                mInteractiveStart = mState.mCursorPosition;
            } else if (oldPos == mInteractiveEnd)
            {
                mInteractiveEnd = mState.mCursorPosition;
            } else
            {
                mInteractiveStart = mState.mCursorPosition;
                mInteractiveEnd = oldPos;
            }
        } else
        {
            mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
        }
        SetSelection(mInteractiveStart, mInteractiveEnd);

        EnsureCursorVisible();
    }
}

void TextEditor::MoveDown(const int aAmount, const bool aSelect)
{
    assert(mState.mCursorPosition.mColumn >= 0);
    const auto oldPos = mState.mCursorPosition;
    mState.mCursorPosition.mLine = std::max(0,
                                            std::min(static_cast<int>(mLines.size()) - 1,
                                                     mState.mCursorPosition.mLine + aAmount));

    if (mState.mCursorPosition != oldPos)
    {
        if (aSelect)
        {
            if (oldPos == mInteractiveEnd)
            {
                mInteractiveEnd = mState.mCursorPosition;
            } else if (oldPos == mInteractiveStart)
            {
                mInteractiveStart = mState.mCursorPosition;
            } else
            {
                mInteractiveStart = oldPos;
                mInteractiveEnd = mState.mCursorPosition;
            }
        } else
        {
            mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
        }
        SetSelection(mInteractiveStart, mInteractiveEnd);

        EnsureCursorVisible();
    }
}

static bool IsUTFSequence(const char c)
{
    return (c & 0xC0) == 0x80;
}

void TextEditor::MoveLeft(int aAmount, const bool aSelect, const bool aWordMode)
{
    if (mLines.empty())
    {
        return;
    }

    const auto oldPos = mState.mCursorPosition;
    mState.mCursorPosition = GetActualCursorCoordinates();
    auto line = mState.mCursorPosition.mLine;
    auto cindex = GetCharacterIndex(mState.mCursorPosition);

    while (aAmount-- > 0)
    {
        if (cindex == 0)
        {
            if (line > 0)
            {
                --line;
                if (static_cast<int>(mLines.size()) > line)
                {
                    cindex = static_cast<int>(mLines.at(line).size());
                } else
                {
                    cindex = 0;
                }
            }
        } else
        {
            --cindex;
            if (cindex > 0)
            {
                if (static_cast<int>(mLines.size()) > line)
                {
                    while (cindex > 0 && IsUTFSequence(mLines.at(line).at(cindex).mChar))
                    {
                        --cindex;
                    }
                }
            }
        }

        mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));
        if (aWordMode)
        {
            mState.mCursorPosition = FindWordStart(mState.mCursorPosition);
            cindex = GetCharacterIndex(mState.mCursorPosition);
        }
    }

    mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));

    assert(mState.mCursorPosition.mColumn >= 0);
    if (aSelect)
    {
        if (oldPos == mInteractiveStart)
        {
            mInteractiveStart = mState.mCursorPosition;
        } else if (oldPos == mInteractiveEnd)
        {
            mInteractiveEnd = mState.mCursorPosition;
        } else
        {
            mInteractiveStart = mState.mCursorPosition;
            mInteractiveEnd = oldPos;
        }
    } else
    {
        mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
    }
    SetSelection(mInteractiveStart,
                 mInteractiveEnd,
                 aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

    EnsureCursorVisible();
}

void TextEditor::MoveRight(int aAmount, const bool aSelect, const bool aWordMode)
{
    const auto oldPos = mState.mCursorPosition;

    if (mLines.empty() || static_cast<size_t>(oldPos.mLine) >= mLines.size())
    {
        return;
    }

    auto cindex = GetCharacterIndex(mState.mCursorPosition);
    while (aAmount-- > 0)
    {
        const auto lindex = mState.mCursorPosition.mLine;
        auto &line = mLines.at(lindex);

        if (static_cast<size_t>(cindex) >= line.size())
        {
            if (static_cast<size_t>(mState.mCursorPosition.mLine) < mLines.size() - 1)
            {
                mState.mCursorPosition.mLine = std::max(0,
                                                        std::min(static_cast<int>(mLines.size()) - 1,
                                                                 mState.mCursorPosition.mLine + 1));
                mState.mCursorPosition.mColumn = 0;
            } else
            {
                return;
            }
        } else
        {
            cindex += UTF8CharLength(line.at(cindex).mChar);
            mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));
            if (aWordMode)
            {
                mState.mCursorPosition = FindNextWord(mState.mCursorPosition);
            }
        }
    }

    if (aSelect)
    {
        if (oldPos == mInteractiveEnd)
        {
            mInteractiveEnd = SanitizeCoordinates(mState.mCursorPosition);
        } else if (oldPos == mInteractiveStart)
        {
            mInteractiveStart = mState.mCursorPosition;
        } else
        {
            mInteractiveStart = oldPos;
            mInteractiveEnd = mState.mCursorPosition;
        }
    } else
    {
        mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
    }
    SetSelection(mInteractiveStart,
                 mInteractiveEnd,
                 aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

    EnsureCursorVisible();
}

void TextEditor::MoveTop(const bool aSelect)
{
    const auto oldPos = mState.mCursorPosition;
    SetCursorPosition(Coordinates(0, 0));

    if (mState.mCursorPosition != oldPos)
    {
        if (aSelect)
        {
            mInteractiveEnd = oldPos;
            mInteractiveStart = mState.mCursorPosition;
        } else
        {
            mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
        }
        SetSelection(mInteractiveStart, mInteractiveEnd);
    }
}

void TextEditor::MoveBottom(const bool aSelect)
{
    const auto oldPos = GetCursorPosition();
    const auto newPos = Coordinates(static_cast<int>(mLines.size()) - 1, 0);
    SetCursorPosition(newPos);
    if (aSelect)
    {
        mInteractiveStart = oldPos;
        mInteractiveEnd = newPos;
    } else
    {
        mInteractiveStart = mInteractiveEnd = newPos;
    }
    SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::MoveHome(const bool aSelect)
{
    const auto oldPos = mState.mCursorPosition;
    SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, 0));

    if (mState.mCursorPosition != oldPos)
    {
        if (aSelect)
        {
            if (oldPos == mInteractiveStart)
            {
                mInteractiveStart = mState.mCursorPosition;
            } else if (oldPos == mInteractiveEnd)
            {
                mInteractiveEnd = mState.mCursorPosition;
            } else
            {
                mInteractiveStart = mState.mCursorPosition;
                mInteractiveEnd = oldPos;
            }
        } else
        {
            mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
        }
        SetSelection(mInteractiveStart, mInteractiveEnd);
    }
}

void TextEditor::MoveEnd(const bool aSelect)
{
    const auto oldPos = mState.mCursorPosition;
    SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, GetLineMaxColumn(oldPos.mLine)));

    if (mState.mCursorPosition != oldPos)
    {
        if (aSelect)
        {
            if (oldPos == mInteractiveEnd)
            {
                mInteractiveEnd = mState.mCursorPosition;
            } else if (oldPos == mInteractiveStart)
            {
                mInteractiveStart = mState.mCursorPosition;
            } else
            {
                mInteractiveStart = oldPos;
                mInteractiveEnd = mState.mCursorPosition;
            }
        } else
        {
            mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
        }
        SetSelection(mInteractiveStart, mInteractiveEnd);
    }
}

void TextEditor::Delete()
{
    assert(!mReadOnly);

    if (mLines.empty())
    {
        return;
    }

    UndoRecord u;
    u.mBefore = mState;

    if (HasSelection())
    {
        u.mRemoved = GetSelectedText();
        u.mRemovedStart = mState.mSelectionStart;
        u.mRemovedEnd = mState.mSelectionEnd;

        DeleteSelection();
    } else
    {
        const auto pos = GetActualCursorCoordinates();
        SetCursorPosition(pos);
        auto &line = mLines.at(pos.mLine);

        if (pos.mColumn == GetLineMaxColumn(pos.mLine))
        {
            if (pos.mLine == static_cast<int>(mLines.size()) - 1)
            {
                return;
            }

            u.mRemoved = '\n';
            u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
            Advance(u.mRemovedEnd);

            auto &nextLine = mLines.at(pos.mLine + 1);
            line.insert(line.end(), nextLine.begin(), nextLine.end());
            RemoveLine(pos.mLine + 1);
        } else
        {
            auto cindex = GetCharacterIndex(pos);
            u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
            u.mRemovedEnd.mColumn++;
            u.mRemoved = GetText(u.mRemovedStart, u.mRemovedEnd);

            auto d = UTF8CharLength(line.at(cindex).mChar);
            while (d-- > 0 && cindex < static_cast<int>(line.size()))
            {
                line.erase(line.begin() + cindex);
            }
        }

        mTextChanged = true;

        Colorize(pos.mLine, 1);
    }

    u.mAfter = mState;
    AddUndo(u);
}

void TextEditor::Backspace()
{
    assert(!mReadOnly);

    if (mLines.empty())
    {
        return;
    }

    UndoRecord u;
    u.mBefore = mState;

    if (HasSelection())
    {
        u.mRemoved = GetSelectedText();
        u.mRemovedStart = mState.mSelectionStart;
        u.mRemovedEnd = mState.mSelectionEnd;

        DeleteSelection();
    } else
    {
        const auto pos = GetActualCursorCoordinates();
        SetCursorPosition(pos);

        if (mState.mCursorPosition.mColumn == 0)
        {
            if (mState.mCursorPosition.mLine == 0)
            {
                return;
            }

            u.mRemoved = '\n';
            u.mRemovedStart = u.mRemovedEnd = Coordinates(pos.mLine - 1, GetLineMaxColumn(pos.mLine - 1));
            Advance(u.mRemovedEnd);

            auto &line = mLines.at(mState.mCursorPosition.mLine);
            auto &prevLine = mLines.at(mState.mCursorPosition.mLine - 1);
            const auto prevSize = GetLineMaxColumn(mState.mCursorPosition.mLine - 1);
            prevLine.insert(prevLine.end(), line.begin(), line.end());

            ErrorMarkers etmp;
            for (auto &i: mErrorMarkers)
            {
                etmp.insert(ErrorMarkers::value_type(i.first - 1 == mState.mCursorPosition.mLine ? i.first - 1
                                                                                                 : i.first,
                                                     i.second));
            }
            mErrorMarkers = std::move(etmp);

            RemoveLine(mState.mCursorPosition.mLine);
            --mState.mCursorPosition.mLine;
            mState.mCursorPosition.mColumn = prevSize;
        } else
        {
            auto &line = mLines.at(mState.mCursorPosition.mLine);
            auto cindex = GetCharacterIndex(pos) - 1;
            auto cend = cindex + 1;
            while (cindex > 0 && IsUTFSequence(line.at(cindex).mChar))
            {
                --cindex;
            }

            //if (cindex > 0 && UTF8CharLength(line[cindex].mChar) > 1)
            //	--cindex;

            u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
            --u.mRemovedStart.mColumn;
            --mState.mCursorPosition.mColumn;

            while (static_cast<size_t>(cindex) < line.size() && cend-- > cindex)
            {
                u.mRemoved += line.at(cindex).mChar;
                line.erase(line.begin() + cindex);
            }
        }

        mTextChanged = true;

        EnsureCursorVisible();
        Colorize(mState.mCursorPosition.mLine, 1);
    }

    u.mAfter = mState;
    AddUndo(u);
}

void TextEditor::SelectWordUnderCursor()
{
    const auto c = GetCursorPosition();
    SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll()
{
    SetSelection(Coordinates(0, 0), Coordinates(static_cast<int>(mLines.size()), 0));
}

bool TextEditor::HasSelection() const
{
    return mState.mSelectionEnd > mState.mSelectionStart;
}

void TextEditor::Copy() const
{
    if (HasSelection())
    {
        ImGui::SetClipboardText(GetSelectedText().c_str());
    } else
    {
        if (!mLines.empty())
        {
            std::string str;
            const auto &line = mLines.at(GetActualCursorCoordinates().mLine);
            for (const auto &g: line)
            {
                str.push_back(g.mChar);
            }
            ImGui::SetClipboardText(str.c_str());
        }
    }
}

void TextEditor::Cut()
{
    if (IsReadOnly())
    {
        Copy();
    } else
    {
        if (HasSelection())
        {
            UndoRecord u;
            u.mBefore = mState;
            u.mRemoved = GetSelectedText();
            u.mRemovedStart = mState.mSelectionStart;
            u.mRemovedEnd = mState.mSelectionEnd;

            Copy();
            DeleteSelection();

            u.mAfter = mState;
            AddUndo(u);
        }
    }
}

void TextEditor::Paste()
{
    if (IsReadOnly())
    {
        return;
    }

    const auto *clipText = ImGui::GetClipboardText();
    if (clipText != nullptr && strlen(clipText) > 0)
    {
        UndoRecord u;
        u.mBefore = mState;

        if (HasSelection())
        {
            u.mRemoved = GetSelectedText();
            u.mRemovedStart = mState.mSelectionStart;
            u.mRemovedEnd = mState.mSelectionEnd;
            DeleteSelection();
        }

        u.mAdded = clipText;
        u.mAddedStart = GetActualCursorCoordinates();

        InsertText(clipText);

        u.mAddedEnd = GetActualCursorCoordinates();
        u.mAfter = mState;
        AddUndo(u);
    }
}

bool TextEditor::CanUndo() const
{
    return !mReadOnly && mUndoIndex > 0;
}

bool TextEditor::CanRedo() const
{
    return !mReadOnly && mUndoIndex < static_cast<int>(mUndoBuffer.size());
}

void TextEditor::Undo(int aSteps)
{
    while (CanUndo() && aSteps-- > 0)
    {
        mUndoBuffer.at(--mUndoIndex).Undo(this);
    }
}

void TextEditor::Redo(int aSteps)
{
    while (CanRedo() && aSteps-- > 0)
    {
        mUndoBuffer.at(mUndoIndex++).Redo(this);
    }
}
\
std::string TextEditor::GetText() const
{
    return GetText(Coordinates(), Coordinates(static_cast<int>(mLines.size()), 0));
}

std::vector<std::string> TextEditor::GetTextLines() const
{
    std::vector<std::string> result;

    result.reserve(mLines.size());

    for (const auto &line: mLines)
    {
        std::string text;

        text.resize(line.size());

        for (size_t i = 0; i < line.size(); ++i)
        {
            text.at(i) = line.at(i).mChar;
        }

        result.emplace_back(std::move(text));
    }

    return result;
}

std::string TextEditor::GetSelectedText() const
{
    return GetText(mState.mSelectionStart, mState.mSelectionEnd);
}

std::string TextEditor::GetCurrentLineText() const
{
    const auto lineLength = GetLineMaxColumn(mState.mCursorPosition.mLine);
    return GetText(Coordinates(mState.mCursorPosition.mLine, 0), Coordinates(mState.mCursorPosition.mLine, lineLength));
}

void TextEditor::Colorize(const int aFromLine, const int aLines)
{
    const int toLine = aLines == -1 ? static_cast<int>(mLines.size())
                                    : std::min(static_cast<int>(mLines.size()), aFromLine + aLines);
    mColorRangeMin = std::min(mColorRangeMin, aFromLine);
    mColorRangeMax = std::max(mColorRangeMax, toLine);
    mColorRangeMin = std::max(0, mColorRangeMin);
    mColorRangeMax = std::max(mColorRangeMin, mColorRangeMax);
    mCheckComments = true;
}

void TextEditor::ColorizeRange(const int aFromLine, const int aToLine)
{
    if (mLines.empty() || aFromLine >= aToLine)
    {
        return;
    }

    std::string buffer;
    std::cmatch results;
    std::string id;

    const int endLine = std::max(0, std::min(static_cast<int>(mLines.size()), aToLine));
    for (int i = aFromLine; i < endLine; ++i)
    {
        auto &line = mLines.at(i);

        if (line.empty())
        {
            continue;
        }

        buffer.resize(line.size());
        for (size_t j = 0; j < line.size(); ++j)
        {
            auto &col = line.at(j);
            buffer.at(j) = col.mChar;
            col.mColorIndex = PaletteIndex::Default;
        }

        const char *bufferBegin = &buffer.front();
        const char *bufferEnd = bufferBegin + buffer.size();

        const auto *last = bufferEnd;

        for (const auto *first = bufferBegin; first != last;)
        {
            const char *token_begin = nullptr;
            const char *token_end = nullptr;
            PaletteIndex token_color = PaletteIndex::Default;

            bool hasTokenizeResult = false;

            if (mLanguageDefinition.mTokenize != nullptr)
            {
                if (mLanguageDefinition.mTokenize(first, last, token_begin, token_end, token_color))
                {
                    hasTokenizeResult = true;
                }
            }

            if (!hasTokenizeResult)
            {
                // todo : remove
                //printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

                for (auto &p: mRegexList)
                {
                    if (std::regex_search(first, last, results, p.first, std::regex_constants::match_continuous))
                    {
                        hasTokenizeResult = true;

                        const auto &v = *results.begin();
                        token_begin = v.first;
                        token_end = v.second;
                        token_color = p.second;
                        break;
                    }
                }
            }

            if (!hasTokenizeResult)
            {
                first++;
            } else
            {
                const size_t token_length = token_end - token_begin;

                if (token_color == PaletteIndex::Identifier)
                {
                    id.assign(token_begin, token_end);

                    // todo : almost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
                    if (!mLanguageDefinition.mCaseSensitive)
                    {
                        std::ranges::transform(id, id.begin(), ::toupper);
                    }

                    if (!line.at(first - bufferBegin).mPreprocessor)
                    {
                        if (mLanguageDefinition.mKeywords.contains(id))
                        {
                            token_color = PaletteIndex::Keyword;
                        } else if (mLanguageDefinition.mIdentifiers.contains(id))
                        {
                            token_color = PaletteIndex::KnownIdentifier;
                        } else if (mLanguageDefinition.mPreprocIdentifiers.contains(id))
                        {
                            token_color = PaletteIndex::PreprocIdentifier;
                        }
                    } else
                    {
                        if (mLanguageDefinition.mPreprocIdentifiers.contains(id))
                        {
                            token_color = PaletteIndex::PreprocIdentifier;
                        }
                    }
                }

                for (size_t j = 0; j < token_length; ++j)
                {
                    line.at((token_begin - bufferBegin) + j).mColorIndex = token_color;
                }

                first = token_end;
            }
        }
    }
}

void TextEditor::ColorizeInternal()
{
    if (mLines.empty() || !mColorizerEnabled)
    {
        return;
    }

    if (mCheckComments)
    {
        const auto endLine = mLines.size();
        constexpr auto endIndex = 0;
        auto commentStartLine = endLine;
        auto commentStartIndex = endIndex;
        auto withinString = false;
        auto withinSingleLineComment = false;
        auto withinPreproc = false;
        auto firstChar = true; // there is no other non-whitespace characters in the line before
        auto concatenate = false; // '\' on the very end of the line
        auto currentLine = 0;
        auto currentIndex = 0;
        while (std::cmp_less(currentLine, endLine) || currentIndex < endIndex)
        {
            auto &line = mLines.at(currentLine);

            if (currentIndex == 0 && !concatenate)
            {
                withinSingleLineComment = false;
                withinPreproc = false;
                firstChar = true;
            }

            concatenate = false;

            if (!line.empty())
            {
                const auto &g = line.at(currentIndex);
                const auto c = g.mChar;

                if (c != mLanguageDefinition.mPreprocChar && (isspace(c) == 0))
                {
                    firstChar = false;
                }

                if (currentIndex == static_cast<int>(line.size()) - 1 && line.at(line.size() - 1).mChar == '\\')
                {
                    concatenate = true;
                }

                bool inComment = (std::cmp_less(commentStartLine , currentLine) ||
                                  (std::cmp_equal(commentStartLine , currentLine) && commentStartIndex <= currentIndex));

                if (withinString)
                {
                    line.at(currentIndex).mMultiLineComment = inComment;

                    if (c == '\"')
                    {
                        if (currentIndex + 1 < static_cast<int>(line.size()) && line.at(currentIndex + 1).mChar == '\"')
                        {
                            currentIndex += 1;
                            if (currentIndex < static_cast<int>(line.size()))
                            {
                                line.at(currentIndex).mMultiLineComment = inComment;
                            }
                        } else
                        {
                            withinString = false;
                        }
                    } else if (c == '\\')
                    {
                        currentIndex += 1;
                        if (currentIndex < static_cast<int>(line.size()))
                        {
                            line.at(currentIndex).mMultiLineComment = inComment;
                        }
                    }
                } else
                {
                    if (firstChar && c == mLanguageDefinition.mPreprocChar)
                    {
                        withinPreproc = true;
                    }

                    if (c == '\"')
                    {
                        withinString = true;
                        line.at(currentIndex).mMultiLineComment = inComment;
                    } else
                    {
                        auto pred = [](const char &a, const Glyph &b) {
                            return a == b.mChar;
                        };
                        auto from = line.begin() + currentIndex;
                        auto &startStr = mLanguageDefinition.mCommentStart;
                        auto &singleStartStr = mLanguageDefinition.mSingleLineComment;

                        if (!singleStartStr.empty() &&
                            currentIndex + singleStartStr.size() <= line.size() &&
                            equals(singleStartStr.begin(),
                                   singleStartStr.end(),
                                   from,
                                   from + singleStartStr.size(),
                                   pred))
                        {
                            withinSingleLineComment = true;
                        } else if (!withinSingleLineComment &&
                                   currentIndex + startStr.size() <= line.size() &&
                                   equals(startStr.begin(), startStr.end(), from, from + startStr.size(), pred))
                        {
                            commentStartLine = currentLine;
                            commentStartIndex = currentIndex;
                        }

                        inComment = inComment = (std::cmp_less(commentStartLine , currentLine) || (std::cmp_equal(commentStartLine , currentLine) &&
                                                                                    commentStartIndex <= currentIndex));

                        line.at(currentIndex).mMultiLineComment = inComment;
                        line.at(currentIndex).mComment = withinSingleLineComment;

                        auto &endStr = mLanguageDefinition.mCommentEnd;
                        if (currentIndex + 1 >= static_cast<int>(endStr.size()) &&
                            equals(endStr.begin(), endStr.end(), from + 1 - endStr.size(), from + 1, pred))
                        {
                            commentStartIndex = endIndex;
                            commentStartLine = endLine;
                        }
                    }
                }
                line.at(currentIndex).mPreprocessor = withinPreproc;
                currentIndex += UTF8CharLength(c);
                if (currentIndex >= static_cast<int>(line.size()))
                {
                    currentIndex = 0;
                    ++currentLine;
                }
            } else
            {
                currentIndex = 0;
                ++currentLine;
            }
        }
        mCheckComments = false;
    }

    if (mColorRangeMin < mColorRangeMax)
    {
        const int increment = (mLanguageDefinition.mTokenize == nullptr) ? 10 : 10000;
        const int to = std::min(mColorRangeMin + increment, mColorRangeMax);
        ColorizeRange(mColorRangeMin, to);
        mColorRangeMin = to;

        if (mColorRangeMax == mColorRangeMin)
        {
            mColorRangeMin = std::numeric_limits<int>::max();
            mColorRangeMax = 0;
        }
        return;
    }
}

float TextEditor::TextDistanceToLineStart(const Coordinates &aFrom) const
{
    const auto &line = mLines.at(aFrom.mLine);
    float distance = 0.0f;
    const float
            spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
    const int colIndex = GetCharacterIndex(aFrom);
    for (size_t it = 0u; it < line.size() && it < colIndex;)
    {
        if (line.at(it).mChar == '\t')
        {
            distance = (1.0f + std::floor((1.0f + distance) / (static_cast<float>(mTabSize) * spaceSize))) *
                       (static_cast<float>(mTabSize) * spaceSize);
            ++it;
        } else
        {
            auto d = UTF8CharLength(line.at(it).mChar);
            std::array<char, 7> tempCString{};
            int i = 0;
            for (; i < 6 && d-- > 0 && it < static_cast<int>(line.size()); i++, it++)
            {
                tempCString.at(i) = line.at(it).mChar;
            }

            tempCString.at(i) = '\0';
            distance += ImGui::GetFont()
                                ->CalcTextSizeA(ImGui::GetFontSize(),
                                                FLT_MAX,
                                                -1.0f,
                                                tempCString.data(),
                                                nullptr,
                                                nullptr)
                                .x;
        }
    }

    return distance;
}

void TextEditor::EnsureCursorVisible()
{
    if (!mWithinRender)
    {
        mScrollToCursor = true;
        return;
    }

    const float scrollX = ImGui::GetScrollX();
    const float scrollY = ImGui::GetScrollY();

    const auto height = ImGui::GetWindowHeight();
    const auto width = ImGui::GetWindowWidth();

    const auto top = 1 + static_cast<int>(ceil(scrollY / mCharAdvance.y));
    const auto bottom = static_cast<int>(ceil((scrollY + height) / mCharAdvance.y));

    const auto left = static_cast<int>(ceil(scrollX / mCharAdvance.x));
    const auto right = static_cast<int>(ceil((scrollX + width) / mCharAdvance.x));

    const auto pos = GetActualCursorCoordinates();
    const auto len = TextDistanceToLineStart(pos);

    if (pos.mLine < top)
    {
        ImGui::SetScrollY(std::max(0.0f, (pos.mLine - 1) * mCharAdvance.y));
    }
    if (pos.mLine > bottom - 4)
    {
        ImGui::SetScrollY(std::max(0.0f, (pos.mLine + 4) * mCharAdvance.y - height));
    }
    if (len + mTextStart < left + 4)
    {
        ImGui::SetScrollX(std::max(0.0f, len + mTextStart - 4));
    }
    if (len + mTextStart > right - 4)
    {
        ImGui::SetScrollX(std::max(0.0f, len + mTextStart + 4 - width));
    }
}

int TextEditor::GetPageSize() const
{
    const auto height = ImGui::GetWindowHeight() - 20.0f;
    return static_cast<int>(floor(height / mCharAdvance.y));
}

TextEditor::UndoRecord::UndoRecord(std::string aAdded,
                                   const Coordinates aAddedStart,
                                   const Coordinates aAddedEnd,
                                   std::string aRemoved,
                                   const Coordinates aRemovedStart,
                                   const Coordinates aRemovedEnd,
                                   const EditorState &aBefore,
                                   const EditorState &aAfter):
    mAdded(std::move(aAdded)),
    mAddedStart(aAddedStart),
    mAddedEnd(aAddedEnd),
    mRemoved(std::move(aRemoved)),
    mRemovedStart(aRemovedStart),
    mRemovedEnd(aRemovedEnd),
    mBefore(aBefore),
    mAfter(aAfter)
{
    assert(mAddedStart <= mAddedEnd);
    assert(mRemovedStart <= mRemovedEnd);
}

void TextEditor::UndoRecord::Undo(TextEditor *aEditor) const
{
    if (!mAdded.empty())
    {
        aEditor->DeleteRange(mAddedStart, mAddedEnd);
        aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 2);
    }

    if (!mRemoved.empty())
    {
        auto start = mRemovedStart;
        (void)aEditor->InsertTextAt(start, mRemoved.c_str());
        aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 2);
    }

    aEditor->mState = mBefore;
    aEditor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor *aEditor) const
{
    if (!mRemoved.empty())
    {
        aEditor->DeleteRange(mRemovedStart, mRemovedEnd);
        aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 1);
    }

    if (!mAdded.empty())
    {
        auto start = mAddedStart;
        (void)aEditor->InsertTextAt(start, mAdded.c_str());
        aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 1);
    }

    aEditor->mState = mAfter;
    aEditor->EnsureCursorVisible();
}