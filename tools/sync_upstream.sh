#!/bin/bash
#
# Merge rusefi/libfirmware master into this fork (epicEFI/libfirmware) on a sync branch, then
# build and run this library's own test suite on the result. See EPIC_FORK.md.
#
#   tools/sync_upstream.sh          local: leaves a conflicted merge in place for you to resolve
#   tools/sync_upstream.sh --ci     CI: aborts a conflicted merge, reports through $GITHUB_OUTPUT
#
# Exit codes: 0 up to date or merged + tests green, 2 merge conflict, 3 tests failed, 1 other.
#
# Upstream is MERGED, never rebased or squashed: epicefi_fw pins commits of our master, and the
# next sync needs upstream's own commits in our history to know what it has already seen.

set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

UPSTREAM_URL=https://github.com/rusefi/libfirmware.git
BRANCH=sync/upstream
CI=0
[[ "${1:-}" == "--ci" ]] && CI=1

out() {
	# key=value for the workflow; a no-op when run by hand
	if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
		echo "$1" >> "$GITHUB_OUTPUT"
	fi
}

if ! git remote get-url upstream >/dev/null 2>&1; then
	git remote add upstream "$UPSTREAM_URL"
fi
# never push to rusefi by accident
git remote set-url --push upstream NO_PUSH_TO_UPSTREAM

if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
	echo "working tree has uncommitted changes - commit or stash them first" >&2
	exit 1
fi

git fetch --quiet origin master
git fetch --quiet upstream master

UPSTREAM_SHA=$(git rev-parse upstream/master)
UPSTREAM_SHORT=$(git rev-parse --short upstream/master)
NEW_COMMITS=$(git rev-list --count origin/master..upstream/master)
out "upstream_sha=$UPSTREAM_SHA"
out "upstream_short=$UPSTREAM_SHORT"
out "new_commits=$NEW_COMMITS"

if [[ "$NEW_COMMITS" == 0 ]]; then
	echo "origin/master already contains upstream/master ($UPSTREAM_SHORT) - nothing to sync"
	out "result=up_to_date"
	exit 0
fi

echo "upstream has $NEW_COMMITS commit(s) not in our master:"
git log --format='  %h %ad %an  %s' --date=short origin/master..upstream/master
git log --format='- %h %ad %an: %s' --date=short origin/master..upstream/master > .sync_upstream_commits.txt

git checkout --quiet -B "$BRANCH" origin/master
git submodule update --init --recursive --quiet

if ! git merge --no-ff --no-edit -m "Merge upstream rusefi/libfirmware $UPSTREAM_SHORT" upstream/master; then
	CONFLICTS=$(git diff --name-only --diff-filter=U | tr '\n' ' ')
	echo "MERGE CONFLICT in: $CONFLICTS" >&2
	out "result=conflict"
	out "conflicts=$CONFLICTS"
	if [[ "$CI" == 1 ]]; then
		git merge --abort
	else
		echo "Resolve, 'git commit', then re-run the tests: make clean && make -j\$(nproc) SANITIZE=yes && build/libfirmware_test" >&2
	fi
	exit 2
fi
git submodule update --init --recursive --quiet

echo "merged cleanly - building and running the library test suite"
make clean >/dev/null
if ! make -j"$(nproc 2>/dev/null || echo 4)" SANITIZE=yes > .sync_upstream_build.log 2>&1; then
	tail -40 .sync_upstream_build.log >&2
	out "result=tests_failed"
	exit 3
fi
if ! ASAN_OPTIONS=detect_stack_use_after_return=1 build/libfirmware_test > .sync_upstream_test.log 2>&1; then
	grep -E '^\[  FAILED  \]|runtime error|ERROR: AddressSanitizer' .sync_upstream_test.log >&2 || tail -40 .sync_upstream_test.log >&2
	out "result=tests_failed"
	exit 3
fi
SUMMARY=$(grep -E '^\[  PASSED  \]' .sync_upstream_test.log | head -1)
echo "tests: $SUMMARY"
out "tests=$SUMMARY"
out "result=merged"

if [[ "$CI" == 0 ]]; then
	cat <<EOF

Branch $BRANCH now holds the merge. Next:
  git push -f origin $BRANCH
  open a PR $BRANCH -> master and merge it with "Create a merge commit" (never squash or rebase)
  then bump firmware/libfirmware in epicefi_fw - see EPIC_FORK.md
EOF
fi
