# rtl87x2j prebuilt lib update

`update_libs.sh` updates `libdriver.a`, `libkernel.a` and
`librtl87x2j_usb.a`:

1. It rebuilds the three libraries from the rtl87x2j multirepo.
2. It stores them in `lib/rtl87x2j/` of this repository, with
   `PROVENANCE.yml` and `SHA256SUMS`, and copies them to `bee/ld/rtl87x2j/`
   of hal_realtek.
3. It builds two Zephyr apps with MCUboot and prints their warnings and
   errors. Only a failed build stops the script.
   - `zephyr/tests/boards/realtek/bee/bee_pm_device`
   - `zephyr/samples/subsys/usb/hid-mouse`

   Each app is built from scratch in its own `build/`, so any build already
   there is replaced. To try the new libs on the board, `cd` into the app and
   run `west flash`.
4. It commits `lib/rtl87x2j/` in this repository, then the three libraries
   in hal_realtek. The hal_realtek commit message gives the `lib_version`
   and the sha256 of each lib. Both messages name the PR of this tool
   instead of a commit, because the tool branch is rebased and its commit
   SHAs do not last. Nothing is pushed.

## Prerequisites

- The multirepo at `~/zephyr/multi_repo_bee5`. If it is elsewhere, set `MR`.
- Arm GNU Toolchain 13.2.Rel1 at
  `~/.rtk_bt_soc_tools/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi`.
  If it is elsewhere, set `GCC_BIN` to its `bin/`.
- `cmake`, `west` and `python3` with PyYAML.

## Update the libs

Run from the top of the Zephyr west workspace. A run takes several minutes.

```bash
realtek-zephyr-project/tool/rtl87x2j_lib/update_libs.sh
```

The script runs `west update --rebase` in the multirepo, so you do not need to
switch branches in each project. The new `lib_version` is the committed one
in `lib/rtl87x2j/PROVENANCE.yml` with the patch number bumped (0.1.0 becomes
0.1.1), or 0.1.0 if there is none yet.

Read the warnings the script prints before you push. They may come from Zephyr
itself rather than the libs.

If the lib API changed, give the version yourself and bump the minor number:

```bash
realtek-zephyr-project/tool/rtl87x2j_lib/update_libs.sh --lib-version 0.2.0
```

## Try a local change

1. Edit the sources in the multirepo, e.g. `realtek/driver/...`.
2. Build temporary libs into `lib/rtl87x2j/` and hal_realtek:

   ```bash
   realtek-zephyr-project/tool/rtl87x2j_lib/update_libs.sh --local
   ```

   This builds the multirepo as it is, local changes included, and does not
   commit anything. The script warns about the local changes, and the libs
   get `lib_version: <current>+local`. After each further edit, run it again.
3. Test on the board, e.g. `cd zephyr/samples/subsys/usb/hid-mouse && west flash`.
   Your own app links the temporary libs as well.
4. Put the libs in both repositories back:

   ```bash
   realtek-zephyr-project/tool/rtl87x2j_lib/update_libs.sh --drop
   ```

The script only reverts the files the build writes in the multirepo, so your
own edits there are kept.

## If the script stops

| Message | What to do |
|---|---|
| `build failed: <app>` | Read `~/.cache/rtl87x2j-lib-update/zephyr_<app>.log`. Fix it, run `--drop`, and rerun with `--skip-update`. |
| `west update --rebase failed` | Resolve the rebase in that project, then rerun with `--skip-update`. |
| `uncommitted libs in ...` | Libs from an earlier run are still there. Remove them with `--drop`. |

## Options

| Option | Description |
|---|---|
| `--lib-version X.Y.Z` | Set `lib_version` instead of bumping the patch number. It must differ from the committed one. |
| `--local` | Build temporary libs from the multirepo as it is. No update, no commit. |
| `--drop` | Restore the libs in both repositories to HEAD. |
| `--skip-update` | Do not run `west update --rebase`. |
| `--no-commit` | Leave the new libs uncommitted. |

| Variable | Default |
|---|---|
| `MR` | `~/zephyr/multi_repo_bee5` |
| `ZW` | The west workspace that contains the script. |
| `HAL` | `hal_realtek` in `ZW`. |
| `GCC_BIN` | The `bin/` of the toolchain above. |
| `WORK` | `~/.cache/rtl87x2j-lib-update`. Holds build logs and the frozen manifest. |

## PROVENANCE.yml

`PROVENANCE.yml` records:
- `lib_version`;
- the sha256, defconfig and compile flags of each lib;
- the commit of each multirepo project the lib was built from;
- `build_tool.pr`, the PR of this tool (`TOOL_PR` in `update_libs.sh`).

`clean_sources: false` means the libs were built from local changes. They
cannot be rebuilt from the recorded commits. The changes are listed under
`dirty`.

The frozen manifest is saved to `$WORK/frozen/<lib_version>.yml`. It contains
internal URLs. Do not commit it.
