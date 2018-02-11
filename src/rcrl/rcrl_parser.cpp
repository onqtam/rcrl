#include "rcrl_parser.h"

#include <cassert>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <sstream>

using namespace std;

namespace rcrl
{
static void ltrim(string& s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](int ch) { return !isspace(ch); }));
}

static void rtrim(string& s) {
    s.erase(find_if(s.rbegin(), s.rend(), [](int ch) { return !isspace(ch); }).base(), s.end());
}

static void trim(string& s) {
    ltrim(s);
    rtrim(s);
}

static vector<string> split(const string& str) {
    vector<string> tokens;

    string       buf;
    stringstream ss(str);
    while(ss >> buf)
        tokens.push_back(buf);

    return tokens;
}

vector<Section> parse_sections_and_remove_comments(string& out, Mode default_mode) {
    const string text(out);

    size_t line = 1;

    bool in_char                = false; // 'c'
    bool in_string              = false; // "str"
    bool in_comment             = false;
    bool in_single_line_comment = false;
    bool in_multi_line_comment  = false;

    vector<Section> section_starts;
    section_starts.push_back({0, line, default_mode}); // this is the default input method

    for(size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];

        in_comment = in_single_line_comment || in_multi_line_comment;

        // assert for consistency
        assert(!in_char || (in_char && !in_comment && !in_string));
        assert(!in_string || (in_string && !in_char && !in_comment));
        assert(!in_comment || (in_comment && !in_char && !in_string));

        if(c == '"' && !in_comment && !in_char) {
            if(!in_string) {
                in_string = true;
            } else {
                int num_slashes = 0;
                while(text[i - 1 - num_slashes] == '\\') {
                    ++num_slashes;
                }

                if(num_slashes % 2 == 0) {
                    in_string = false;
                }
            }
        }
        if(c == '\'' && !in_comment && !in_string) {
            if(!in_char) {
                in_char = true;
            } else {
                if(text[i - 1] != '\\') {
                    in_char = false;
                }
            }
        }
        if(c == '/') {
            if(!in_comment && !in_char && !in_string && i > 0 && text[i - 1] == '/') {
                in_single_line_comment = true;
                out[i - 1]             = ' '; // substitute last character with whitespace - apparently we are in a comment
                out[i]                 = ' ';
                continue;
            }
            if(in_multi_line_comment && text[i - 1] == '*') {
                in_multi_line_comment = false;
                out[i]                = ' '; // append whitespace
                continue;
            }
        }
        if(c == '*') {
            if(!in_comment && !in_char && !in_string && i > 0 && text[i - 1] == '/') {
                in_multi_line_comment = true;
                out[i - 1]            = ' '; // substitute last character with whitespace - apparently we are in a comment
                out[i]                = ' ';
                continue;
            }
        }
        if(c == '\n') {
            ++line;

            if(in_single_line_comment && text[i - 1] != '\\') {
                in_single_line_comment = false;

                // search for RCRL directives
                auto directive_finder = [&](const string& look_for) {
                    auto directive_last_pos = text.rfind(look_for, i);
                    if(directive_last_pos != string::npos) {
                        // check that there are is only whitespace after the directive
                        for(auto k = directive_last_pos + look_for.size(); k < i; ++k)
                            if(!isspace(text[k]))
                                return false;

                        // check that there are is only whitespace before the directive
                        for(auto k = directive_last_pos - 1; text[k] != '/'; --k)
                            if(!isspace(text[k]))
                                return false;

                        return true;
                    }
                    return false;
                };

                if(directive_finder("global"))
                    section_starts.push_back({i, line - 1, GLOBAL});
                if(directive_finder("vars"))
                    section_starts.push_back({i, line - 1, VARS});
                if(directive_finder("once"))
                    section_starts.push_back({i, line - 1, ONCE});

                continue;
            }
        }

        if(in_comment) {
            // contents of comments are turned into whitespace
            out[i] = ' ';
        }
    }

    return section_starts;
}

vector<VariableDefinition> parse_vars(const string& text, size_t line_start) {
    vector<VariableDefinition> out;

    bool in_char       = false; // 'c'
    bool in_string     = false; // "str"
    bool in_whitespace = false;

    bool just_enterned_char_string = false;

    vector<pair<char, size_t>> braces;                       // the current active stack of braces
    int                        opened_template_brackets = 0; // the current active stack of template <> braces

    vector<size_t> semicolons;        // the positions of all semicolons
    vector<size_t> whitespace_begins; // the positions of all whitespace beginings
    vector<size_t> whitespace_ends;   // the positions of all whitespace endings

    VariableDefinition current_var;
    bool               in_var               = false;
    size_t             current_var_name_end = 0;

    int line   = line_start;
    int column = 1;

    auto parse_error = [&](const char* msg) {
        return string("parse error (") + to_string(line) + "/" + to_string(column) + "): " + msg;
    };

    for(size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];

        if(c == '"' && !in_char) {
            if(!in_string) {
                in_string                 = true;
                just_enterned_char_string = true;
            } else {
                int num_slashes = 0;
                while(text[i - 1 - num_slashes] == '\\') {
                    ++num_slashes;
                }

                if(num_slashes % 2 == 0) {
                    in_string = false;
                }
            }
        }
        if(c == '\'' && !in_string) {
            if(!in_char) {
                in_char                   = true;
                just_enterned_char_string = true;
            } else {
                if(text[i - 1] != '\\') {
                    in_char = false;
                }
            }
        }

        // proceed with parsing variable definitions
        if(!in_string && !in_char) {
            if(braces.size() == 0 && opened_template_brackets == 0 && (c == ';' || c == '(' || c == '{' || c == '=')) {
                // detect decltype
                bool opening_decltype = false;
                if(c == '(') {
                    auto last_decltype_pos = text.rfind("decltype", i);
                    if(last_decltype_pos != string::npos) {
                        // check to see if there are any non-whitespace characters between 'decltype' and the opening brace '('
                        bool only_whitespace_after_decltype = true;
                        for(auto k = last_decltype_pos + strlen("decltype"); k < i; ++k)
                            if(!isspace(text[k]))
                                only_whitespace_after_decltype = false;

                        if(only_whitespace_after_decltype)
                            opening_decltype = true;
                    }
                }

                // if after the name of a variable
                if(!in_var && !opening_decltype) {
                    if(whitespace_ends.size() < 2)
                        throw runtime_error(parse_error("expected <type> <name>... with atleast 1 space in between"));

                    auto var_name_begin  = whitespace_ends.back();
                    auto var_name_len    = i - var_name_begin;
                    current_var_name_end = i;
                    in_var               = true;

                    current_var.name = text.substr(var_name_begin, var_name_len);
                    trim(current_var.name);
                    size_t type_begin = semicolons.size() ? semicolons.back() + 1 : 0;
                    current_var.type  = text.substr(type_begin, var_name_begin - type_begin);
                    trim(current_var.type);

                    if(current_var.type.size() == 0)
                        throw runtime_error(parse_error("couldn't parse type for var"));

                    if(current_var.type.size() > 1 && current_var.type.back() == '&') {
                        current_var.is_reference = true;
                        current_var.type.pop_back();
                        trim(current_var.type);

                        // if the type was ending with &&
                        if(current_var.type.back() == '&')
                            throw runtime_error(parse_error("rvalue references as local variables not supported!"));
                    }

                    // detect "auto"/"const auto" types and put them in a canonical form with just 1 space between them
                    auto tokens = split(current_var.type);
                    if((tokens.size() == 1 && tokens[0] == "auto") ||
                       (tokens.size() == 2 && tokens[0] == "const" && tokens[1] == "auto"))
                        current_var.type = tokens[0] + (tokens.size() == 2 ? string(" ") + tokens[1] : "");
                }

                // if we are finalizing the variable - check if there is anything for its initialization
                if(c == ';' && in_var) {
                    current_var.initializer = text.substr(current_var_name_end, i - current_var_name_end);
                    trim(current_var.initializer);

                    if(current_var.initializer.size()) {
                        if(current_var.initializer.front() == '=') {
                            current_var.initializer.erase(current_var.initializer.begin());
                            current_var.has_assignment = true;
                            trim(current_var.initializer);
                            if(current_var.initializer.size() == 0)
                                throw runtime_error(parse_error("no initializer code between '=' and ';'"));
                        }
                        // if a reference - get the address of the result of the initializer since we are using a pointer under the hood
                        if(current_var.is_reference)
                            current_var.initializer = "&" + current_var.initializer;

                        // surround the initializer with braces if there are none
                        if(current_var.initializer.front() != '(' && current_var.initializer.front() != '{')
                            current_var.initializer = "(" + current_var.initializer + ")";
                    }

                    if(current_var.is_reference && current_var.initializer.size() == 0)
                        throw runtime_error(parse_error("references must be initialized"));

                    // var parsed
                    out.push_back(current_var);

                    // clear state
                    in_var      = false;
                    current_var = VariableDefinition();
                }
            }

            // track template brackets only if not in any other braces
            if(braces.size() == 0 && (c == '<' || c == '>')) {
                if(c == '<') {
                    ++opened_template_brackets;
                } else {
                    if(opened_template_brackets == 0)
                        throw runtime_error(parse_error("template opening/closing bracket mismatch"));
                    --opened_template_brackets;
                }
            }
            if(c == '(' || c == '[' || c == '{') {
                braces.push_back({c, i});
            }
            if(c == ')' || c == ']' || c == '}') {
                // check that we are closing the right bracket
                if(braces.size() == 0)
                    throw runtime_error(parse_error("encountered closing brace without an opening one"));
                if(braces.back().first != (c == ')' ? '(' : (c == '}' ? '{' : '[')))
                    throw runtime_error(parse_error("closing brace mismatch"));
                braces.pop_back();
            }
            if(braces.size() == 0 && c == ';') {
                semicolons.push_back(i);
            }
            if(c == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }

        if((!in_string && !in_char) || just_enterned_char_string) {
            // if the current char is not a whitespace, but the previous one was - mark the end of a whitespace block
            if(!isspace(c) && (i == 0 || isspace(text[i - 1]))) {
                in_whitespace = false;
                whitespace_ends.push_back(i);
                if(i == 0)
                    whitespace_begins.push_back(i); // handle corner case
                assert(whitespace_begins.size() == whitespace_ends.size());
            }
            // if the current char is a whitespace, but the previous wasn't - mark the start of a whitespace block
            if(isspace(c) && (i == 0 || !isspace(text[i - 1]))) {
                whitespace_begins.push_back(i);
                in_whitespace = true;
            }
        }

        just_enterned_char_string = false;

        // assert for consistency
        assert(!in_char || (in_char && !in_string));
        assert(!in_string || (in_string && !in_char));
    }

    if(in_whitespace)
        whitespace_ends.push_back(text.size() - 1);

    assert(whitespace_begins.size() == whitespace_ends.size());
    if(in_var)
        throw runtime_error("parse error - parsing of variables not finished");
    if(braces.size() != 0)
        throw runtime_error("parse error - not all braces are closed");
    // and check that nothing is left unparsed - so people can't enter in garbage
    for(auto i = (semicolons.size() ? semicolons.back() + 1u : 0u); i < text.size(); ++i)
        if(!isspace(text[i]))
            throw runtime_error("parse error - unparsed contents");

    return out;
}

} // namespace rcrl
