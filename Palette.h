//
// Created by droc101 on 2/11/26.
//

#pragma once

#include <array>
#include <cstdint>
#include "imgui.h"

enum class PaletteIndex : uint8_t
{
    Default,
    Keyword,
    Number,
    String,
    CharLiteral,
    Punctuation,
    Preprocessor,
    Identifier,
    KnownIdentifier,
    PreprocIdentifier,
    Comment,
    MultiLineComment,
    Background,
    Cursor,
    Selection,
    ErrorMarker,
    Breakpoint,
    LineNumber,
    CurrentLineFill,
    CurrentLineFillInactive,
    CurrentLineEdge,
    Max
};

using Palette = std::array<ImU32, static_cast<unsigned>(PaletteIndex::Max)>;

const Palette &GetDarkPalette();
const Palette &GetLightPalette();
