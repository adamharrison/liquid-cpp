#ifndef LIQUIDLEXER_H
#define LIQUIDLEXER_H

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


        struct Error : LiquidLexerError {
            typedef LiquidLexerErrorType Type;

            Error() {
                type = Type::LIQUID_LEXER_ERROR_TYPE_NONE;
            }
            Error(const Error& error) = default;
            Error(Error&& error) = default;
            Error(Lexer& lexer, Type type, const std::string& message = "") {
                details.line = lexer.line;
                details.column = lexer.column;
                this->type = type;
                strncpy(details.args[0], message.data(), LIQUID_ERROR_ARG_MAX_LENGTH-1);
                details.args[0][LIQUID_ERROR_ARG_MAX_LENGTH-1] = 0;
            }

            operator bool() const { return type != Type::LIQUID_LEXER_ERROR_TYPE_NONE; }

            static string english(const LiquidLexerError& error) {
                char buffer[512] = { 0 };
                switch (error.type) {
                    case Type::LIQUID_LEXER_ERROR_TYPE_NONE: break;
                    case Type::LIQUID_LEXER_ERROR_TYPE_UNEXPECTED_END:
                        if (error.details.args[0])
                            sprintf(buffer, "Unexpected end to block '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                        else
                            sprintf(buffer, "Unexpected end to block on line %lu, column %lu.", error.details.line, error.details.column);
                    break;
                }
                return std::string(buffer);
            }
        };


        size_t column;
        size_t line;
        State state;

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

        bool isWhitespace(char c) { return isspace(c); }
        bool isWhitespace(unsigned int c) {
            switch (c) {
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                case 0xc2a0:
                case 0xe19a80:
                case 0xe28080:
                case 0xe28081:
                case 0xe28082:
                case 0xe28083:
                case 0xe28084:
                case 0xe28085:
                case 0xe28086:
                case 0xe28087:
                case 0xe28088:
                case 0xe28089:
                case 0xe2808a:
                case 0xe2808b:
                case 0xe280af:
                case 0xe2819f:
                case 0xe38080:
                    return true;
            }
            return false;
        }

        bool isUTF8Character(const char* offset) {
            return *offset & 0xC0;
        }

        unsigned int getUTF8Character(const char* offset, const char* end, int* length) {
            int bytes = 1;
            int c = *offset++;
            while ((*offset & 0xC0) == 0x80) {
                if (++offset >= end)
                    break;
                c |= *offset << (8*++bytes);
            }
            *length = bytes;
            return c;
        }

        // 0 for no, whitespace, if multi-width utf8 character, returns width, otherwise 1.
        int isWhitespace(const char* offset, const char* end) {
            if (isUTF8Character(offset)) {
                int length;
                unsigned int character = getUTF8Character(offset, end, &length);
                return isWhitespace(character) ? length : 0;
            }
            return isWhitespace(*offset);
        }

        // Gets the closest actual character, starting on the first character. Doesn't ever go past start.
        // Given that this is being embedded in large interpreters like perl and ruby; I have no idea what's going to happen
        // if I just go setting the C locale willy-nilly.
        // I also don't want to re-write to use C++ strings.
        // So, I'm literally just going to parse in and look at the 25 or so UTF-8 condepoints that correspond to spaces.
        // God help me.
        const char* previousBoundary(const char* start, const char* offset)  {
            while (offset > start) {
                if (isWhitespace(*offset))
                    --offset;
                else if (isUTF8Character(offset)) {
                    // Assuming that we're a UTF-8 multibyte string. Currently, that's the only thing we support.
                    int bytes = 0;
                    unsigned int c = 0;
                    while ((*offset & 0xC0) == 0x80) {
                        c |= ((unsigned char)*offset) << (8*bytes++);
                        if (--offset < start)
                            return start;
                    }
                    c |= ((unsigned char)*offset) << (8*bytes++);
                    if (!isWhitespace(c))
                        return offset;
                    --offset;
                } else {
                    return offset;
                }
            }
            return start;
        }

        const char* nextBoundary(const char* offset, const char* end) {
            while (offset < end) {
                int size = isWhitespace(offset, end);
                if (size == 0)
                    return offset;
                offset += size;
            }
            return offset;
        }

        Lexer(const Context& context) : context(context) { }
        ~Lexer() { }

        bool processControlChunk(const char* chunk, size_t size, bool isNumber, bool hasPoint) {
            if (size > 0) {
                if (!isNumber || (size == 1 && chunk[0] == '-'))
                    return static_cast<T*>(this)->literal(chunk, size);
                else if (hasPoint)
                    return static_cast<T*>(this)->floating(atof(chunk));
                return static_cast<T*>(this)->integer(atoll(chunk));
            }
            return true;
        }

        // Must be a whole file, for now. Should be null-terminated. Treats it as UTF8.
        Error parse(const char* str, size_t size, Lexer::State initialState = State::INITIAL) {
            size_t offset = 0;
            size_t lastInitial = 0;
            size_t i;
            bool ongoing = true;
            line = 1;
            const char* end = str+size;
            column = 0;
            state = initialState;
            while (ongoing && offset < size) {
                ++column;
                switch (state) {
                    case State::INITIAL:
                        switch (str[offset]) {
                            case '\n': {
                                ++line;
                                column = 0;
                            } break;
                            case '{': {
                                if (offset > 0 && str[offset-1] == '{') {
                                    if (offset-1 < size && str[offset+1] == '-') {
                                        if (offset - lastInitial - 1 > 0) {
                                            i = (size_t)(previousBoundary(str, &str[offset-2]) - str);
                                            static_cast<T*>(this)->literal(&str[lastInitial], i - lastInitial + 1);
                                        }
                                        ongoing = static_cast<T*>(this)->startOutputBlock(true);
                                        ++offset;
                                    } else {
                                        if (offset - lastInitial - 1 > 0)
                                            static_cast<T*>(this)->literal(&str[lastInitial], offset - lastInitial - 1);
                                        ongoing = static_cast<T*>(this)->startOutputBlock(false);
                                    }
                                    state = State::OUTPUT;
                                }
                            } break;
                            case '%': {
                                if (offset > 0 && str[offset-1] == '{') {
                                    if (offset-1 < size && str[offset+1] == '-') {
                                        if (offset - lastInitial - 1 > 0) {
                                            i = (size_t)(previousBoundary(str, &str[offset-2]) - str);
                                            static_cast<T*>(this)->literal(&str[lastInitial], i - lastInitial + 1);
                                        }
                                        ++offset;
                                    } else {
                                        if (offset - lastInitial - 1 > 0)
                                            static_cast<T*>(this)->literal(&str[lastInitial], offset - lastInitial - 1);
                                    }

                                    // Check for the raw tag. This is a special lexing halter.
                                    i = (size_t)(nextBoundary(&str[offset+1], end) - str);
                                    if (i < size - 4 && strncmp("raw", &str[i], 3) == 0) {
                                        i = (size_t)(nextBoundary(&str[i+4], end) - str);
                                        if (i < size && str[i] == '-')
                                            ++i;
                                        if (i >= size - 2 || str[i] != '%' || str[i+1] != '}')
                                            return Lexer::Error(*this, Lexer::Error::Type::LIQUID_LEXER_ERROR_TYPE_UNEXPECTED_END);
                                        lastInitial = i+2;
                                        state = State::RAW;
                                    } else {
                                        ongoing = static_cast<T*>(this)->startControlBlock(false);
                                        state = State::CONTROL;
                                    }
                                }
                            } break;
                        }
                        ++offset;
                    break;
                    case State::OUTPUT:
                    case State::CONTROL: {
                        offset = (size_t)(nextBoundary(&str[offset], end) - str);
                        size_t startOfWord = offset, endOfWord;
                        int bytes = 1;
                        bool isNumber = true;
                        bool isSymbol = true;
                        bool isWord = true;
                        bool hasPoint = false;
                        bool processComplete = false;
                        while (offset < size) {
                            switch (str[offset]) {
                                case '"': {
                                    ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                    if (ongoing) {
                                        for (endOfWord = offset+1; endOfWord < size && (str[endOfWord] == '\\' || str[endOfWord] != '"'); ++endOfWord);
                                        ongoing = static_cast<T*>(this)->string(&str[offset+1], endOfWord - offset - 1);
                                        offset = endOfWord+1;
                                        processComplete = true;
                                    }
                                } break;
                                case '\'': {
                                    ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                    if (ongoing) {
                                        for (endOfWord = offset+1; endOfWord < size && (str[endOfWord] == '\\' || str[endOfWord] != '\''); ++endOfWord);
                                        ongoing = static_cast<T*>(this)->string(&str[offset+1], endOfWord - offset - 1);
                                        offset = endOfWord+1;
                                        processComplete = true;
                                    }
                                } break;
                                case '\n':
                                    ++line;
                                    column = 0;
                                    ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                    processComplete = true;
                                break;
                                case ' ':
                                case '\t':
                                    ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                    processComplete = true;
                                break;
                                case '-':
                                    // Special case: when this is NOT followed by a space, and it's part of a word, this is treated as part of a literal.
                                    if (offset != startOfWord) {
                                        if (!isWord || offset+1 >= size || isWhitespace(&str[offset+1], end)) {
                                            ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                            isNumber = false;
                                        }
                                    } else {
                                        isWord = false;
                                    }
                                break;
                                case '.':
                                    if (!isNumber) {
                                        ongoing = static_cast<T*>(this)->literal(&str[startOfWord], offset - startOfWord) && static_cast<T*>(this)->dot();
                                        ++offset;
                                        processComplete = true;
                                    } else {
                                        if (hasPoint) {
                                            // Likely an operator, like '..'; treat as a literal. Back up 1, and treat the number as a number, and then start processing from its end.
                                            if (str[startOfWord] != '.') {
                                                hasPoint = false;
                                                ongoing = processControlChunk(&str[startOfWord], offset - startOfWord - 1, isNumber, hasPoint);
                                                --offset;
                                                processComplete = true;
                                            } else {
                                                isNumber = false;
                                                isWord = false;
                                            }
                                        } else
                                            hasPoint = true;
                                    }
                                break;
                                case '[': ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint) && static_cast<T*>(this)->startVariableDereference(); processComplete = true;  ++offset; break;
                                case ']': ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint) && static_cast<T*>(this)->endVariableDereference(); processComplete = true;  ++offset; break;
                                case '(': ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint) && static_cast<T*>(this)->openParenthesis(); processComplete = true; ++offset; break;
                                case ')': ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint) && static_cast<T*>(this)->closeParenthesis(); processComplete = true;  ++offset; break;
                                case ':': ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint) && static_cast<T*>(this)->colon(); processComplete = true;  ++offset; break;
                                case ',': ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint) && static_cast<T*>(this)->comma(); processComplete = true;  ++offset; break;
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
                                    if (!isNumber && !isWord) {
                                        ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                        processComplete = true;
                                    } else {
                                        isSymbol = false;
                                    }
                                break;
                                case '}': {
                                    if (state == State::CONTROL) {
                                        if (str[offset-1] == '%') {
                                            ongoing = processControlChunk(&str[startOfWord], offset - startOfWord - (str[offset-2] == '-' ? 2 : 1), isNumber, hasPoint) && static_cast<T*>(this)->endControlBlock(str[offset-2] == '-');
                                            state = State::INITIAL;
                                            if (str[offset-2] == '-')
                                                offset = (size_t)(nextBoundary(&str[offset+1], end) - str);
                                            else
                                                ++offset;
                                            lastInitial = offset;
                                            processComplete = true;
                                        }
                                    } else {
                                        if (str[offset-1] == '}') {
                                            ongoing = processControlChunk(&str[startOfWord], offset - startOfWord - (str[offset-2] == '-' ? 2 : 1), isNumber, hasPoint) && static_cast<T*>(this)->endOutputBlock(str[offset-2] == '-');
                                            state = State::INITIAL;
                                            if (str[offset-2] == '-')
                                                offset = (size_t)(nextBoundary(&str[offset+1], end) - str);
                                            else
                                                ++offset;
                                            lastInitial = offset;
                                            processComplete = true;
                                        }
                                    }
                                } break;
                                default:
                                    unsigned int character;
                                    if (isUTF8Character(&str[startOfWord])) {
                                        character = getUTF8Character(&str[startOfWord], end, &bytes);
                                    } else {
                                        character = str[offset];
                                    }
                                    if (isWhitespace(character)) {
                                        ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                        processComplete = true;
                                    } else {
                                        // In the case where we change from symbol to word, word to symbol, or from number to either, we process a control chunk and split the thing up.
                                        isNumber = false;
                                        if (hasPoint) {
                                            ongoing = static_cast<T*>(this)->dot();
                                            processComplete = true;
                                        } else {
                                            if (isalpha(str[offset]) || str[offset] == '_') {
                                                if (!isWord) {
                                                    ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                                    processComplete = true;
                                                } else {
                                                    isSymbol = false;
                                                }
                                            } else {
                                                if (!isSymbol) {
                                                    ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                                    processComplete = true;
                                                } else {
                                                    isWord = false;
                                                }
                                            }
                                        }
                                    }
                                break;
                            }
                            if (processComplete)
                                break;
                            else
                                offset += bytes;
                        }
                        if (offset == size && !processComplete)
                            ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                    } break;
                    case State::RAW: {
                        // Go until the next raw tag;
                        do {
                            if (str[offset] == '}' && str[offset-1] == '%') {
                                size_t target = offset - 2;
                                if (str[target] == '-')
                                    --target;
                                target = (size_t)(previousBoundary(str, &str[target]) - str);
                                if (strncmp("endraw", &str[target-5], 6) == 0) {
                                    target = (size_t)(previousBoundary(str, &str[target-6]) - str);
                                    bool hasSuppressed = str[target-1] == '-';
                                    if (hasSuppressed)
                                        --target;
                                    if (str[target] == '%' && str[target-1] == '{') {
                                        target -= 2;
                                        if (target - lastInitial - 1 > 0)
                                            static_cast<T*>(this)->literal(&str[lastInitial], target - lastInitial + 1);
                                        state = State::INITIAL;
                                        ++offset;
                                        if (hasSuppressed)
                                            offset = (size_t)(nextBoundary(&str[offset], end) - str);
                                        lastInitial = offset;
                                        break;
                                    }
                                }
                            }
                        } while (++offset < size);
                        if (state == State::RAW)
                            return Lexer::Error(*this, Lexer::Error::Type::LIQUID_LEXER_ERROR_TYPE_UNEXPECTED_END, "raw");
                    } break;
                }
            }
            if (ongoing) {
                if (state != initialState) {
                    return Error(*this, Error::Type::LIQUID_LEXER_ERROR_TYPE_UNEXPECTED_END);
                } else if (state == State::INITIAL && offset > lastInitial) {
                    static_cast<T*>(this)->literal(&str[lastInitial], offset - lastInitial);
                }
            }
            return Error();
        }
    };
}

#endif
