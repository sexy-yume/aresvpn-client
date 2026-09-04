#!/usr/bin/env python3
"""AresVPN Client - derive the fork's .ts files from upstream's, without editing upstream's.

AresProject ROADMAP 18-3b mechanism 3 / 18-3h, and #D180 rule 3.

WHY THIS EXISTS. `client/translations/` is upstream's, it churns (104k lines in four months),
and editing it turns every upstream release into a merge conflict on ten large XML files. But
599 translated strings across those ten languages still carry Amnezia's marks, and the GPL
licenses their code, not their name (#D177, #D178). So the files are read, never written: this
produces `aresvpnclient_<locale>.ts` into a build directory and `CLIENT_TS_FILES` points the
build at those instead. Upstream's files stay byte-identical and merge clean for ever.

WHAT IT DOES, and the boundary is the point:
  * it rewrites brand names ONLY inside <translation> and <numerusform> text - the translated
    string a customer reads;
  * it NEVER touches <source>. A .ts entry is matched to the code BY its source text, so a
    changed <source> silently unmatches the entry and the string falls back to English. The
    check below asserts every source byte is identical, because that failure is invisible;
  * it NEVER touches <name> (the context, which is a C++/QML class name), <comment> or
    <extracomment>;
  * it REFUSES to rewrite inside a URL or an email address. Measured before deciding: every
    lowercase `amnezia` in these files is one of `support@amnezia.org`,
    `github.com/amnezia-vpn/amnezia-client`, six `connect-amnezia-premium` documentation deep
    links, or a `t.me/amnezia_vpn*` handle. Turning those into ares-vpn.org would invent
    support channels that do not exist yet - an #L007 input the operator settles, not something
    to guess - and the github URL is upstream ATTRIBUTION, which a fork keeps. They are left
    alone and REPORTED, so the residue is a number on screen rather than a silence.

THE MAP was measured, not assumed (`--report` prints what is in the files). The brand is written
in Latin script in all ten locales, with the locale's own suffixes attached - Korean
`AmneziaWG는`, Urdu `AmneziaWG،`, Hindi `Amneziaडब्ल्यूजी` - which plain substring replacement
preserves. hi_IN is the only locale that also transliterates (`एम्नेज़िया`, twice).

Usage:
    python derive-translations.py --in <upstream translations dir> --out <dir>
    python derive-translations.py --report --in <dir>      # what the brand looks like today
    python derive-translations.py --selftest               # prove it can fail
"""

import argparse
import os
import re
import sys

# Ordered longest-first: AmneziaWG must be seen before Amnezia, or every WG becomes AresVPNWG.
BRAND_MAP = [
    ("AmneziaWG", "AresWG"),
    ("AmneziaDNS", "AresDNS"),
    ("AmneziaVPN", "AresVPN"),
    ("AMNEZIAWG", "ARESWG"),
    ("AMNEZIADNS", "ARESDNS"),
    ("AMNEZIAVPN", "ARESVPN"),
    ("AMNEZIA", "ARESVPN"),
    ("Amnezia", "AresVPN"),
    # hi_IN transliterates the name; the others do not (measured with --report).
    ("एम्नेज़िया", "AresVPN"),
]

# A URL or an email is left exactly as it is - see the docstring. These are masked before the
# brand map runs and restored afterwards, so a replacement cannot reach inside one.
PROTECTED = re.compile(
    r"(?:https?://[^\s<>\"']+"
    r"|mailto:[^\s<>\"']+"
    r"|[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})"
)

# Only these carry text a customer reads.
TRANSLATED = re.compile(r"(<(translation|numerusform)(?:\s[^>]*)?>)(.*?)(</\2>)", re.S)

BRAND_ANY = re.compile(
    r"[^\s<>\"'(),.;:!?]*"
    r"(?:Amnezia|AMNEZIA|amnezia|एम्नेज़िया)"
    r"[^\s<>\"'(),.;:!?]*"
)

SOURCE_RE = re.compile(r"<source>(.*?)</source>", re.S)
NAME_RE = re.compile(r"<name>(.*?)</name>", re.S)
MESSAGE_RE = re.compile(r"<message[\s>]")


def rewrite_text(text):
    """Rewrite the brand in one translated string. Returns (new_text, protected_hits)."""
    holes = []

    def stash(m):
        holes.append(m.group(0))
        # A placeholder that no brand pattern and no XML parser can touch.
        return "\x00%d\x00" % (len(holes) - 1)

    masked = PROTECTED.sub(stash, text)
    for old, new in BRAND_MAP:
        masked = masked.replace(old, new)
    for i, hole in enumerate(holes):
        masked = masked.replace("\x00%d\x00" % i, hole)
    protected_hits = [h for h in holes if BRAND_ANY.search(h)]
    return masked, protected_hits


MESSAGE_BLOCK = re.compile(r"<message(?:\s[^>]*)?>.*?</message>", re.S)


def derive(src_text):
    """Rewrite a whole .ts document. Returns (text, stats).

    The rewrite runs per <message>, not per <translation>, because one thing has to be decided
    with the source in hand: **a translation byte-identical to its source is not a translation.**
    Sixty entries in these files are localisable URL SLUGS - `<source>` and `<translation>` both
    read `documentation/instructions/connect-amnezia-premium#windows`, on the Premium
    instructions screen. Rewriting one would make the pair diverge for no benefit (the fallback
    already shows the source, which is ours), and it is a path rather than a name. Skipping is
    both narrower and more correct than widening the URL filter to swallow them, which is the
    move `#L017` warns about: never widen the guard to silence the loud output.
    """
    stats = {"changed": 0, "protected": [], "same_as_source": 0}

    def one_message(mm):
        block = mm.group(0)
        sources = SOURCE_RE.findall(block)
        source = sources[0] if sources else None

        def one(m):
            open_tag, _kind, body, close_tag = m.group(1), m.group(2), m.group(3), m.group(4)
            if source is not None and body.strip() == source.strip():
                stats["same_as_source"] += 1
                return open_tag + body + close_tag
            new_body, prot = rewrite_text(body)
            if new_body != body:
                stats["changed"] += 1
            stats["protected"].extend(prot)
            return open_tag + new_body + close_tag

        return TRANSLATED.sub(one, block)

    return MESSAGE_BLOCK.sub(one_message, src_text), stats


def assert_structure(before, after, path):
    """Everything that must NOT have moved. A failure here is fatal, never a warning."""
    problems = []
    if MESSAGE_RE.findall(before).__len__() != MESSAGE_RE.findall(after).__len__():
        problems.append("the <message> count changed")
    if SOURCE_RE.findall(before) != SOURCE_RE.findall(after):
        problems.append("a <source> changed - the entry would silently stop matching the code")
    if NAME_RE.findall(before) != NAME_RE.findall(after):
        problems.append("a <name> (context) changed")
    if len(after) < len(before) * 0.9:
        problems.append("the file shrank by more than 10%% (%d -> %d bytes)" % (len(before), len(after)))
    if problems:
        raise SystemExit("FATAL %s: %s" % (path, "; ".join(problems)))


def residue(after):
    """Brand occurrences still in translated text after the rewrite, each with WHY it survived.

    Three categories, and only the third is a problem. A category is how this reports rather
    than hides: a count of 0 that was reached by not looking is the failure `#L041` names.
    """
    out = []
    for mm in MESSAGE_BLOCK.finditer(after):
        block = mm.group(0)
        sources = SOURCE_RE.findall(block)
        source = sources[0] if sources else None
        for m in TRANSLATED.finditer(block):
            body = m.group(3)
            same = source is not None and body.strip() == source.strip()
            protected_spans = [(x.start(), x.end()) for x in PROTECTED.finditer(body)]
            for b in BRAND_ANY.finditer(body):
                if same:
                    # Two very different things end up here and lumping them was the first
                    # version's mistake. A SLUG (a path, a url, an identifier) is fine. A brand
                    # NAME left untranslated means the ENGLISH SOURCE still carries the mark -
                    # which is a string in the code, not a translation, and rewriting the
                    # translation would hide it instead of fixing it.
                    token = b.group(0)
                    why = ("same-as-source-slug"
                           if ("/" in token or "@" in token or "." in token)
                           else "SOURCE-STRING-STILL-CARRIES-THE-MARK")
                elif any(s <= b.start() and b.end() <= e for s, e in protected_spans):
                    why = "url-or-email"
                else:
                    why = "UNPROTECTED"
                out.append((b.group(0), why))
    return out


def run_report(indir):
    import collections
    per = collections.Counter()
    for name in sorted(os.listdir(indir)):
        if not name.endswith(".ts"):
            continue
        text = open(os.path.join(indir, name), encoding="utf-8").read()
        for m in TRANSLATED.finditer(text):
            for b in BRAND_ANY.finditer(m.group(3)):
                per[b.group(0)] += 1
    for word, count in per.most_common():
        print("%6d  %s" % (count, word))
    print("%6d  TOTAL brand occurrences in translated text" % sum(per.values()))
    return 0


def merge_overlay(after, overlay_text, name):
    """Add the fork's OWN strings to a derived .ts.

    AresProject ROADMAP 18-3h. The derivation above rewrites brand names inside translations that
    ALREADY EXIST; it cannot invent an entry for a string upstream never had, and the screens this
    phase rebuilt are full of those. Measured on the eight screens we own: **93 unique qsTr strings,
    22 already Korean, 71 not** - which is why a render on the real Windows platform showed a
    Korean "설정" beside an English "Rents" on the same screen.

    The merge is deliberately the simplest thing that is correct: a .ts file may carry the same
    context name more than once and Qt loads every message in the file, so the overlay's <context>
    blocks are INSERTED before </TS> rather than spliced into the matching context. Nothing
    upstream wrote is touched, and the two halves stay separable by eye in the output.

    It REFUSES a source that already exists in the derived file with a non-empty translation. That
    is the one thing that could go wrong silently - an overlay entry quietly shadowing, or being
    shadowed by, upstream's - and it would look exactly like a working build.
    """
    ours = re.findall(r"<context>.*?</context>", overlay_text, re.S)
    if not ours:
        raise SystemExit("FATAL: the overlay for %s has no <context> block - it would add nothing, "
                         "and a silent no-op here ships an untranslated UI (#L041)" % name)

    # A Qt translation is keyed by (CONTEXT, source), not by source alone, and the first version
    # of this check compared sources across the whole file. It refused four strings whose only
    # upstream translation lives in a DIFFERENT context - and dropping them from the overlay is
    # what put an English "Support" on an otherwise Korean About screen. Found by rendering and
    # looking (#L055); the check was over-strict, which is the rarer half of #L020 and still a
    # defect, because it silently removed correct work.
    existing = set()
    for ctx in re.finditer(r"<context>(.*?)</context>", after, re.S):
        name = re.search(r"<name>(.*?)</name>", ctx.group(1), re.S)
        ctxname = name.group(1) if name else ""
        for m in re.finditer(r"<source>(.*?)</source>\s*<translation(?:\s[^>]*)?>(.*?)</translation>",
                             ctx.group(1), re.S):
            if m.group(2).strip():
                existing.add((ctxname, m.group(1)))

    added = 0
    clashes = []
    for block in ours:
        name = re.search(r"<name>(.*?)</name>", block, re.S)
        ctxname = name.group(1) if name else ""
        for m in re.finditer(r"<source>(.*?)</source>", block, re.S):
            if (ctxname, m.group(1)) in existing:
                clashes.append(ctxname + " / " + m.group(1))
            added += 1
    if clashes:
        raise SystemExit("FATAL: %d overlay string(s) already carry a translation upstream, so one "
                         "of the two would silently win: %s"
                         % (len(clashes), "; ".join(sorted(set(clashes))[:4])))

    marker = "</TS>"
    assert after.count(marker) == 1, "a .ts with no single </TS> is not a .ts"
    body = "\n<!-- AresVPN Client: the strings this fork added (cmake/aresvpn/translations). -->\n"
    body += "\n".join(ours) + "\n"
    return after.replace(marker, body + marker, 1), added


def run_derive(indir, outdir, prefix, overlaydir=None):
    os.makedirs(outdir, exist_ok=True)
    total_changed = 0
    total_unprotected = 0
    total_added = 0
    files = 0
    protected_kinds = {}
    for name in sorted(os.listdir(indir)):
        if not name.endswith(".ts"):
            continue
        locale = name.rsplit("_", 2)
        if len(locale) < 3:
            raise SystemExit("FATAL: cannot read a locale out of %s" % name)
        suffix = locale[-2] + "_" + locale[-1]
        before = open(os.path.join(indir, name), encoding="utf-8").read()
        after, stats = derive(before)
        assert_structure(before, after, name)
        left = residue(after)
        unprotected = [w for w, why in left if why == "UNPROTECTED"]
        for w, why in left:
            if why != "UNPROTECTED":
                key = why + "  " + w
                protected_kinds[key] = protected_kinds.get(key, 0) + 1
        total_unprotected += len(unprotected)
        if unprotected:
            print("  %-24s STILL CARRIES THE MARK in %d place(s): %s"
                  % (name, len(unprotected), ", ".join(sorted(set(unprotected))[:6])))
        # The fork's own strings, if an overlay exists for this locale. Structure is asserted
        # BEFORE the merge, above, so a broken derivation is never masked by a successful append.
        added = 0
        if overlaydir:
            ov = os.path.join(overlaydir, suffix)
            if os.path.isfile(ov):
                after, added = merge_overlay(after, open(ov, encoding="utf-8").read(), name)
                total_added += added

        dest = os.path.join(outdir, prefix + "_" + suffix)
        with open(dest, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(after)
        print("  %-24s -> %-28s %4d translated string(s) rewritten%s"
              % (name, os.path.basename(dest), stats["changed"],
                 (", %d of ours added" % added) if added else ""))
        total_changed += stats["changed"]
        files += 1

    if files == 0:
        raise SystemExit("FATAL: no .ts file in %s - nothing was derived and that is not a pass" % indir)

    print("%d file(s), %d translated string(s) rewritten, %d of the fork's own strings added, "
          "%d unprotected brand occurrence(s) left"
          % (files, total_changed, total_added, total_unprotected))
    if protected_kinds:
        print("LEFT ALONE, each for a stated reason:")
        print("  url-or-email          an #L007 input - the operator's real domain settles it")
        print("  same-as-source-slug   a path or identifier, not a name")
        print("  SOURCE-STRING-...     the ENGLISH string still carries the mark. That is a")
        print("                        qsTr() in the code, and rewriting the translation here")
        print("                        would HIDE it. Fix the source; this tool will not.")
        for key in sorted(protected_kinds):
            print("   %4d  %s" % (protected_kinds[key], key))
    # A floor: if nothing was rewritten the map has gone blind, and a silent 0 here would ship
    # Amnezia's marks in ten languages (#L041).
    if total_changed == 0:
        raise SystemExit("FATAL: 0 strings rewritten across %d files - the brand map matched "
                         "nothing, which is blindness rather than a clean tree" % files)
    return 1 if total_unprotected else 0


SELFTEST_TS = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ko_KR">
<context>
    <name>PageSettingsAbout</name>
    <message>
        <source>Amnezia is a free and open-source VPN</source>
        <translation>Amnezia는 무료 오픈소스 VPN입니다</translation>
    </message>
    <message>
        <source>Use AmneziaDNS</source>
        <translation>AmneziaDNS를 사용합니다. AmneziaWG는 빠릅니다.</translation>
    </message>
    <message>
        <source>Contact us</source>
        <translation>support@amnezia.org 로 연락하세요</translation>
    </message>
    <message numerus="yes">
        <source>%n day(s)</source>
        <translation>
            <numerusform>AmneziaVPN %n일</numerusform>
        </translation>
    </message>
    <message>
        <source>documentation/instructions/connect-amnezia-premium#windows</source>
        <translation>documentation/instructions/connect-amnezia-premium#windows</translation>
    </message>
</context>
</TS>
"""


def run_selftest():
    ok = True

    after, stats = derive(SELFTEST_TS)

    def check(label, cond):
        nonlocal ok
        print("  %-58s %s" % (label, "ok" if cond else "FAIL"))
        if not cond:
            ok = False

    check("the bare mark is rewritten in translated text",
          "AresVPN는 무료" in after)
    # NOTE the shape of this one. `"AmneziaDNS" not in after` is WRONG and failed on the first
    # run: <source>Use AmneziaDNS</source> is meant to survive untouched, so the whole document
    # still contains the word. The assertion has to look at translated text only - which is the
    # thing this tool is about, and an assertion that does not know its own subject is #L025.
    translated_only = "".join(m.group(3) for m in TRANSLATED.finditer(after))
    check("AmneziaDNS -> AresDNS in translated text, and only there",
          "AresDNS" in translated_only and "AmneziaDNS" not in translated_only
          and "<source>Use AmneziaDNS</source>" in after)
    check("AmneziaWG -> AresWG with the Korean suffix kept",
          "AresWG는" in after)
    check("a numerusform is rewritten too", "AresVPN %n" in after)
    check("the <source> lines are untouched",
          "<source>Amnezia is a free and open-source VPN</source>" in after
          and "<source>Use AmneziaDNS</source>" in after)
    check("an email address is NOT rewritten", "support@amnezia.org" in after)
    check("the context <name> is untouched", "<name>PageSettingsAbout</name>" in after)
    check("four translated strings were counted as changed", stats["changed"] == 3)

    # the structural guard must REFUSE, not warn
    broken = after.replace("<source>Contact us</source>", "<source>Contact them</source>")
    try:
        assert_structure(SELFTEST_TS, broken, "selftest")
        check("a changed <source> is refused", False)
    except SystemExit as exc:
        check("a changed <source> is refused", "source" in str(exc))

    try:
        assert_structure(SELFTEST_TS, "<TS></TS>", "selftest")
        check("a collapsed file is refused", False)
    except SystemExit as exc:
        check("a collapsed file is refused", "message" in str(exc) or "shrank" in str(exc))

    check("a slug whose translation equals its source is left alone",
          "connect-amnezia-premium#windows" in after and stats["same_as_source"] == 1)

    left = residue(after)
    check("nothing is left UNPROTECTED, and both other reasons are named",
          [w for w, why in left if why == "UNPROTECTED"] == []
          and any(why == "url-or-email" for _w, why in left)
          and any(why == "same-as-source-slug" for _w, why in left))

    print("selftest: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="indir")
    ap.add_argument("--out", dest="outdir")
    ap.add_argument("--prefix", default="aresvpnclient")
    ap.add_argument("--overlay", dest="overlaydir",
                    help="directory of <locale>.ts files carrying the strings THIS FORK added; "
                         "each is appended to the derived file for that locale")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return run_selftest()
    if args.report:
        if not args.indir:
            raise SystemExit("--report needs --in")
        return run_report(args.indir)
    if not args.indir or not args.outdir:
        raise SystemExit("need --in and --out (or --report / --selftest)")
    return run_derive(args.indir, args.outdir, args.prefix, args.overlaydir)


if __name__ == "__main__":
    sys.exit(main())
