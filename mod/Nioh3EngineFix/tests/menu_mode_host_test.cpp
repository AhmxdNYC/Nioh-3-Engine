#define main original_menu_harness_main
#include "menu_harness.cpp"
#undef main
int main() {
 using namespace menu;
 namespace hf = host_fixture;
 hf::view={}; hf::view.size=sizeof(hf::view);hf::view.version=rmb::kVersion;
 hf::view.session=7;hf::view.generation=1;hf::view.flags=rmb::HostedReady;hf::view.workerTickMs=Now();
 g_rematchApi={sizeof(rmb::Api),rmb::kVersion,rmb::kRequiredCapabilities,0,hf::Attach,hf::Detach,hf::Post,hf::Copy,hf::Covered};g_rematchSession=7;
 Open("mode test"); Change(kRematchRow,0); CHECK(hf::posted==rmb::OpenRematch);
 hf::Complete(true);CHECK(RefreshRematchHost(Now()));
 Change(kPracticeRow,0);CHECK(hf::posted==rmb::OpenPractice);
 hf::Complete(true);CHECK(RefreshRematchHost(Now()));Close("mode test");
 hf::Complete(false);CHECK(RefreshRematchHost(Now()));
 Open("reopen Training");OpenModePage(true);CHECK(hf::posted==rmb::OpenPractice);
 hf::Complete(true);CHECK(RefreshRematchHost(Now()));
 Change(kRematchRow,0);CHECK(hf::posted==rmb::OpenRematch);
 hf::Complete(true);CHECK(RefreshRematchHost(Now()));Close("mode test");
 hf::Complete(false);CHECK(RefreshRematchHost(Now()));
 Open("reopen Boss Matches");OpenModePage(true);CHECK(hf::posted==rmb::OpenRematch);
 puts("PASS: real host switches and reopens Training and Boss Matches");
}
