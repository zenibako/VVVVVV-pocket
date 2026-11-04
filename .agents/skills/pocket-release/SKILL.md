---
name: pocket-release
description: Use when validating VVVVVV Pocket distributions, preparing release candidates, reviewing plugin assets or publishing an explicitly authorized release.
---

# VVVVVV Pocket releases

Read repository instructions and the [release guide](../../../openfpga/release/README.md) first.

## Read the shared workflow

Canonical owner: <https://github.com/zenibako/pocket-dev-tools> (formerly
`pocket-harness`). Use the root established in this session, then
`POCKET_DEV_TOOLS_ROOT`, then legacy `POCKET_HARNESS_ROOT`. Resolve conflicting
explicit roots before proceeding. A sibling `pocket-dev-tools` or `pocket-harness`
is only a candidate: verify `.agents/skills/pocket-release/SKILL.md` and
`docs/RELEASING.md`, confirm the Git remote identifies the canonical repository
(or its former name), and record its full commit. Resolve and read that canonical
skill before following its relative links. Apply it together with this project's
local release guide; stronger local checks remain required.

If the shared checkout is unavailable, report the missing dependency and continue
applicable local checks. Do not claim shared validation, copy the shared skill
body, or create cross-repository skill symlinks. A checkout predating the release
skill needs an explicit update; a matching directory name alone is insufficient.

## Project contract

## Test builds

Build device test trees (game, `BENCH=1` bench core and `MAKEANDPLAY=1`) with
`make -C openfpga package-test`, never plain `package`. It writes the SD tree
(`openfpga/build/sd`, or `build/makeandplay/sd`); zip that tree's contents for
the device. core.json then reports
`<next>-rc.<N>` from pocket-dev-tools' shared `pocket_version.py` (set
`POCKET_DEV_TOOLS_ROOT`, or keep a sibling checkout): N counts the commits
since the last `vX.Y.Z` tag, the next version is the last release with its
patch bumped, and `NEXT_VERSION=X.Y.Z` overrides it for a minor or major
release. Run `git fetch --tags` if no tag is found (`git fetch --unshallow --tags` in a shallow clone). Name the zip after that
version and the short commit, for example
`vvvvvv-pocket-v0.2.1-rc.3-<sha>-<topic>.zip`. Pocket Sync still offers the
published release as an update to a test build; that is expected. Release
assembly writes the release version itself.

## Release contract

Use this checkout's `openfpga-release.yml`, `openfpga/release/assemble.py` and `runtime.lock.json`. Keep the complete asset/source/license inventory, protected publication environment and source/runtime checks. Candidate-only builds do not authorize publication; record physical launch, controls/audio, progress-save and applicable Memory/sleep checks separately from CI.
