# GitHub C2

Cobalt Strike **User-Defined C2** (UDC2) channel that carries Beacon frames in GitHub pull-request comments. The implant is a Windows x64 BOF. A small Python relay unwraps those comments and shuttles unmodified frames to a Team Server UDC2 listener.

Protocol and proxy contract follow [Cobalt-Strike/udc2-vs](https://github.com/Cobalt-Strike/udc2-vs).

```
Beacon [UDC2 BOF]  <-->  GitHub PR comments  <-->  github_udc2_server.py  <-->  Team Server
```

For authorized Offensive Security use only.

## How it works

1. `go()` opens a unique `c2/<32-hex>` branch, commits a dummy blob, and opens a PR against the repo default branch.
2. Beacon calls `udc2Proxy()` on each check-in. The BOF posts the complete CS frame and waits for the matching reply.
3. The relay watches open `c2/*` PRs. For each Beacon it opens one TCP session to the UDC2 listener, sends the initial `"go"` frame, then forwards frames in both directions. It does not decrypt Beacon.
4. Sleep and jitter come from the Cobalt Strike profile, not from a compile-time poll loop.

A frame is the udc2-vs layout:

```
| 4-byte little-endian payload length | payload |
```

GitHub issue comments are size-capped, so a large frame is split into sequential base64 slices of the **same** CS bytes. That is transport only — not a second packet format. Comment bodies are opaque base64; there is no plaintext check-in and no HTML command protocol.

## Layout

| Path | Role |
|---|---|
| `src/udc2_bof.c` | BOF entry: `go` / `udc2Proxy` / `udc2Close` |
| `src/github.c` | WinHTTP GitHub client, PR bootstrap, frame post/wait |
| `src/main.c` | Standalone exe that only bootstraps a PR (no Beacon) |
| `scripts/github_udc2_server.py` | Operator relay to the UDC2 listener |
| `scripts/boflint.py` | BOF object linter (`python3`, stdlib only) |
| `config.mk.example` | Compile-time owner/repo/PAT template |

`config.mk` is gitignored. Never commit a PAT.

## Channel repository

Point the BOF and relay at a **dedicated empty private GitHub repo**, not this source tree. The implant opens `c2/<hex>` branches and PRs against that repo's default branch. Mixing channel traffic with development history leaves leftover PRs, comments, and branches in the source repo.

Create the channel repo once, grant the PAT **Contents**, **Pull requests**, and **Issues** write, then put that owner/repo/PAT in `config.mk`.

## Requirements

- `x86_64-w64-mingw32-gcc`
- Python 3 (relay + `boflint.py`)
- Cobalt Strike 4.12+ with a UDC2 listener
- Fine-grained or classic GitHub PAT on the **operator-controlled channel repo** with **Contents**, **Pull requests**, and **Issues** write

## Build

```bash
cp config.mk.example config.mk
# edit GITHUB_OWNER, GITHUB_REPO, GITHUB_PAT

make exe          # bin/github-c2.exe
make bof          # bin/github-c2.x64.o
make lint-bof     # python3 scripts/boflint.py --loader cs bin/github-c2.x64.o
```

Owner/repo/PAT are compiled into the binary via `-D` flags. Rebuild after changing `config.mk`. CI builds with dummy values.

## Standalone exe

`github-c2.exe` only proves GitHub access: it opens a PR, prints the URL, and exits.

```text
https://github.com/<owner>/<repo>/pull/<n>
```

Expect head ref `c2/<32 hex chars>`, the same id as the PR title and dummy blob path, and no check-in comment.

## UDC2 BOF

1. Create a UDC2 listener in Cobalt Strike (**not** debug-only). Set the BOF path to `bin/github-c2.x64.o`.
2. Start the relay with network visibility of that listener:

```bash
python3 scripts/github_udc2_server.py \
  --owner <owner> --repo <repo> --pat <pat> \
  --ts-addr <team-server> --ts-port <udc2-port> -v
```

`--pr N` pins a single pull request; the default is every open `c2/*` PR. `--pat` can also come from `GITHUB_PAT` or `GITHUB_TOKEN`.

3. Export a payload for that listener and run it on Windows x64.
4. A new `c2/<hex>` PR appears. The relay logs a TCP connect, the `"go"` frame, then `client N bytes` / `posted server frame`. The Beacon checks in on the Team Server.

Issue tasks as usual. Output returns on the next `udc2Proxy` after Beacon sleep/jitter. Two payloads produce two PRs and two Team Server TCP sessions.

## Notes

- GitHub issue-comment IDs are 64-bit. The BOF parses them as `unsigned long long` (32-bit `strtoul` saturates and the implant never sees the Team Server reply).
- `udc2Proxy` returning `-1` or `0` resets the Beacon session (udc2-vs).
- Close leftover `c2/*` test PRs when finished.
- Do not paste the PAT into git, CI logs you publish, or chat.

## License

MIT. Authorized offensive-security use only.
