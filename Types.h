//
// Created by droc101 on 2/11/26.
//

#pragma once
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "Palette.h"

using String = std::string;
using ErrorMarkers = std::map<int, std::string>;
using Breakpoints = std::unordered_set<int>;
using Char = uint8_t;
using Keywords = std::unordered_set<std::string>;

enum class SelectionMode : uint8_t
{
    Normal,
    Word,
    Line
};

struct Breakpoint
{
        int mLine = -1;
        bool mEnabled = false;
        std::string mCondition{};
};

// Represents a character coordinate from the user's point of view,
// i. e. consider an uniform grid (assuming fixed-width font) on the
// screen as it is rendered, and each cell has its own coordinate, starting from 0.
// Tabs are counted as [1..mTabSize] count empty spaces, depending on
// how many space is necessary to reach the next tab stop.
// For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when mTabSize = 4,
// because it is rendered as "    ABC" on the screen.
struct Coordinates
{
        int mLine, mColumn;
        Coordinates(): mLine(0), mColumn(0) {}
        Coordinates(const int aLine, const int aColumn): mLine(aLine), mColumn(aColumn)
        {
            assert(aLine >= 0);
            assert(aColumn >= 0);
        }
        static Coordinates Invalid()
        {
            static const Coordinates invalid(-1, -1);
            return invalid;
        }

        bool operator==(const Coordinates &o) const
        {
            return mLine == o.mLine && mColumn == o.mColumn;
        }

        bool operator!=(const Coordinates &o) const
        {
            return mLine != o.mLine || mColumn != o.mColumn;
        }

        bool operator<(const Coordinates &o) const
        {
            if (mLine != o.mLine)
            {
                return mLine < o.mLine;
            }
            return mColumn < o.mColumn;
        }

        bool operator>(const Coordinates &o) const
        {
            if (mLine != o.mLine)
            {
                return mLine > o.mLine;
            }
            return mColumn > o.mColumn;
        }

        bool operator<=(const Coordinates &o) const
        {
            if (mLine != o.mLine)
            {
                return mLine < o.mLine;
            }
            return mColumn <= o.mColumn;
        }

        bool operator>=(const Coordinates &o) const
        {
            if (mLine != o.mLine)
            {
                return mLine > o.mLine;
            }
            return mColumn >= o.mColumn;
        }
};

struct Identifier
{
        Coordinates mLocation{};
        std::string mDeclaration{};
};

struct Glyph
{
        Char mChar;
        PaletteIndex mColorIndex = PaletteIndex::Default;
        bool mComment : 1 = false;
        bool mMultiLineComment : 1 = false;
        bool mPreprocessor : 1 = false;

        Glyph(const Char aChar, const PaletteIndex aColorIndex): mChar(aChar), mColorIndex(aColorIndex) {}
};

using Identifiers = std::unordered_map<std::string, Identifier>;
using TokenRegexString = std::pair<std::string, PaletteIndex>;
using TokenRegexStrings = std::vector<TokenRegexString>;
using TokenizeCallback = bool (*)(const char *in_begin,
                                  const char *in_end,
                                  const char *&out_begin,
                                  const char *&out_end,
                                  PaletteIndex &paletteIndex);
using Line = std::vector<Glyph>;
using Lines = std::vector<Line>;
