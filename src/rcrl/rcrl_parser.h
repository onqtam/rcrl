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
};

// removes comments from the string and returns a list of parsed beginings of sections - one of the 3: global/vars/once
std::vector<std::pair<size_t, Mode>> remove_comments(std::string& out);
// parses variables from code - for 'vars' sections
std::vector<VariableDefinition> parse_vars(const std::string& text);

} // namespace rcrl