#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest/doctest.h"

#include "../src/rcrl/rcrl_parser.h"

void check_helper(const char* code, const char* type, const char* name, const char* init, bool has_assign, bool is_ref) {
    auto res = rcrl::parse_vars(code);
    CHECK(res.size() == 1);
    CHECK(res[0].type == type);
    CHECK(res[0].name == name);
    CHECK(res[0].initializer == init);
    CHECK(res[0].has_assignment == has_assign);
    CHECK(res[0].is_reference == is_ref);
}

TEST_CASE("single variables") {
    check_helper("int a = 5;", "int", "a", "(5)", true, false);
    check_helper("int a(5);", "int", "a", "(5)", false, false);
    check_helper("std::vector<int> vec = {};", "std::vector<int>", "vec", "{}", true, false);
    check_helper("std::vector<decltype(my_lambda<templated>())> vec = {};", "std::vector<decltype(my_lambda<templated>())>",
                 "vec", "{}", true, false);
    check_helper("std::map<std::vector<std::string>, decltype(*wtf())> vec = {};",
                 "std::map<std::vector<std::string>, decltype(*wtf())>", "vec", "{}", true, false);
    check_helper(R"raw(std::vector
<int>
vec
=
{
}
;)raw",
                 "std::vector\n<int>", "vec", "{\n}", true, false);

	CHECK_THROWS(rcrl::parse_vars("int a(5)")); // no semicolon
	CHECK_THROWS(rcrl::parse_vars("int a((5);")); // brace mismatch
	CHECK_THROWS(rcrl::parse_vars("int (5);")); // no name
	CHECK_THROWS(rcrl::parse_vars("a = 5;")); // no name 2
}
