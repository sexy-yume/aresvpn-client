#!/usr/bin/env python3
"""AresVPN Client - name the SOURCE REVISION of every third-party component we convey.

AresProject ROADMAP 18-3d: *GPL-3 compliance as a mechanism, not a memory*.

GPL-3 section 6 requires the Corresponding Source of every covered component in a binary we
convey. "The source is on GitHub" is not that: a project's default branch moves, so a link
without a revision does not identify the code that produced the bytes we shipped. THIRD_PARTY
LICENSES.md carried exactly that gap in writing - *the exact source revision of the shipped
binary is not yet established* - and it was written as a note for a human to remember.

This is the check instead. It reads the conan recipes in the local cache, classifies how each
one names its source, and REFUSES a reference that moves:

    TAG        get(... /archive/refs/tags/vX.Y.Z.zip)    - immutable, fine
    RELEASE    a GitHub release asset at a fixed version - immutable, fine
    PINNED-URL a raw URL carrying a 40-hex commit         - immutable, fine
    SHA256     download(..., sha256=...)                  - the BYTES are pinned, fine
    GIT-TAG    git.clone(--branch vX.Y.Z)                 - immutable, fine
    GIT-BRANCH git.clone(--branch some-branch)            - **MOVES. This is the defect.**
    UNKNOWN    nothing matched                            - treated as a defect, not as fine

A GIT-BRANCH is only acceptable if the resolved commit is recorded, and this script will read it
out of the conan source folder's git checkout when one exists - which is what turns "pinned to a
branch" into a fact a Corresponding Source offer can carry.

It is proved able to fail by --selftest, which feeds it one recipe of each shape and requires the
branch one to be reported and the tag one not to be (#L020).

    python source-revisions.py                 # report on this machine's conan cache
    python source-revisions.py --out FILE      # also write the manifest
    python source-revisions.py --selftest      # both arms, no cache needed
"""

import argparse
import json
import os
import re
import subprocess
import sys

HEX40 = re.compile(r"\b[0-9a-f]{40}\b")

# The components this product actually conveys. Build tools (cmake, ninja, msys2, go, swig, nasm,
# autoconf...) are NOT conveyed - they never enter a binary a customer receives - so they carry no
# section 6 duty and listing them would bury the ones that do. Naming them explicitly rather than
# filtering by a heuristic: a heuristic that silently drops a shipped component is exactly the
# blindness #L041 is about.
#
# The platform is part of the record because the question is never "is everything pinned" - it is
# "may THIS build be conveyed". `#D178` makes the product Windows and Android; the Apple entries
# are here so a future target is covered rather than discovered, and they must not read as
# blockers for a build that does not contain them.
CONVEYED = {
    # Windows
    "awg-windows": "windows",
    "tap-windows6": "windows",
    "win-split-tunnel": "windows",
    "wintun": "windows",
    # every desktop target
    "amnezia-xray-bindings": "desktop",
    "tun2socks": "desktop",
    "openvpn": "desktop",
    "v2ray-rules-dat": "desktop",
    # every target
    "libssh": "all",
    "openssl": "all",
    "zlib": "all",
    # Apple network extension - NOT a target platform under #D178
    "awg-apple": "apple",
    "hev-socks5-tunnel": "apple",
    "openvpnadapter": "apple",
    "awg-go": "linux-macos",
    # Android
    "amnezia-libxray": "android",
    "awg-android": "android",
    "openvpn-pt-android": "android",
}

# Which of the labels above a given --os actually ships.
PLATFORM_SETS = {
    "windows": {"windows", "desktop", "all"},
    "android": {"android", "all"},
    "apple": {"apple", "all"},
    "all": set(CONVEYED.values()) | {"all"},
}


def classify(text):
    """Return (kind, detail) for one recipe's source reference."""
    # git.clone(url=..., args=[..., "--branch", X])
    m = re.search(r"git\.clone\(", text)
    if m:
        url = re.search(r'url\s*=\s*[f]?"([^"]+)"', text[m.start():])
        branch = re.search(r'"--branch"\s*,\s*[f]?"([^"]+)"', text[m.start():])
        u = url.group(1) if url else "?"
        b = branch.group(1) if branch else "?"
        # A CHECKOUT OF A FULL COMMIT SHA is the most immutable reference there is, and it is what
        # a PIN looks like (#D191): `--branch` accepts a branch or a tag and NOT an arbitrary sha,
        # so pinning a recipe is clone-then-checkout. This is tested BEFORE the branch cases
        # because a pinned recipe still carries a `git.clone` - reading only the clone reported a
        # correct pin as UNPINNED, which is a pin looking identical to no pin at all (#L012).
        # A SHORT sha is deliberately not accepted: it is ambiguous by construction.
        if re.search(r"git\.checkout\(", text):
            c = (re.search(r'=\s*"([0-9a-f]{40})"', text)
                 or re.search(r'checkout\(\s*"([0-9a-f]{40})"', text))
            if c:
                return "GIT-COMMIT", f"{u} @ {c.group(1)}"
        # `v{self.version}` and a literal vX.Y are tags; anything else is a branch that moves.
        if b.startswith("v{self.version}") or re.fullmatch(r"v[0-9][0-9.]*", b):
            return "GIT-TAG", f"{u} @ {b}"
        return "GIT-BRANCH", f"{u} @ {b}"

    if "/archive/refs/tags/" in text:
        m = re.search(r'"(https://[^"]*?/archive/refs/tags/[^"]*)"', text)
        return "TAG", m.group(1) if m else "archive/refs/tags"

    if "/releases/download/" in text:
        m = re.search(r'"(https://[^"]*?/releases/download/[^"]*)"', text)
        return "RELEASE", m.group(1) if m else "releases/download"

    m = HEX40.search(text)
    if m and "raw.githubusercontent.com" in text:
        url = re.search(r'"(https://raw\.githubusercontent\.com[^"]*)"', text)
        return "PINNED-URL", (url.group(1) if url else m.group(0))

    if re.search(r"sha256\s*=", text):
        return "SHA256", "the bytes are pinned by digest in the recipe"

    m = re.search(r'get\(\s*self\s*,\s*(?:url\s*=\s*)?[f]?"(https://[^"]+)"', text)
    if m:
        return "URL", m.group(1)

    return "UNKNOWN", "no source reference matched"


def classify_conandata(recipe_path, version):
    """conan-center recipes keep the URL and its sha256 in conandata.yml, not in the recipe.

    Reading only the recipe reported those as UNKNOWN, which is #L041's shape exactly: the
    instrument going blind produced a line that reads like a finding about the subject. The data
    file is beside the recipe and is the authority for that family.
    """
    data = os.path.join(os.path.dirname(recipe_path), "conandata.yml")
    if not os.path.isfile(data):
        return None
    text = open(data, encoding="utf-8", errors="replace").read()
    # A tiny reader rather than a YAML dependency: find the block for this version under
    # `sources:` and take the first url and sha256 in it.
    m = re.search(r"^sources:\s*$(.*?)(?=^\S|\Z)", text, re.M | re.S)
    if not m:
        return None
    block = m.group(1)
    vm = re.search(re.escape(f'"{version}"') + r":(.*?)(?=^\s{0,4}\"|\Z)", block, re.M | re.S)
    scope = vm.group(1) if vm else block
    # `url:` is sometimes a scalar and sometimes a YAML LIST of mirrors, in which case the value is
    # on the next line behind a `- `. Without the second alternative the match landed on the list
    # dash and the manifest printed `url: -`, which is worse than no line at all - it looks like a
    # URL somebody could follow.
    url = re.search(r'url:\s*(?:\r?\n\s*-\s*)?"?(https?://[^"\s]+)"?', scope)
    sha = re.search(r'sha256:\s*"?([0-9a-fA-F]{64})"?', scope)
    if url and sha:
        return "SHA256", f"{url.group(1)} (sha256 {sha.group(1)[:16]}...)"
    if url:
        return "URL", url.group(1)
    return None


def resolved_commit(cache_pkg_dir):
    """If conan checked the source out with git, read the commit it landed on."""
    for sub in ("s", "b"):
        d = os.path.join(cache_pkg_dir, sub)
        if os.path.isdir(os.path.join(d, ".git")):
            try:
                out = subprocess.run(["git", "-C", d, "rev-parse", "HEAD"],
                                     capture_output=True, text=True, timeout=30)
                if out.returncode == 0:
                    return out.stdout.strip()
            except Exception:
                pass
    return None


def scan(conan_home):
    """name -> record, over every recipe in the cache."""
    found = {}
    root = os.path.join(conan_home, "p")
    if not os.path.isdir(root):
        return found
    for entry in sorted(os.listdir(root)):
        recipe = os.path.join(root, entry, "e", "conanfile.py")
        if not os.path.isfile(recipe):
            continue
        text = open(recipe, encoding="utf-8", errors="replace").read()
        name = re.search(r'^\s*name\s*=\s*"([^"]+)"', text, re.M)
        version = re.search(r'^\s*version\s*=\s*"([^"]+)"', text, re.M)
        if not name:
            continue
        name = name.group(1)
        if name not in CONVEYED:
            continue
        ver = version.group(1) if version else None
        kind, detail = classify(text)
        if kind == "UNKNOWN":
            alt = classify_conandata(recipe, ver or "")
            if alt:
                kind, detail = alt
        # The recipes interpolate `{self.version}`; a manifest that reproduces the f-string is not
        # a reference anybody can follow.
        if ver:
            detail = detail.replace("{self.version}", ver)
        # win-split-tunnel's URL carries the arch triple as a recipe property. Windows x64 is the
        # only one this product builds (#D178), so the manifest names it rather than reproducing
        # the placeholder - a Corresponding Source line has to be followable.
        detail = detail.replace("{self._target}", "x86_64-pc-windows-msvc")
        rec = {
            "name": name,
            "version": ver or "(from the reference)",
            "platform": CONVEYED[name],
            "kind": kind,
            "detail": detail,
            "recipe": recipe,
        }
        if kind in ("GIT-BRANCH", "UNKNOWN"):
            commit = resolved_commit(os.path.join(root, entry))
            if commit:
                rec["resolved_commit"] = commit
        # A package can be in the cache twice (two package_ids); keep the one that says the most.
        prev = found.get(name)
        if prev is None or ("resolved_commit" in rec and "resolved_commit" not in prev):
            found[name] = rec
    return found


def report(found, out_path=None):
    lines = []
    lines.append("AresVPN Client - source revisions of the components this product conveys")
    lines.append("")
    lines.append("GPL-3 section 6 requires the Corresponding Source of every covered component in a")
    lines.append("binary we convey. This file names, for each one, the immutable reference that")
    lines.append("identifies the code the shipped bytes were built from. It is generated - see")
    lines.append("cmake/aresvpn/source-revisions.py - and a component whose source reference MOVES")
    lines.append("fails that script rather than appearing here as a link.")
    lines.append("")
    bad = []
    for name in sorted(found):
        r = found[name]
        lines.append(f"{r['name']} {r['version']}   [{r.get('platform', '?')}]")
        lines.append(f"    {r['kind']}: {r['detail']}")
        if "resolved_commit" in r:
            lines.append(f"    resolved commit: {r['resolved_commit']}")
        lines.append("")
        if r["kind"] in ("GIT-BRANCH", "UNKNOWN") and "resolved_commit" not in r:
            bad.append(r)

    text = "\n".join(lines)
    if out_path:
        with open(out_path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
    return text, bad


RECIPES_FOR_SELFTEST = {
    "tag": ('class X(ConanFile):\n    name = "openvpn"\n    version = "2.7.0"\n'
            '    def source(self):\n'
            '        get(self, f"https://github.com/OpenVPN/openvpn/archive/refs/tags/v{self.version}.zip")\n'),
    "branch": ('class X(ConanFile):\n    name = "openvpn-pt-android"\n    version = "1.0.0"\n'
               '    def source(self):\n        git = Git(self)\n'
               '        git.clone(url="https://github.com/amnezia-vpn/openvpn-pt-android.git",\n'
               '                  target=".", args=["--recurse-submodules", "--branch", "update-ovpn3"])\n'),
    "gittag": ('class X(ConanFile):\n    name = "awg-android"\n    version = "3.1.20260814"\n'
               '    def source(self):\n        git = Git(self)\n'
               '        git.clone(url="https://github.com/amnezia-vpn/amneziawg-android.git",\n'
               '                  target=".", args=["--recurse-submodules", "--branch", f"v{self.version}"])\n'),
    "pinned": ('class X(ConanFile):\n    name = "win-split-tunnel"\n    version = "1.2.5.0"\n'
               '    def build(self):\n'
               '        url = f"https://raw.githubusercontent.com/mullvad/mullvadvpn-app-binaries/'
               'ff0e3746c89a04314377cffeb52faaa976413a69/{self._target}/split-tunnel"\n'),
    "nothing": ('class X(ConanFile):\n    name = "zlib"\n    version = "1.3.2"\n'
                '    def source(self):\n        pass\n'),
}


def selftest():
    expected = {
        "tag": "TAG",
        "branch": "GIT-BRANCH",
        "gittag": "GIT-TAG",
        "pinned": "PINNED-URL",
        "nothing": "UNKNOWN",
    }
    failures = 0
    for key, text in RECIPES_FOR_SELFTEST.items():
        kind, detail = classify(text)
        ok = kind == expected[key]
        print(f"  selftest {key:8s} -> {kind:11s} {'ok' if ok else 'EXPECTED ' + expected[key]}")
        if not ok:
            failures += 1

    # The half that matters: the branch case must be REPORTED as a defect and the tag case must
    # not. A classifier that is right and a report that ignores it would still ship the gap.
    found = {}
    for key in ("tag", "branch", "gittag"):
        kind, detail = classify(RECIPES_FOR_SELFTEST[key])
        nm = re.search(r'name = "([^"]+)"', RECIPES_FOR_SELFTEST[key]).group(1)
        found[nm] = {"name": nm, "version": "x", "kind": kind, "detail": detail, "recipe": "-"}
    _, bad = report(found)
    names = sorted(r["name"] for r in bad)
    if names != ["openvpn-pt-android"]:
        print(f"  selftest report   -> FAILED: refused {names}, expected only openvpn-pt-android")
        failures += 1
    else:
        print("  selftest report   -> ok, the moving branch is refused and the two tags are not")

    print(f"selftest {'PASSED' if failures == 0 else 'FAILED'} ({failures} failure(s))")
    return 0 if failures == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--conan-home", default=os.path.join(os.path.expanduser("~"), ".conan2"))
    ap.add_argument("--os", default="all", choices=sorted(PLATFORM_SETS),
                    help="the build being asked about. A component this platform does not ship "
                         "is listed and never blocks it.")
    ap.add_argument("--out")
    ap.add_argument("--json")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--allow-unpinned", action="store_true",
                    help="report a moving reference without failing. For a DEVELOPMENT build only: "
                         "nothing may be conveyed while this is needed.")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    found = scan(args.conan_home)
    if not found:
        # An empty scan is #L041's floor: a report of nothing must not read as a clean bill.
        print(f"FATAL: no conveyed component found under {args.conan_home}/p - either the cache is "
              f"empty or this script has gone blind. It is not evidence that nothing needs a "
              f"revision.")
        return 2

    text, bad = report(found, args.out)
    print(text)
    if args.json:
        with open(args.json, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(found, fh, indent=2, sort_keys=True)

    shipped = PLATFORM_SETS[args.os]
    blocking = [r for r in bad if r.get("platform") in shipped]
    other = [r for r in bad if r.get("platform") not in shipped]
    print(f"{len(found)} conveyed component(s) examined for --os {args.os}; "
          f"{len(blocking)} without an immutable source reference")
    for r in blocking:
        print(f"  UNPINNED  {r['name']} {r['version']} [{r.get('platform')}]: {r['kind']} {r['detail']}")
    for r in other:
        # Said out loud rather than filtered away: a silent cap reads as "covered everything".
        print(f"  (not shipped on {args.os}, so not blocking) {r['name']} [{r.get('platform')}]: "
              f"{r['kind']} {r['detail']}")
    bad = blocking
    if bad and not args.allow_unpinned:
        print("A component whose source reference MOVES cannot be conveyed: the Corresponding "
              "Source offer would not identify the code the binary was built from (GPL-3 s6).")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
