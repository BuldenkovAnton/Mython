#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <iostream>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input) : input_(input) {
        while (input_) {
            auto token = FindNextToken();
            tokens_.push_back(token);
        }

    }

    const Token& Lexer::CurrentToken() const {
        return tokens_.at(token_current_index_);
    }

    Token Lexer::NextToken() {
        token_current_index_++;
        if (token_current_index_ >= tokens_.size())  return token_type::Eof{};
        return CurrentToken();

        throw std::logic_error("Not implemented"s);
    }


    Token Lexer::FindNextToken() {
        //find end of file
        if (IsEof()) {
            if (indent_ > 0) {
                for (int i = indent_; i > 0; i -= 2) {
                    tokens_.push_back(token_type::Dedent{});
                }
            }

            if (!tokens_.empty()) {
                if (tokens_.back() != token_type::Newline{} && tokens_.back() != token_type::Dedent{} && tokens_.back() != token_type::Indent{}) {
                    tokens_.push_back(token_type::Newline{});
                }
            }


            return token_type::Eof{};
        }
        char token;

        //find indent and dedent
        if (is_new_line_) {
            is_new_line_ = false;

            int spaces = 0;

            while (input_ && input_.peek() == ' ') {
                ++spaces;
                input_.get();
            }

            if (input_.peek() != '\n') {
                int delta = spaces - indent_;
                indent_ = spaces;

                if (delta == 0) {

                }
                else if (delta > 0) {
                    for (int i = 0; i < delta - 2; i += 2) {
                        tokens_.push_back(token_type::Indent{});
                    }
                    return token_type::Indent{};
                }
                else {
                    for (int i = delta; i + 2 < 0; i += 2) {
                        tokens_.push_back(token_type::Dedent{});
                    }
                    return token_type::Dedent{};
                }
            }
            else {
                return FindNextToken();
            }


        }

        token = input_.peek();

        //find spaces
        if (token == ' ') {
            input_.get();
            while (input_ && input_.peek() == ' ') {
                input_.get();
            }

            return FindNextToken();
        }

        //find comments
        if (token == '#') {
            input_.get();
            while (input_ && input_.peek() != '\n') {
                input_.get();
            }

            return FindNextToken();
        }

        //find strings
        if (IsString(token)) {
            char quote = input_.get();

            std::string str;
            while (input_ && input_.peek() != quote) {
                if (input_.peek() == '\\') {
                    input_.get();
                    if (input_.peek() == '\'') {
                        input_.get();
                        str += '\'';
                        continue;
                    }
                    if (input_.peek() == '\"') {
                        input_.get();
                        str += '\"';
                        continue;
                    }
                    if (input_.peek() == 'n') {
                        input_.get();
                        str += '\n';
                        continue;
                    }
                    if (input_.peek() == 't') {
                        input_.get();
                        str += '\t';
                        continue;
                    }
                    if (input_.peek() == '\\') {
                        input_.get();
                        str += '\\';
                        continue;
                    }
                }
                str += input_.get();
            }
            input_.get();
            return token_type::String{ str };
        }



        //find numbers
        if (IsNumber(token)) {
            char c = input_.get();

            std::string number{ c };
            while (IsNumber(input_.peek())) {
                c = input_.get();
                number += c;

            }

            return token_type::Number{ std::stoi(number) };
        }

        //fine end of line
        if (token == '\n') {
            input_.get();
            is_new_line_ = true;

            if (tokens_.empty()) return FindNextToken();

            if (tokens_.back() != token_type::Newline{}) {
                return token_type::Newline{};
            }
            else
                return FindNextToken();
        }

        //find special words
        if (IsId(input_.peek())) {
            std::string word;
            while (IsId(input_.peek())) {
                char c = input_.get();
                word += c;
            }

            if (keywords_.count(word) > 0) {
                return keywords_.at(word);
            }
            return token_type::Id{ word };
        }


        //find symbols
        char c = input_.get();
        if (c == '=' && input_.peek() == '=') {
            input_.get();
            return token_type::Eq{};
        }
        if (c == '!' && input_.peek() == '=') {
            input_.get();
            return token_type::NotEq{};
        }
        if (c == '<' && input_.peek() == '=') {
            input_.get();
            return token_type::LessOrEq{};
        }
        if (c == '>' && input_.peek() == '=') {
            input_.get();
            return token_type::GreaterOrEq{};
        }
        return token_type::Char{ c };
    }

}  // namespace parse