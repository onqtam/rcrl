#pragma once

#include "rcrl.h"

#include <vector>

namespace rcrl
{
struct VariableDefinition
{
    std::string type;
    std::string name;
    std::string initializer;
    bool        has_assignment = false;
    bool        is_reference   = false;
};

struct Section
{
    size_t start_idx;
    size_t line;
    Mode   mode;
};

// removes comments from the string and returns a list of parsed beginings of sections - one of the 3: global/vars/once
std::vector<Section> parse_sections_and_remove_comments(std::string& out, Mode default_mode);

// parses variables from code - for 'vars' sections
std::vector<VariableDefinition> parse_vars(const std::string& text, size_t line_start = 1);

} // namespace rcrl