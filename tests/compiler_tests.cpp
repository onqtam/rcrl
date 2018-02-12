#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest/doctest.h"

#include "../src/rcrl/rcrl.h"

TEST_CASE("single variables") {
	int exitcode = 0;

	rcrl::submit_code("int a = 5;", rcrl::VARS);
	while(!rcrl::try_get_exit_status_from_compile(exitcode));
	REQUIRE_FALSE(exitcode);
	rcrl::copy_and_load_new_plugin();

	rcrl::submit_code("a++;", rcrl::ONCE);
	while(!rcrl::try_get_exit_status_from_compile(exitcode));
	REQUIRE_FALSE(exitcode);
	rcrl::copy_and_load_new_plugin();
}

#ifndef __APPLE__

#ifdef _WIN32
#define RCRL_SYMBOL_EXPORT __declspec(dllexport)
#else
#define RCRL_SYMBOL_EXPORT __attribute__((visibility("default")))
#endif

std::vector<int> g_pushed_ints;
RCRL_SYMBOL_EXPORT void test_ctor_dtor_order(int num) { g_pushed_ints.push_back(num); }

TEST_CASE("destructor order") {
	int exitcode = 0;

	// about to check destructor order
    rcrl::submit_code(R"raw(
//vars
int num_instances = 0;

//global
RCRL_SYMBOL_IMPORT void test_ctor_dtor_order(int);
struct S {
	int instance;
	S() : instance(++num_instances) { test_ctor_dtor_order(instance); }
	~S() { test_ctor_dtor_order(instance); }
};

//vars
S a1;
S a2;
)raw");
    while(!rcrl::try_get_exit_status_from_compile(exitcode));
	REQUIRE_FALSE(exitcode);

	rcrl::copy_and_load_new_plugin();

	REQUIRE(g_pushed_ints.size() == 2);
	REQUIRE(g_pushed_ints[0] == 1);
	REQUIRE(g_pushed_ints[1] == 2);

	rcrl::cleanup_plugins();

	REQUIRE(g_pushed_ints.size() == 4);
	REQUIRE(g_pushed_ints[2] == 2);
	REQUIRE(g_pushed_ints[3] == 1);
}

#endif
