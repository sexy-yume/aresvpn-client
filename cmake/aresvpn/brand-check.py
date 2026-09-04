#!/usr/bin/env python3
"""AresVPN Client - does any screen a customer can REACH still carry Amnezia's marks?

AresProject ROADMAP 18-3b / 18-3d, #D178 (their name is not licensed by the GPL), #D180 rule 3.

THE QUESTION THIS ANSWERS, and why the obvious version of it is useless. `grep -r Amnezia client/`
returns hundreds of hits and most of them MUST stay: 120 MPL-2.0 headers, 11 copyright and
modification notices, every container key (`amnezia-awg`), the About page's attribution, and the
protocol clarification "AresWG (AmneziaWG)". A count over all of them is noise, and noise is how a
check stops being read (`#L030`). So this asks the narrower question that actually matters:

    of the strings a customer can SEE, on a screen this product can REACH, does any carry the mark?

REACHABILITY IS DERIVED, NOT TYPED. Starting from the pages this fork's routing actually enters,
it follows `PageEnum.X` mentions from file to file and takes the transitive closure. A page nobody
can navigate to is reported and does not fail the run - its strings ship in the binary but no
customer meets them, and rewriting a screen we do not use costs an upstream merge for nothing
(#D177 rule 3). A page that IS reachable and carries the mark fails, loudly, with the line.

The exceptions are listed one by one with their reason, never as a pattern - an exception list that
matches by shape is how the next real one gets waved through.

Usage:
    python brand-check.py [--root <fork root>]
    python brand-check.py --selftest
"""

import argparse
import os
import re
import sys

BRAND = re.compile(r"Amnezia|AMNEZIA|amnezia")

# A string a customer reads. THREE forms, and the first version of this had only two - it missed
# `{ DockerContainer::Awg, "AmneziaWG" }` and `return "AmneziaWG Legacy";`, which are the protocol
# LABELS on the connect screen, and it missed the later fragments of a C++ adjacent-literal
# concatenation so that "AmneziaWG is an excellent choice" inside a tr() was invisible. Both were
# found by reading the SHIPPED BINARY's UTF-16 text rather than the source (#L009: a call-site
# regex is structurally incomplete; #L019: assert on the artefact).
#
#   1. qsTr("...") / tr("..." "..." "...")  - the whole argument, so every fragment is seen
#   2. a text-ish QML property assigned a literal
#   3. EVERY literal in a file that is nothing but a label table (LABEL_TABLES below)
USER_STRING = re.compile(
    r"(?:qsTr|tr)\s*\(\s*((?:\"(?:[^\"\\]|\\.)*\"\s*)+)"
    r"|(?:text|headerText|descriptionText|title|placeholderText|textString)\s*:\s*(\"(?:[^\"\\]|\\.)*\")"
)

ANY_LITERAL = re.compile(r"\"((?:[^\"\\]|\\.)*)\"")

# Files whose string literals ARE the user-visible labels. Scanning every literal everywhere
# would drown in container keys and settings paths; scanning every literal in these two is right,
# because that is all they contain.
LABEL_TABLES = ("containerUtils.cpp", "containersModel.cpp")

# ...but those two files hold KEYS as well as labels, and #D180 rule 3 keeps every key. A key is
# lower-case and hyphenated (`amnezia-awg`, `amnezia-openvpn-cloak`) or a bare prefix; a label is
# prose. Scanning every literal without this brought `amnezia-awg` back as a finding, which is the
# opposite of the decision.
CONTAINER_KEY = re.compile(r"^[a-z0-9]+(?:[-_/][a-z0-9]*)+$|^amnezia-?$")

PAGE_REF = re.compile(r"PageEnum\.(\w+)")

# Where this product's navigation actually starts. PageStart is the shell, main2.qml is always
# loaded, and the other three are #D182's own screens.
ROOTS = ["PageStart", "PageHome", "PageSettings", "PageSetupWizardAresLogin", "PageAresRents"]

# DEAD SURFACE. These screens belong to products this fork does not sell - Amnezia's Premium
# catalogue and the self-hosted server installer - and 18-3e closes their front doors with a
# RUNTIME gate rather than by deleting the files (deleting them is the merge-killing shape,
# #D177 rule 3). The reachability walk below follows a `PageEnum.X` MENTION, and a mention is
# not a route, so it cannot see that gate: it reports these as reachable and it is wrong to.
#
# THE IMPORTANT PART IS WHY THE MARK STAYS, and it is not laziness. These strings say
# "Amnezia Premium subscription", "Configure Amnezia VPN on your own server", "the Amnezia app".
# Rewriting them to AresVPN would not remove a trademark, it would CLAIM A PRODUCT WE DO NOT
# SELL - a worse defect than the one it fixes. The right treatment for a dead surface is to
# leave it saying what it is.
DEAD_SURFACE = {
    "PageSettingsApiInstructions.qml": "Amnezia Premium setup guides - a product we do not sell",
    "PageSettingsApiNativeConfigs.qml": "Amnezia Premium config export",
    "PageSettingsApiSubscriptionKey.qml": "Amnezia Premium subscription key",
    "PageSettingsApiServerInfo.qml": "Amnezia Premium server info",
    "PageSettingsApiDevices.qml": "Amnezia Premium device list",
    "PageSettingsApiSupport.qml": "Amnezia Premium support",
    "PageSettingsApiAvailableCountries.qml": "Amnezia Premium country list",
    "PageSetupWizardApiServicesList.qml": "the Premium services catalogue",
    "PageSetupWizardConfigSource.qml": "the self-hosted install wizard's entry",
    "PageSetupWizardCredentials.qml": "the self-hosted install wizard's SSH credentials",
    "PageSetupWizardInstalling.qml": "the self-hosted install wizard's progress",
    "PageSettingsServerData.qml": "administering a server the customer installed",
    "PageSettingsServerProtocol.qml": "a container's protocol on a self-hosted server",
    "PageServiceDnsSettings.qml": "AmneziaDNS on a self-hosted server",
    "PageShareConnection.qml": "sharing a self-hosted server's config",
    "PageUpdate.qml": "Amnezia's own update channel, which 18-3e guards off",
}

# THREE FILES ARE MIXED, and putting them in the dict above was wrong for a measurable reason:
# it exempted the WHOLE file, and `containerUtils.cpp` also holds the protocol LABELS that
# #D180 rule 3 says must become Ares marks. The exemption jumped from 29 strings to 40 and
# swallowed `{ DockerContainer::Awg, "AmneziaWG" }` on the way. So a mixed file is exempted by
# SUBSTRING, one line per string, exactly like ALLOWED - never by name.
DEAD_SURFACE_STRINGS = [
    ("main2.qml", "legacy Amnezia subscription type",
     "a dialog for a subscription we never issue"),
    ("errorStrings.cpp", "legacy Amnezia subscription format",
     "a Premium error code that cannot be raised here"),
    ("errorStrings.cpp", "Amnezia Premium subscription has expired",
     "a Premium error code that cannot be raised here"),
    ("containerUtils.cpp", "Amnezia will create a",
     "one sentence about SFTP file storage on a server the customer installed"),
]

# Every exception, one line each, with the reason. Matched on (file basename, substring).
ALLOWED = [
    ("PageSettingsAbout.qml", "marks of their owners",
     "the attribution notice itself - GPL-3 section 5 and #D178. Removing the mark HERE would be "
     "the licence violation, not the other way round."),
    ("containerUtils.cpp", "AresWG (AmneziaWG)",
     "#D180 rule 3: the LABEL is AresWG, the protocol on the wire is AmneziaWG. Naming it once in "
     "parentheses is a technical fact a customer needs, not a brand claim."),
    ("containersModel.cpp", "AresWG (AmneziaWG)",
     "same string, same reason - the model repeats the description."),
]


def read(path):
    try:
        return open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return ""


def page_files(root):
    """PageEnum key -> file, by upstream's own rule: getPagePath maps a key to Pages2/<key>.qml."""
    out = {}
    pages_dir = os.path.join(root, "client", "ui", "qml", "Pages2")
    if not os.path.isdir(pages_dir):
        return out
    for name in os.listdir(pages_dir):
        if name.endswith(".qml"):
            out[name[:-4]] = os.path.join(pages_dir, name)
    return out


def reachable_pages(root, roots=None):
    pages = page_files(root)
    seen = set()
    # main2.qml is loaded unconditionally, so anything it names is reachable too.
    queue = list(roots if roots is not None else ROOTS)
    extra = os.path.join(root, "client", "ui", "qml", "main2.qml")
    for key in PAGE_REF.findall(read(extra)):
        queue.append(key)
    while queue:
        key = queue.pop()
        if key in seen or key not in pages:
            seen.add(key)
            continue
        seen.add(key)
        for nxt in PAGE_REF.findall(read(pages[key])):
            if nxt not in seen:
                queue.append(nxt)
    return seen & set(pages), pages


def user_strings(text, basename=""):
    if basename in LABEL_TABLES:
        for m in ANY_LITERAL.finditer(text):
            yield m.group(1), m.start()
        return
    for m in USER_STRING.finditer(text):
        raw = m.group(1) if m.group(1) is not None else m.group(2)
        # join the adjacent literals of a C++ concatenation into the one string a reader sees
        joined = "".join(ANY_LITERAL.findall(raw))
        yield joined, m.start()


def line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def allowed_reason(basename, string):
    for f, needle, why in ALLOWED:
        if basename == f and needle in string:
            return why
    return None


def scan(root, roots=None, ignore_dead=False):
    reach, pages = reachable_pages(root, roots)
    failures, unreachable_hits, allowed_hits, dead_hits = [], [], [], []
    keys_skipped = []

    # every page, plus the non-page sources that produce customer text
    targets = dict(pages)
    for rel in ("client/core/utils/containers/containerUtils.cpp",
                "client/core/utils/errorStrings.cpp",
                "client/ui/models/containersModel.cpp",
                "client/ui/qml/main2.qml"):
        p = os.path.join(root, *rel.split("/"))
        if os.path.exists(p):
            targets["<" + os.path.basename(p) + ">"] = p

    for key, path in sorted(targets.items()):
        text = read(path)
        base = os.path.basename(path)
        # a non-page source is always reachable; a page is reachable only if the graph says so
        is_reachable = key.startswith("<") or key in reach
        for s, off in user_strings(text, base):
            if not BRAND.search(s):
                continue
            if base in LABEL_TABLES and CONTAINER_KEY.match(s):
                # #D180 rule 3 in as many words: the LABELS change, the container keys and every
                # identifier stay. `amnezia-awg` is the key the node and the config agree on.
                # Counted, not dropped - a silent skip is how a bucket total stops adding up.
                keys_skipped.append((base, line_of(text, off), s[:96]))
                continue
            why = allowed_reason(base, s)
            row = (base, line_of(text, off), s[:96])
            if why:
                allowed_hits.append(row + (why,))
                continue
            dead_why = DEAD_SURFACE.get(base)
            if dead_why is None:
                for f, needle, w in DEAD_SURFACE_STRINGS:
                    if base == f and needle in s:
                        dead_why = w
                        break
            if dead_why and not ignore_dead:
                dead_hits.append(row + (dead_why,))
            elif is_reachable:
                failures.append(row)
            else:
                unreachable_hits.append(row)
    return failures, unreachable_hits, allowed_hits, dead_hits, keys_skipped, reach, pages


def run(root):
    failures, unreachable, allowed, dead, keys, reach, pages = scan(root)

    print("pages: %d total, %d reachable from %s" % (len(pages), len(reach), ", ".join(ROOTS)))
    if dead:
        by_file = {}
        for base, _line, _s, why in dead:
            by_file.setdefault((base, why), 0)
            by_file[(base, why)] += 1
        print("\nDEAD SURFACE - the mark STAYS here on purpose. These screens are Amnezia's")
        print("Premium catalogue and self-hosted installer, which 18-3e gates off at runtime.")
        print("Rewriting them to AresVPN would not remove a trademark, it would claim a product")
        print("we do not sell:")
        for (base, why), n in sorted(by_file.items()):
            print("  %-40s %2d string(s)  - %s" % (base, n, why))
    if allowed:
        print("\nDELIBERATE - each listed with its reason, never matched by shape:")
        for base, line, s, why in allowed:
            print("  %s:%d  %s" % (base, line, s))
            print("      %s" % why)
    if unreachable:
        print("\nUNREACHABLE - these ship in the binary and no customer can navigate to them, so")
        print("they are reported rather than swept (rewriting a screen we do not use costs an")
        print("upstream merge for nothing - #D177 rule 3):")
        for base, line, s in unreachable:
            print("  %-42s :%-5d %s" % (base, line, s))
    if failures:
        print("\nFAIL - a REACHABLE screen carries the mark:")
        for base, line, s in failures:
            print("  %-42s :%-5d %s" % (base, line, s))
    print("\n%d on a reachable customer screen, %d unreachable, %d dead surface, %d deliberate"
          " and %d container key(s) kept by #D180 rule 3"
          % (len(failures), len(unreachable), len(dead), len(allowed), len(keys)))
    # A floor: if the walker reached nothing, its zero means blindness (#L041).
    if len(reach) < 3:
        print("FAIL the reachability walk found %d pages - that is a broken walker, not a clean "
              "tree" % len(reach))
        return 1
    return 1 if failures else 0


def run_selftest(root):
    ok = True

    def check(label, cond):
        nonlocal ok
        print("  %-62s %s" % (label, "ok" if cond else "FAIL"))
        if not cond:
            ok = False

    reach, pages = reachable_pages(root)
    check("the walk finds the home screen", "PageHome" in reach)
    check("the walk finds the rent list from the home screen", "PageAresRents" in reach)
    check("the walk finds the login", "PageSetupWizardAresLogin" in reach)
    check("it did not simply reach everything", len(reach) < len(pages))
    # NOTE what is NOT asserted here, and it is the honest half. An earlier version asserted the
    # walk does NOT reach the Premium and self-hosted screens - and it FAILED, because 18-3e
    # closes those doors with a RUNTIME gate and this walk follows a textual `PageEnum.X`
    # mention. A mention is not a route. Rather than weaken the walk or pretend, those files are
    # named in DEAD_SURFACE with the reason the mark stays on them (#L015: instrument first).

    failures, unreachable, allowed, dead, keys, _r, _p = scan(root)
    check("no customer-facing screen carries the mark today", failures == [])
    check("the deliberate uses are found and named", len(allowed) >= 3)
    check("the dead surface is reported rather than swept", len(dead) > 0)

    # CONSERVATION. Every brand string the scanner sees must land in exactly one bucket. Without
    # this, a category added later could silently swallow strings and the headline would read 0
    # because nothing looked (#L041). Counted independently, from the files rather than from the
    # buckets, so the two numbers are not the same measurement twice.
    seen = 0
    _r2, pages2 = reachable_pages(root)
    files = list(pages2.values()) + [
        os.path.join(root, *p.split("/")) for p in (
            "client/core/utils/containers/containerUtils.cpp",
            "client/core/utils/errorStrings.cpp",
            "client/ui/models/containersModel.cpp",
            "client/ui/qml/main2.qml")]
    for p in files:
        for s, _off in user_strings(read(p), os.path.basename(p)):
            if BRAND.search(s):
                seen += 1
    check("every brand string lands in exactly one bucket (%d seen)" % seen,
          seen == len(failures) + len(unreachable) + len(allowed) + len(dead) + len(keys))

    # PLANT ONE: with the dead-surface exemption switched off, those same strings MUST fail.
    # That is what proves the check can fail at all rather than exempting its way to green.
    f2, _u2, _a2, _d2, _k2, _r2, _p2 = scan(root, ignore_dead=True)
    check("...and with the exemption off the very same strings FAIL, so it is not blind",
          len(f2) > len(failures))

    print("selftest: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument("--root", default=os.path.abspath(os.path.join(here, "..", "..")))
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    return run_selftest(args.root) if args.selftest else run(args.root)


if __name__ == "__main__":
    sys.exit(main())
