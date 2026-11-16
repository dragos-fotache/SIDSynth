# GitHub Actions builds and manual releases

The repository has two workflows. Neither compiles or runs the test suite,
pluginval, sanitizers, audio playback or Renoise automation. Builds explicitly
use `BUILD_TESTING=OFF`, as requested by the user.

## Linux build

[Linux build](https://github.com/dragos-fotache/SIDSynth/actions/workflows/build.yml)
runs on pushes to `master`, pull requests targeting `master`, and the manual
**Run workflow** button. It uses a standard Ubuntu 24.04 x86_64 runner, builds
the Release VST3 and packages:

- `SIDSynth-<version>-linux-x86_64.tar.gz`: plugin, licenses, docs and examples.
- `SIDSynth-<version>-source.tar.gz`: corresponding project sources, workflows,
  and the two original pinned dependency source archives.
- `SHA256SUMS`.

Open a completed workflow run and download its artifact under **Artifacts**.
The artifact name includes the source commit. Retention is **7 days**; pinned
dependency downloads are cached separately. Superseded automatic builds on the
same branch are cancelled to avoid redundant work. No release or tag is created.

Both workflows use `.github/actions/build-linux/action.yml` so their build and
packaging procedure is the same. It installs the Linux packages listed there,
fetches the exact JUCE/libresidfp commits and builds with at most two parallel
jobs. CMake and packaging verify the source archive hashes. If dependency pins
change, update the matching download URLs/filenames in that composite action.
Only dependency archives are cached; build output is compiled afresh.

All external Actions are pinned to full commits:

| Action | Release | Commit |
| --- | --- | --- |
| checkout | 5.0.0 | `08c6903cd8c0fde910a37f88322edcfb5dd907a8` |
| cache | 4.2.3 | `5a3ec84eff668545956fd18022155c47e93e2684` |
| upload-artifact | 4.6.2 | `ea165f8d65b6e75b540449e92b4886f43607fa02` |
| download-artifact | 4.3.0 | `d3f86a106a0bac45b974a628896c90dbdf5c8093` |

The Ubuntu runner and its system build tools receive GitHub/Ubuntu updates.
The application dependencies remain pinned. A successful cloud build establishes
compilation/packaging on Ubuntu 24.04; it does not establish host behavior or
sound quality. Earlier Arch/Renoise results in RELEASE.md are historical checks
for the identified module, not runtime validation of every new cloud artifact.

## Manual release

[Manual release](https://github.com/dragos-fotache/SIDSynth/actions/workflows/release.yml)
has **only `workflow_dispatch`**. Pushes, tags, successful builds and schedules
cannot start it. The agent does not run this workflow during setup.

To release when ready:

1. Set the intended version in `CMakeLists.txt` and commit/push the source.
2. Open **Actions → Manual release → Run workflow** and choose the source branch.
3. Leave **Create a draft** selected to review before publishing, or clear it
   to publish when this manually requested workflow completes. Preview status
   is selected by default and can also be changed.
4. Run the workflow. It builds the selected commit, then creates
   `v<project-version>` and attaches both archives plus `SHA256SUMS`.

If that tag already points to another commit, the release fails rather than
attaching the wrong sources to it. Existing releases are not overwritten.
Bump the project version for a new release.

The build job has read-only repository access. Only the final release job has
`contents: write`; it downloads that same run's packages and uses the built-in
`GITHUB_TOKEN`. No additional repository secret or personal token is required.

GitHub documents the manual trigger in
[Manually running a workflow](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow).
