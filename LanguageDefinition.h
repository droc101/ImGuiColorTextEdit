//
// Created by droc101 on 2/11/26.
//

#pragma once
#include <string>
#include "Types.h"

class LanguageDefinition
{
    public:
        std::string mName;
        Keywords mKeywords;
        Identifiers mIdentifiers;
        Identifiers mPreprocIdentifiers;
        std::string mCommentStart, mCommentEnd, mSingleLineComment;
        char mPreprocChar = '#';
        bool mAutoIndentation = true;

        TokenizeCallback mTokenize = nullptr;

        TokenRegexStrings mTokenRegexStrings;

        bool mCaseSensitive = true;

        LanguageDefinition() = default;

        static const LanguageDefinition &GLSL();
        static const LanguageDefinition &AngelScript();
};
