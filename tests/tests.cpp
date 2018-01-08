#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest/doctest.h"

#include "../src/rcrl/rcrl_parser.h"

TEST_CASE("") {
    auto code = R"raw(int a = 5;
)raw";

    auto vars = rcrl::parse_vars(code);
    CHECK(vars.size() == 1);
}
