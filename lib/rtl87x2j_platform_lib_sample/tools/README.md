# Platform library provenance

`gen-provenance.sh` records which commits `librtl87x2j_platform_lib.a` was built
from, so the archive uploaded to the server can be traced and rebuilt.

## When to run it

After building the archive for publishing, before uploading it. The archive must
come from committed sources: the script checks every project of the bee5
workspace (including its manifest repository) and this repository, and refuses
to continue if any of them has uncommitted or untracked files.

## Usage

```sh
tools/gen-provenance.sh \
    --lib /path/to/librtl87x2j_platform_lib.a \
    --lib-version 0.1.0 \
    --bee5 ~/bee5 \
    --cflags "<flags passed to the library Makefile>" \
    --out ./provenance
```

Options `--west`, `--cc` select the `west` and compiler binaries when they are
not in `PATH`. `--allow-dirty` writes the record anyway and marks it
`reproducible: false`; use it only for local experiments.

## Outputs

| File | Content | Commit to a public repo |
|------|---------|:-----------------------:|
| `PROVENANCE.yml` | library version, archive sha256, compiler, flags, SHA of the bee5 manifest, of this repository and of every bee5 project | yes |
| `bee5-frozen.yml` | `west manifest --freeze` of the bee5 workspace, contains internal URLs | **no** |
| `<archive>.sha256` | checksum of the archive | yes |

Upload `bee5-frozen.yml` next to the archive on the server. To rebuild the
archive from it:

```sh
mkdir ws && cd ws
west init -l <manifest repo containing bee5-frozen.yml>   # or copy the file in
west update
```

## Versioning

`lib_version` is the version of the library itself and is independent of the
Zephyr release version. Bump it whenever the archive content changes, and state
it (with the first 8 characters of `archive_sha256`) in the release note of the
Zephyr release that uses it.
