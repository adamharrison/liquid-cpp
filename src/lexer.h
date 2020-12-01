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


        struct Error : LiquidLexerError {
            typedef LiquidLexerErrorType Type;

            Error() {
                type = Type::LIQUID_LEXER_ERROR_TYPE_NONE;
                message[0] = 0;
            }
            Error(const Error& error) = default;
            Error(Error&& error) = default;

            Error(Lexer& lexer, Type type) {
                this->row = lexer.row;
                this->column = lexer.column;
                this->type = type;
            }
            Error(Lexer& lexer, Type type, const std::string& message) {
                this->row = lexer.row;
                this->column = lexer.column;
                this->type = type;
                strcpy(this->message, message.data());
            }
            Error(Type type) {
                this->row = 0;
                this->column = 0;
                this->type = type;
                message[0] = 0;
            }
            Error(Type type, const std::string& message) {
                this->row = 0;
                this->column = 0;
                this->type = type;
                strcpy(this->message, message.data());
            }
        };


        size_t column;
        size_t row;
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

        bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n'; }

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

        // Must be a whole file, for now. Should be null-terminated.
        Error parse(const char* str, size_t size) {
            size_t offset = 0;
            size_t lastInitial = 0;
            size_t i;
            bool ongoing = true;
            row = 1;
            column = 0;
            state = State::INITIAL;
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
                                if (offset > 0 && str[offset-1] == '{') {
                                    if (offset-1 < size && str[offset+1] == '-') {
                                        if (offset - lastInitial - 1 > 0) {
                                            for (i = offset-2; isWhitespace(str[i]); --i);
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
                                            for (i = offset-2; isWhitespace(str[i]); --i);
                                            static_cast<T*>(this)->literal(&str[lastInitial], i - lastInitial + 1);
                                        }
                                        ++offset;
                                    } else {
                                        if (offset - lastInitial - 1 > 0)
                                            static_cast<T*>(this)->literal(&str[lastInitial], offset - lastInitial - 1);
                                    }

                                    // Check for the raw tag. This is a special lexing halter.
                                    for (i = offset+1; i < size && isWhitespace(str[i]); ++i);
                                    if (i < size - 4 && strncmp("raw", &str[i], 3) == 0) {
                                        for (i = i + 4; i < size && isWhitespace(str[i]); ++i);
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
                        for (; isWhitespace(str[offset]) && offset < size; ++offset);
                        size_t startOfWord = offset, endOfWord;
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
                                case ' ':
                                case '\t':
                                case '\n':
                                    ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                    processComplete = true;
                                break;
                                case '-':
                                    if (offset != startOfWord) {
                                        ongoing = processControlChunk(&str[startOfWord], offset - startOfWord, isNumber, hasPoint);
                                        isNumber = false;
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
                                default:
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
                                break;
                                case '}': {
                                    if (state == State::CONTROL) {
                                        if (str[offset-1] == '%') {
                                            ongoing = processControlChunk(&str[startOfWord], offset - startOfWord - (str[offset-2] == '-' ? 2 : 1), isNumber, hasPoint) && static_cast<T*>(this)->endControlBlock(str[offset-2] == '-');
                                            state = State::INITIAL;
                                            if (str[offset-2] == '-')
                                                for (offset = offset+1; isWhitespace(str[offset]) && offset < size; ++offset);
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
                                                for (offset = offset+1; isWhitespace(str[offset]) && offset < size; ++offset);
                                            else
                                                ++offset;
                                            lastInitial = offset;
                                            processComplete = true;
                                        }
                                    }
                                } break;
                            }
                            if (processComplete)
                                break;
                            else
                                ++offset;
                        }
                    } break;
                    case State::RAW: {
                        // Go until the next raw tag;
                        do {
                            if (str[offset] == '}' && str[offset-1] == '%') {
                                size_t target = offset - 2;
                                if (str[target] == '-')
                                    --target;
                                for (; isWhitespace(str[target]); --target);
                                if (strncmp("endraw", &str[target-5], 6) == 0) {
                                    for (target = target-6; isWhitespace(str[target]); --target);
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
                                            for (; isWhitespace(str[offset]) && offset < size; ++offset);
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
                if (state != State::INITIAL) {
                    return Error(*this, Error::Type::LIQUID_LEXER_ERROR_TYPE_UNEXPECTED_END);
                } else {
                    if (offset > lastInitial) {
                        static_cast<T*>(this)->literal(&str[lastInitial], offset - lastInitial);
                    }
                }
            }
            return Error();
        }
    };
}

#endif
