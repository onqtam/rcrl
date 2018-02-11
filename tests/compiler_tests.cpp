#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest/doctest.h"

#include "../src/rcrl/rcrl.h"

#define REMOVE_WINDOWS_NEWLINE(a) a.erase(std::remove(a.begin(), a.end(), '\r'), a.end())

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

TEST_CASE("destructor order") {
	int exitcode = 0;

	// about to check destructor order
    rcrl::submit_code(R"raw(
//vars
int num_instances = 0;

//global
#include <iostream>
struct S {
	int instance;
	S() : instance(++num_instances) { std::cout << "hi " << instance << std::endl; }
	~S() { std::cout << "bye " << instance << std::endl; }
};

//vars
S a1;
S a2;
)raw");
    while(!rcrl::try_get_exit_status_from_compile(exitcode));
	REQUIRE_FALSE(exitcode);
	auto output_from_loading = rcrl::copy_and_load_new_plugin(true);
	REMOVE_WINDOWS_NEWLINE(output_from_loading);
	CHECK(output_from_loading == "hi 1\nhi 2\n");

	auto output_from_unloading = rcrl::cleanup_plugins(true);
	REMOVE_WINDOWS_NEWLINE(output_from_unloading);
    CHECK(output_from_unloading == "bye 2\nbye 1\n");
}
