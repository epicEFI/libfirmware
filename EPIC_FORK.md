# epicEFI fork of rusefi/libfirmware

This repository is epicEFI's fork of [rusefi/libfirmware](https://github.com/rusefi/libfirmware).
[epicEFI/epicefi_fw](https://github.com/epicEFI/epicefi_fw) consumes it as the submodule
`firmware/libfirmware`, pinned to a commit on this repository's `master`.

We carry our own fixes here and keep merging upstream's, so rusEFI bug fixes still reach
epicEFI firmware. We do not send changes upstream.

## Branch rules

| rule | why |
|---|---|
| `master` is upstream plus our changes, and is the only branch epicefi_fw pins | one line of history to reason about |
| **never force-push or rebase `master`** | epicefi_fw pins commits of it; rewriting orphans them and old firmware releases stop checking out |
| **upstream is merged, never rebased or squashed** - sync PRs use **"Create a merge commit"** | the next sync needs upstream's own commits in our history; a squash makes every later sync re-conflict on everything upstream ever changed |
| our own PRs may be squash-merged | they are ours, nothing downstream of us tracks their commits |
| push only to `origin` (this fork) | `tools/sync_upstream.sh` sets upstream's push URL to `NO_PUSH_TO_UPSTREAM` |

To see what is ours: `git log --no-merges upstream/master..master`.

## Our changes on top of upstream

| change | files |
|---|---|
| `Timer::hasElapsedUs()`: thresholds over 2^32 ticks fired early - at ~1074 s on STM32 (saturating cast), at ~17 s for a 60 s timer on x86 (wrapping cast). Compared in float now, and NaN is explicitly "elapsed" | `util/src/timer.cpp`, `util/test/test_timer.cpp` |
| `cyclic_buffer`: size 0 let `add()` write past `elements[]` (and `get()` loop forever); `contains()` ignored every slot at or above `currentIndex` once the buffer wrapped; `sum/minValue/maxValue(n)` counted elements twice when `n` exceeded the size | `util/include/rusefi/containers/cyclic_buffer.h`, `util/test/test_cyclic_buffer.cpp` |
| `fifo_buffer_sync::get()` in `EFI_UNIT_TEST` builds returned `true` with a stale element on an empty queue; it now reports the timeout like the ECU does. First tests for `fifo_buffer` | `util/include/rusefi/containers/fifo_buffer.h`, `util/test/test_fifo_buffer.cpp` |
| `SANITIZE=yes` adds UBSan and `-fsanitize=float-cast-overflow` (GCC's `-fsanitize=undefined` leaves that one out - it is how the Timer cast hid) | `Makefile` |
| `atoff()`: the error guards compared `absI(result) == ATOI_ERROR_CODE` - a magnitude against a negative sentinel - so they could never fire and invalid text parsed as `-3.11223344e8` instead of NaN (bypassing cli_registry's `isnan()` rejection of bad console floats); the fraction of a negative value was also added unsigned (`"-42.5"` -> `-41.5`). Direct sentinel comparisons and a sign-carrying result now | `util/src/efistringutil.cpp`, `util/test/test_efistringutil.cpp` |
| upstream sync tooling and this file | `tools/sync_upstream.sh`, `.github/workflows/sync-upstream.yaml`, `EPIC_FORK.md` |

Add a row whenever you add a change here, so whoever resolves the next sync conflict knows
which side is ours and why.

## Syncing upstream

### Automatic (weekly)

`.github/workflows/sync-upstream.yaml` runs Mondays 06:23 UTC and on demand
(Actions > Sync upstream > Run workflow). It runs `tools/sync_upstream.sh --ci`, which:

1. fetches `rusefi/libfirmware` master and stops if our master already contains it;
2. merges it into a `sync/upstream` branch cut from our master (`--no-ff`);
3. builds and runs this library's own suite under ASan + UBSan + float-cast-overflow.

Then the workflow:

- **merged and green**: force-pushes `sync/upstream` (it is scratch - never pin it) and opens or
  refreshes a PR to `master`. The epicEFI org does not let `GITHUB_TOKEN` open PRs, so it falls
  back to an issue titled "Sync upstream rusefi/libfirmware ..." holding the one-click compare link.
  An org admin can remove that fallback: org Settings > Actions > General > "Allow GitHub Actions
  to create and approve pull requests".
- **conflict or failing tests**: opens (or comments on) an issue "Upstream sync needs a human".

PRs opened by the workflow do not trigger the "Unit Tests" workflow (a GitHub rule for
`GITHUB_TOKEN`); the sync job ran the suite itself, and "Unit Tests" runs on `master` after the merge.

### By hand (conflicts, or when you need a fix now)

```bash
git clone https://github.com/epicEFI/libfirmware.git && cd libfirmware   # or use firmware/libfirmware in epicefi_fw
git submodule update --init --recursive
tools/sync_upstream.sh
# on a conflict it stops with the merge in place: resolve, `git commit`, then
#   make clean && make -j$(nproc) SANITIZE=yes && build/libfirmware_test
git push -f origin sync/upstream
# open a PR sync/upstream -> master, merge with "Create a merge commit"
```

When resolving, check the table above for each conflicting file: keep our fix unless upstream fixed
the same defect, in which case take theirs and delete our row.

## After `master` moves: bump epicefi_fw

Nothing reaches firmware until epicefi_fw's pin moves, and that bump is the real gate - the
library's suite cannot see how the firmware uses it. In epicefi_fw:

```bash
firmware/tools/libfirmware_fork_status.sh     # pin vs fork vs upstream, test-name collision check
cd firmware/libfirmware && git fetch origin && git checkout <new master sha> && cd ../..
cd firmware && ./build.sh test build && ./build.sh test run
cd ../unit_tests && ./run_shuffled_seeds.sh $(seq 1 24)
# and at least one ARM firmware build: it runs checkIllegalConversion, which rejects any
# float->int64 conversion (__aeabi_f2lz) in the image
```

Full procedure and the traps already hit: `CLAUDE/libfirmware_fork.md` in epicefi_fw.

## Rules for code in this repository

- **No float -> int64 casts** in anything the firmware links: epicefi_fw's post-build
  `checkIllegalConversion` fails the build on `__aeabi_f2lz`.
- **Test file names must not collide with epicefi_fw's unit tests**: its build puts every object
  in one flat `build/obj/` by basename, so `util/test/test_foo.cpp` and a `unit_tests/.../test_foo.cpp`
  break its link. `firmware/tools/libfirmware_fork_status.sh` checks this.
- Keep the fast paths of anything called from ISRs or 200 Hz callbacks in single-precision float -
  the F4 has no double-precision FPU.
