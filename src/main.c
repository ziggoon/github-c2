#include "github.h"

int main(void)
{
	gh_ctx_t ctx;

	if (gh_init(&ctx) != 0) {
		LOGERR("init failed");
		return 1;
	}
	if (gh_bootstrap_pr(&ctx) != 0) {
		LOGERR("PR bootstrap failed");
		gh_close(&ctx);
		return 1;
	}
	LOG("https://github.com/%s/%s/pull/%d", GITHUB_OWNER, GITHUB_REPO, ctx.pr_number);
	gh_close(&ctx);
	return 0;
}
