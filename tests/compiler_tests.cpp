#include "../src/rcrl/rcrl.h"

int main() {
	int exitcode = 0;

	rcrl::submit_code("int a = 5;", rcrl::VARS);
	while(!rcrl::try_get_exit_status_from_compile(exitcode));
	if(exitcode) return exitcode;
	rcrl::copy_and_load_new_plugin();

	rcrl::submit_code("a++;", rcrl::ONCE);
	while(!rcrl::try_get_exit_status_from_compile(exitcode));
	if(exitcode) return exitcode;
	rcrl::copy_and_load_new_plugin();

	rcrl::cleanup_plugins();
}
