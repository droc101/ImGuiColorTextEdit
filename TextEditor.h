#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <regex>
#include <string>
#include <utility>
#include <vector>
#include "imgui.h"
#include "LanguageDefinition.h"
#include "Palette.h"
#include "Types.h"

class TextEditor
{
    public:
        TextEditor();
        ~TextEditor() = default;

        void SetLanguageDefinition(const LanguageDefinition &aLanguageDef);
        const LanguageDefinition &GetLanguageDefinition() const
        {
            return mLanguageDefinition;
        }

        const Palette &GetPalette() const
        {
            return mPaletteBase;
        }
        void SetPalette(const Palette &aValue);

        void SetErrorMarkers(const ErrorMarkers &aMarkers)
        {
            mErrorMarkers = aMarkers;
        }
        void SetBreakpoints(const Breakpoints &aMarkers)
        {
            mBreakpoints = aMarkers;
        }

        void Render(const char *aTitle, const ImVec2 &aSize = ImVec2(), bool aBorder = false);
        void SetText(const std::string &aText);
        std::string GetText() const;

        void SetTextLines(const std::vector<std::string> &aLines);
        std::vector<std::string> GetTextLines() const;

        std::string GetSelectedText() const;
        std::string GetCurrentLineText() const;

        int GetTotalLines() const
        {
            return static_cast<int>(mLines.size());
        }
        bool IsOverwrite() const
        {
            return mOverwrite;
        }

        void SetReadOnly(bool aValue);
        bool IsReadOnly() const
        {
            return mReadOnly;
        }
        bool IsTextChanged() const
        {
            return mTextChanged;
        }
        bool IsCursorPositionChanged() const
        {
            return mCursorPositionChanged;
        }

        bool IsColorizerEnabled() const
        {
            return mColorizerEnabled;
        }
        void SetColorizerEnable(bool aValue);

        Coordinates GetCursorPosition() const
        {
            return GetActualCursorCoordinates();
        }
        void SetCursorPosition(const Coordinates &aPosition);

        void SetHandleMouseInputs(const bool aValue)
        {
            mHandleMouseInputs = aValue;
        }
        bool IsHandleMouseInputsEnabled() const
        {
            return mHandleKeyboardInputs;
        }

        void SetHandleKeyboardInputs(const bool aValue)
        {
            mHandleKeyboardInputs = aValue;
        }
        bool IsHandleKeyboardInputsEnabled() const
        {
            return mHandleKeyboardInputs;
        }

        void SetImGuiChildIgnored(const bool aValue)
        {
            mIgnoreImGuiChild = aValue;
        }
        bool IsImGuiChildIgnored() const
        {
            return mIgnoreImGuiChild;
        }

        void SetShowWhitespaces(const bool aValue)
        {
            mShowWhitespaces = aValue;
        }
        bool IsShowingWhitespaces() const
        {
            return mShowWhitespaces;
        }

        void SetTabSize(int aValue);

        int GetTabSize() const
        {
            return mTabSize;
        }

        void InsertText(const std::string &aValue);
        void InsertText(const char *aValue);

        void MoveUp(int aAmount = 1, bool aSelect = false);
        void MoveDown(int aAmount = 1, bool aSelect = false);
        void MoveLeft(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
        void MoveRight(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
        void MoveTop(bool aSelect = false);
        void MoveBottom(bool aSelect = false);
        void MoveHome(bool aSelect = false);
        void MoveEnd(bool aSelect = false);

        void SetSelectionStart(const Coordinates &aPosition);
        void SetSelectionEnd(const Coordinates &aPosition);
        void SetSelection(const Coordinates &aStart,
                          const Coordinates &aEnd,
                          SelectionMode aMode = SelectionMode::Normal);
        void SelectWordUnderCursor();
        void SelectAll();
        bool HasSelection() const;

        void Copy() const;
        void Cut();
        void Paste();
        void Delete();

        bool CanUndo() const;
        bool CanRedo() const;
        void Undo(int aSteps = 1);
        void Redo(int aSteps = 1);

    private:
        using RegexList = std::vector<std::pair<std::regex, PaletteIndex>>;

        struct EditorState
        {
                Coordinates mSelectionStart;
                Coordinates mSelectionEnd;
                Coordinates mCursorPosition;
        };

        class UndoRecord
        {
            public:
                UndoRecord() = default;
                ~UndoRecord() = default;

                UndoRecord(std::string aAdded,
                           Coordinates aAddedStart,
                           Coordinates aAddedEnd,

                           std::string aRemoved,
                           Coordinates aRemovedStart,
                           Coordinates aRemovedEnd,

                           const EditorState &aBefore,
                           const EditorState &aAfter);

                void Undo(TextEditor *aEditor) const;
                void Redo(TextEditor *aEditor) const;

                std::string mAdded;
                Coordinates mAddedStart;
                Coordinates mAddedEnd;

                std::string mRemoved;
                Coordinates mRemovedStart;
                Coordinates mRemovedEnd;

                EditorState mBefore;
                EditorState mAfter;
        };

        using UndoBuffer = std::vector<UndoRecord>;

        void Colorize(int aFromLine = 0, int aCount = -1);
        void ColorizeRange(int aFromLine = 0, int aToLine = 0);
        void ColorizeInternal();
        float TextDistanceToLineStart(const Coordinates &aFrom) const;
        void EnsureCursorVisible();
        int GetPageSize() const;
        std::string GetText(const Coordinates &aStart, const Coordinates &aEnd) const;
        Coordinates GetActualCursorCoordinates() const;
        ImVec2 GetCursorScreenPosition() const;
        Coordinates SanitizeCoordinates(const Coordinates &aValue) const;
        void Advance(Coordinates &aCoordinates) const;
        void DeleteRange(const Coordinates &aStart, const Coordinates &aEnd);
        int InsertTextAt(Coordinates &aWhere, const char *aValue);
        void AddUndo(const UndoRecord &aValue);
        Coordinates ScreenPosToCoordinates(const ImVec2 &aPosition) const;
        Coordinates FindWordStart(const Coordinates &aFrom) const;
        Coordinates FindWordEnd(const Coordinates &aFrom) const;
        Coordinates FindNextWord(const Coordinates &aFrom) const;
        int GetCharacterIndex(const Coordinates &aCoordinates) const;
        int GetCharacterColumn(int aLine, int aIndex) const;
        int GetLineCharacterCount(int aLine) const;
        int GetLineMaxColumn(int aLine) const;
        bool IsOnWordBoundary(const Coordinates &aAt) const;
        void RemoveLine(int aStart, int aEnd);
        void RemoveLine(int aIndex);
        Line &InsertLine(int aIndex);
        void EnterCharacter(ImWchar aChar, bool aShift);
        void Backspace();
        void DeleteSelection();
        std::string GetWordUnderCursor() const;
        std::string GetWordAt(const Coordinates &aCoords) const;
        ImU32 GetGlyphColor(const Glyph &aGlyph) const;

        void HandleKeyboardInputs();
        void HandleMouseInputs();
        void Render();

        float mLineSpacing;
        Lines mLines;
        EditorState mState;
        UndoBuffer mUndoBuffer;
        int mUndoIndex;

        int mTabSize;
        bool mOverwrite;
        bool mReadOnly;
        bool mWithinRender;
        bool mScrollToCursor;
        bool mScrollToTop;
        bool mTextChanged;
        bool mColorizerEnabled;
        float mTextStart; // position (in pixels) where a code line starts relative to the left of the TextEditor.
        int mLeftMargin;
        bool mCursorPositionChanged;
        int mColorRangeMin, mColorRangeMax;
        SelectionMode mSelectionMode;
        bool mHandleKeyboardInputs;
        bool mHandleMouseInputs;
        bool mIgnoreImGuiChild;
        bool mShowWhitespaces;

        Palette mPaletteBase{};
        Palette mPalette{};
        LanguageDefinition mLanguageDefinition;
        RegexList mRegexList;

        bool mCheckComments;
        Breakpoints mBreakpoints;
        ErrorMarkers mErrorMarkers;
        ImVec2 mCharAdvance;
        Coordinates mInteractiveStart, mInteractiveEnd;
        std::string mLineBuffer;
        uint64_t mStartTime;

        float mLastClick;
};
