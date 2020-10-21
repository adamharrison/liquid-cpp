#ifndef LEXER_H
#define LEXER_H

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace Liquid {
    struct Context;

    template <class T>
    struct Lexer {
        const Context& context;

        enum class State {
            INITIAL,
            RAW,
            CONTROL,
            OUTPUT
        };

        size_t column = 0;
        size_t row = 1;
        State state = State::INITIAL;

        bool startOutputBlock(bool suppress) { return true; }
        bool endOutputBlock(bool suppress) { return true; }

        bool literal(const char* str, size_t len) { return true; }
        bool string(const char* str, size_t len) { return true; }
        bool integer(long long i) { return true; }
        bool floating(double f) { return true; }
        bool dot() { return true; }
        bool comma() { return true; }
        bool colon() { return true; }
        bool startVariableDereference() { return true; }
        bool endVariableDereference() { return true; }

        bool startControlBlock(bool suppress) { return true; }
        bool endControlBlock(bool suppress) { return true; }

        bool openParenthesis() { return true; }
        bool closeParenthesis() { return true; }

        bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n'; }

        Lexer(const Context& context) : context(context) { }
        ~Lexer() { }

        // Must be a whole file, for now. Should be null-terminated.
        void parse(const char* str, size_t size) {
            size_t offset = 0;
            size_t lastInitial = 0;
            bool ongoing = true;
            while (ongoing && offset < size) {
                ++column;
                switch (state) {
                    case State::INITIAL:
                        switch (str[offset]) {
                            case '\n': {
                                ++row;
                                column = 0;
                            } break;
                            case '{': {
                                if (str[offset-1] == '{') {
                                    if (offset - lastInitial - 1 > 0)
                                        static_cast<T*>(this)->literal(&str[lastInitial], offset - lastInitial - 1);
                                    if (offset-1 < size && str[offset+1] == '-') {
                                        ongoing = static_cast<T*>(this)->startOutputBlock(true);
                                        ++offset;
                                    } else
                                        ongoing = static_cast<T*>(this)->startOutputBlock(false);
                                    state = State::OUTPUT;
                                }
                            } break;
                            case '%': {
                                if (str[offset-1] == '{') {
                                    if (offset-1 < size && str[offset+1] == '-') {
                                        ongoing = static_cast<T*>(this)->startControlBlock(true);
                                        ++offset;
                                    } else
                                        ongoing = static_cast<T*>(this)->startControlBlock(false);
                                    state = State::CONTROL;
                                }
                            } break;
                        }
                        ++offset;
                    break;
                    case State::OUTPUT:
                    case State::CONTROL: {
                        size_t nonWhitespaceCharacter, endOfWord, ongoingWord;
                        for (nonWhitespaceCharacter = offset; isWhitespace(str[nonWhitespaceCharacter]) && nonWhitespaceCharacter < size; ++nonWhitespaceCharacter);
                        switch (str[nonWhitespaceCharacter]) {
                            case '"': {
                                for (endOfWord = nonWhitespaceCharacter+1; endOfWord < size && (str[endOfWord] == '\\' || str[endOfWord] != '"'); ++endOfWord);
                                ongoing = static_cast<T*>(this)->string(&str[nonWhitespaceCharacter+1], endOfWord - nonWhitespaceCharacter - 1);
                                offset = endOfWord;
                            } break;
                            case '\'': {
                                for (endOfWord = nonWhitespaceCharacter+1; endOfWord < size && (str[endOfWord] == '\\' || str[endOfWord] != '\''); ++endOfWord);
                                ongoing = static_cast<T*>(this)->string(&str[nonWhitespaceCharacter+1], endOfWord - nonWhitespaceCharacter - 1);
                                offset = endOfWord;
                            } break;
                            default: {
                                bool isNumber = true;
                                bool hasPoint = false;
                                ongoingWord = nonWhitespaceCharacter;
                                for (endOfWord = nonWhitespaceCharacter; endOfWord < size && !isWhitespace(str[endOfWord]); ++endOfWord) {
                                    switch (str[endOfWord]) {
                                        case '-':
                                            if (endOfWord != nonWhitespaceCharacter)
                                                isNumber = false;
                                        break;
                                        case '[':
                                        case ']':
                                        case '(':
                                        case ')':
                                        case '.':
                                        case ':':
                                        case ',':
                                            if (!isNumber) {
                                                ongoing = static_cast<T*>(this)->literal(&str[nonWhitespaceCharacter], endOfWord - ongoingWord + 1);
                                                if (ongoing) {
                                                    switch (str[endOfWord]) {
                                                        case '.': ongoing = static_cast<T*>(this)->dot(); break;
                                                        case '[': ongoing = static_cast<T*>(this)->startVariableDereference(); break;
                                                        case ']': ongoing = static_cast<T*>(this)->endVariableDereference(); break;
                                                        case '(': ongoing = static_cast<T*>(this)->openParenthesis(); break;
                                                        case ')': ongoing = static_cast<T*>(this)->closeParenthesis(); break;
                                                        case ':': ongoing = static_cast<T*>(this)->colon(); break;
                                                        case ',': ongoing = static_cast<T*>(this)->comma(); break;
                                                    }
                                                }
                                                ongoingWord = endOfWord+1;
                                            } else {
                                                if (hasPoint)
                                                    isNumber = false;
                                                else
                                                    hasPoint = true;
                                            }
                                        break;
                                        break;
                                        case '0':
                                        case '1':
                                        case '2':
                                        case '3':
                                        case '4':
                                        case '5':
                                        case '6':
                                        case '7':
                                        case '8':
                                        case '9':
                                        break;
                                        default:
                                            isNumber = false;
                                        break;
                                        case '}': {
                                            if (state == State::CONTROL) {
                                                if (str[endOfWord-1] == '%') {
                                                    ongoing = static_cast<T*>(this)->endControlBlock(str[endOfWord-2] == '-');
                                                    state = State::INITIAL;
                                                    lastInitial = endOfWord+1;
                                                }
                                            } else {
                                                if (str[endOfWord-1] == '}') {
                                                    ongoing = static_cast<T*>(this)->endOutputBlock(str[endOfWord-2] == '-');
                                                    state = State::INITIAL;
                                                    lastInitial = endOfWord+1;
                                                }
                                            }
                                        } break;
                                    }
                                }
                                if (state != State::INITIAL) {
                                    if (isNumber) {
                                        char pulled = 0;
                                        if (endOfWord < offset) {
                                            pulled = str[endOfWord];
                                            const_cast<char*>(str)[endOfWord] = 0;
                                        }
                                        if (hasPoint) {
                                            ongoing = static_cast<T*>(this)->floating(atof(&str[nonWhitespaceCharacter]));
                                        } else {
                                            ongoing = static_cast<T*>(this)->integer(atoll(&str[nonWhitespaceCharacter]));
                                        }
                                        if (endOfWord < offset)
                                            const_cast<char*>(str)[endOfWord] = pulled;
                                    } else {
                                        if (strncmp(&str[nonWhitespaceCharacter], "raw", 3) == 0) {
                                            state = State::RAW;
                                        } else {
                                            ongoing = static_cast<T*>(this)->literal(&str[nonWhitespaceCharacter], endOfWord - nonWhitespaceCharacter);
                                        }
                                    }
                                }
                                offset = endOfWord;
                            } break;
                        }
                    } break;
                    break;
                    case State::RAW: {

                    } break;
                }
            }
            assert(state == State::INITIAL);
            if (offset > lastInitial) {
                static_cast<T*>(this)->literal(&str[lastInitial], offset - lastInitial);
            }
        }
    };
}

#endif
