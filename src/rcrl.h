#pragma once

#include <vector>
#include <string>

namespace rcrl
{
enum Mode
{
	FROM_COMMENTS,
    GLOBAL,
    VARS,
    ONCE
};

void        cleanup_plugins();
bool        submit_code(std::string code, Mode mode);
std::string get_new_compiler_output();
bool        is_compiling();
bool        try_get_exit_status_from_compile(int& exitcode);
void        copy_and_load_new_plugin();
} // namespace rcrl