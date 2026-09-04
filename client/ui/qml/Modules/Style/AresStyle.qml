pragma Singleton

import QtQuick

// AresVPN Client - the design tokens for the rebuilt UI (AresProject ROADMAP 18-3h).
//
// Upstream's AmneziaStyle.qml is left exactly as it is so its merges stay clean; every screen
// we rebuild draws from HERE instead. The mark the operator supplied is achromatic - a white
// shield on near-black - so the neutrals below are biased a few degrees cold to read as chosen
// rather than as default grey, and the accent is a DECISION rather than something the brand
// already made.
//
// THE ACCENT IS ONE LINE. The three directions put to the operator (#D071's method):
//   A "Instrument"  '#F2F4F7'  - achromatic, state reads from fill and weight alone
//   B "Signal"      '#7FC6E8'  - one cold accent, spent only on the live tunnel
//   C "Ember"       '#D98A4A'  - a warm signal; Ares rather than network
QtObject {
    id: root

    // ---- the one decision --------------------------------------------------------------
    readonly property color accent: '#7FC6E8'
    // NOT `onAccent`. QML reads any `on<Capital>` identifier as a SIGNAL HANDLER, so
    // `readonly property color onAccent: ...` is `Cannot assign a value to a signal` -
    // AresStyle then fails to load, and with it every type that imports it, up to
    // `main2.qml: Type PageStart unavailable`. The whole UI was blank and nothing said so:
    // qmllint passed, the name cross-check passed, the build was RC=0. It took loading the
    // QML in a real Qt engine to see it (AresProject 18-3h).
    readonly property color accentForeground: '#08090B'

    property QtObject color: QtObject {
        readonly property color transparent: 'transparent'

        // grounds, darkest to lightest
        readonly property color bg: '#0B0C0E'
        readonly property color surface: '#131519'
        readonly property color surfaceHi: '#1A1D22'
        readonly property color surfacePressed: '#252931'
        readonly property color line: '#20242B'
        readonly property color lineStrong: '#2F343D'

        // text
        readonly property color text: '#F2F4F7'
        readonly property color textDim: '#98A0AD'
        readonly property color textMute: '#666E7B'

        readonly property color accent: root.accent
        readonly property color accentForeground: root.accentForeground
        readonly property color accentSoft: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.14)

        // semantic - separate from the accent on purpose, so "connected" and "expiring"
        // never have to compete for the same hue
        readonly property color ok: '#5FBF8A'
        readonly property color warn: '#D9A54A'
        readonly property color bad: '#D9635F'

        readonly property color scrim: Qt.rgba(0, 0, 0, 0.55)
        readonly property color hover: Qt.rgba(1, 1, 1, 0.05)
    }

    // PT Root UI stays, under its OFL duties (#D180). Data - addresses, ports, idx, counters -
    // is set in a mono face, because this product is addresses and expiry dates.
    property QtObject font: QtObject {
        // PT Root UI carries NO HANGUL, and this product's first market is Korea. The fallback is
        // registered once in C++ (AmneziaApplication::loadFonts, QFont::insertSubstitutions)
        // rather than here, because `font.families` is not assignable on Text in this Qt -
        // measured: "Cannot assign to non-existent property families", and it took the whole UI
        // down for one build until --qml-smoke said so.
        //
        // WHY THE HARNESS CANNOT CHECK IT, measured rather than inferred (--font-report):
        // the offscreen process sees exactly ONE font family, the bundled PT Root UI VF. Malgun
        // Gothic, Segoe UI, Noto Sans CJK KR, Gulim, Batang and Dotum are all ABSENT, and every
        // family name asked for resolves to that one font, which has no Hangul glyph. So every
        // screenshot renders Korean as tofu whatever the code does, and the render harness can say
        // nothing about it either way. Only a desktop run settles it.
        //
        // AND PT MONO IS NOT BUNDLED EITHER - the same report shows `PT Mono` resolving to
        // PT Root UI VF. Only pt-root-ui_vf.ttf is added as an application font. On a real desktop
        // the substitution list picks up Consolas; the design's mono face is a REQUEST, not a
        // guarantee, and saying so here is cheaper than someone measuring it again.
        readonly property string family: 'PT Root UI'
        readonly property string mono: 'PT Mono'
    }

    property QtObject size: QtObject {
        readonly property int display: 32
        readonly property int title: 22
        readonly property int heading: 17
        readonly property int body: 15
        readonly property int small: 13
        readonly property int label: 11
    }

    property QtObject space: QtObject {
        readonly property int xs: 4
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int lg: 16
        readonly property int xl: 24
        readonly property int xxl: 32
    }

    property QtObject radius: QtObject {
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int pill: 999
    }
}
