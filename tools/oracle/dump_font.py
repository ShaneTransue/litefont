"""Write a reference dump of an OpenType font with fontTools, for FontOracleTests.

Usage:
    python dump_font.py fonts/segoeui.ttf out/reference/segoeui.txt
    python dump_font.py fonts/msyh.ttc out/reference/msyh-1.txt --face 1 --outline-stride 16
    python dump_font.py fonts/MyriadPro-Regular.otf out/reference/myriadpro.txt

The dump mirrors the rules the C++ Font follows, so the two can be compared
line by line:
    font <face> <path>
    names <family>\\t<subfamily>\\t<fullName>
        name IDs 1, 2 and 4: Windows English first, then any Windows Unicode
        name, then a Unicode platform name, then Macintosh Roman English
    metrics <unitsPerEm> <ascender> <descender> <lineGap> <xHeight> <capHeight>
    traits <weight> <bold 0|1> <italic 0|1> <italicAngle>
    decoration <subSize> <subOffset> <superSize> <superOffset> <strikeSize> <strikePosition> <underlinePosition> <underlineThickness>
        as stored in OS/2 and post, 0 when the table is missing
        line metrics from hhea, or OS/2 typo values when USE_TYPO_METRICS is set;
        heights are -1 when OS/2 is older than version 2 (the C++ side estimates them)
    glyphs <count>
    cmap <codePoint> <glyph>            the first of (3,10) (0,4) (3,1) (0,3) (0,2) (0,1) (0,0) in format 4 or 12
    hmtx <glyph> <advance> <leftSideBearing>
    kernsample <glyph> ...              GPOS fonts: every pair of these glyphs is checked,
                                        and pairs without a kern line must kern by 0
    kern <left> <right> <value>         GPOS fonts: the nonzero sampled pairs, from the
                                        Python reference below; otherwise every pair of the
                                        first horizontal format 0 subtable of a version 0 kern table
    outline <glyph> <tokens>            M x y | L x y | Q cx cy x y | C ... | Z, in font units;
                                        "unsupported" when a component uses point matching or a
                                        CFF glyph uses seac or an operator the C++ side rejects

The GPOS reference is an independent Python version of the C++ rules, over
fontTools' parsed tables: the 'kern' feature of the default script's default
language, else Latin's, else every kern feature; pair adjustment lookups
(also inside extensions); within a lookup the first subtable that covers the
pair applies (a pair list without the second glyph passes on, a class table
always applies); lookups add up; only the first glyph's x advance counts.

The oracle checks the library; it is never linked into it.
"""
import argparse
from pathlib import Path

from fontTools.misc.psCharStrings import T2OutlineExtractor
from fontTools.pens.basePen import BasePen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

KERN_SAMPLE = list(range(32, 127)) + list(range(0xA1, 0x100)) + list(range(0x100, 0x180))


class CanonicalPen(BasePen):
    """Records drawing calls as tokens; BasePen splits TrueType curves into single quads."""

    def __init__(self):
        super().__init__(glyphSet=None)
        self.tokens = []

    def _point(self, point):
        for value in point:
            self.tokens.append(repr(float(value)) if value != int(value) else str(int(value)))

    def _moveTo(self, point):
        self.tokens.append("M")
        self._point(point)

    def _lineTo(self, point):
        self.tokens.append("L")
        self._point(point)

    def _qCurveToOne(self, control, point):
        self.tokens.append("Q")
        self._point(control)
        self._point(point)

    def _curveToOne(self, control1, control2, point):
        self.tokens.append("C")
        self._point(control1)
        self._point(control2)
        self._point(point)

    def _closePath(self):
        self.tokens.append("Z")

    def _endPath(self):
        pass


class UnsupportedGlyph(Exception):
    pass


class StrictExtractor(T2OutlineExtractor):
    """fontTools' Type 2 interpreter, except that endchar with seac arguments fails as in C++."""

    def op_endchar(self, index):
        self.endPath()
        if self.popallWidth():
            raise UnsupportedGlyph("seac")


def draw_truetype(glyf, name, pen, depth=0):
    """Draws a glyph without the glyph set's left-side-bearing offset, decomposing components."""
    if depth > 8:
        raise UnsupportedGlyph(name)
    glyph = glyf[name]
    if glyph.isComposite():
        for component in glyph.components:
            if not hasattr(component, "x"):
                raise UnsupportedGlyph(name)      # placed by point matching
            base, transform = component.getComponentInfo()
            draw_truetype(glyf, base, TransformPen(pen, transform), depth + 1)
    else:
        glyph.draw(pen, glyf)


def draw_cff(char_strings, name, pen):
    char_string = char_strings[name]
    char_string.outlineExtractor = StrictExtractor
    try:
        char_string.draw(pen)
    except UnsupportedGlyph:
        raise
    except Exception as error:                   # arithmetic operators and malformed programs
        raise UnsupportedGlyph(f"{name}: {error}")


def pick_character_map(font):
    preference = [(3, 10), (0, 4), (3, 1), (0, 3), (0, 2), (0, 1), (0, 0)]
    for platform, encoding in preference:
        for table in font["cmap"].tables:
            if table.platformID == platform and table.platEncID == encoding and table.format in (4, 12):
                return table
    return None


def decode_name(data, utf16):
    if not utf16:
        return "".join(chr(byte) if byte < 0x80 else "\ufffd" for byte in data)
    units = [(data[i] << 8) | data[i + 1] for i in range(0, len(data) - 1, 2)]
    text = []
    i = 0
    while i < len(units):
        unit = units[i]
        if 0xD800 <= unit < 0xDC00 and i + 1 < len(units) and 0xDC00 <= units[i + 1] < 0xE000:
            text.append(chr(0x10000 + ((unit - 0xD800) << 10) + (units[i + 1] - 0xDC00)))
            i += 2
            continue
        text.append("\ufffd" if 0xD800 <= unit < 0xE000 else chr(unit))
        i += 1
    return "".join(text)


def best_name(font, name_id):
    if "name" not in font:
        return ""
    best, best_rank = None, -1
    for record in font["name"].names:
        if record.nameID != name_id:
            continue
        platform, encoding, language = record.platformID, record.platEncID, record.langID
        if platform == 3 and encoding in (1, 10):
            rank = 0 if language == 0x409 else 1
        elif platform == 0:
            rank = 2
        elif platform == 1 and encoding == 0 and language == 0:
            rank = 3
        else:
            continue
        if best_rank >= 0 and rank >= best_rank:
            continue
        best, best_rank = record, rank
    if best is None:
        return ""
    data = best.string if isinstance(best.string, bytes) else best.toBytes()
    return decode_name(data, best.platformID != 1)


def kern_subtables(font):
    """The pair adjustment subtables of the chosen kern lookups, grouped by lookup in lookup order."""
    if "GPOS" not in font:
        return []
    gpos = font["GPOS"].table
    if gpos.FeatureList is None or gpos.LookupList is None:
        return []
    features = gpos.FeatureList.FeatureRecord
    is_kern = [record.FeatureTag == "kern" for record in features]

    chosen = []
    scripts = gpos.ScriptList.ScriptRecord if gpos.ScriptList is not None else []
    for wanted in ("DFLT", "latn"):
        for record in scripts:
            if chosen:
                break
            if record.ScriptTag != wanted or record.Script.DefaultLangSys is None:
                continue
            language = record.Script.DefaultLangSys
            indices = ([language.ReqFeatureIndex] if language.ReqFeatureIndex != 0xFFFF else []) + list(language.FeatureIndex)
            chosen += [index for index in indices if index < len(features) and is_kern[index]]
        if chosen:
            break
    if not chosen:
        chosen = [index for index, kern in enumerate(is_kern) if kern]

    lookups = sorted({lookup for index in chosen for lookup in features[index].Feature.LookupListIndex})
    groups = []
    for index in lookups:
        if index >= len(gpos.LookupList.Lookup):
            continue
        lookup = gpos.LookupList.Lookup[index]
        subtables = []
        for subtable in lookup.SubTable:
            if lookup.LookupType == 9:
                if subtable.ExtensionLookupType != 2:
                    continue
                subtable = subtable.ExtSubTable
            elif lookup.LookupType != 2:
                continue
            if subtable.Format in (1, 2):
                subtables.append(prepare_pair_subtable(subtable))
        if subtables:
            groups.append(subtables)
    return groups


def prepare_pair_subtable(subtable):
    coverage = {}
    for index, name in enumerate(subtable.Coverage.glyphs):
        coverage.setdefault(name, index)
    prepared = {"format": subtable.Format, "coverage": coverage}
    if subtable.Format == 1:
        sets = []
        for pair_set in subtable.PairSet:
            pairs = {}
            for record in pair_set.PairValueRecord:
                pairs.setdefault(record.SecondGlyph, x_advance(record.Value1))
            sets.append(pairs)
        prepared["sets"] = sets
    else:
        prepared["class1"] = subtable.ClassDef1.classDefs if subtable.ClassDef1 is not None else {}
        prepared["class2"] = subtable.ClassDef2.classDefs if subtable.ClassDef2 is not None else {}
        prepared["counts"] = (subtable.Class1Count, subtable.Class2Count)
        prepared["values"] = [[x_advance(record.Value1) for record in row.Class2Record] for row in subtable.Class1Record]
    return prepared


def x_advance(value):
    if value is None:
        return 0
    return getattr(value, "XAdvance", 0) or 0


def apply_pair(subtable, left, right):
    """The x advance a subtable gives the pair, or None when it passes the pair on."""
    index = subtable["coverage"].get(left)
    if index is None:
        return None
    if subtable["format"] == 1:
        if index >= len(subtable["sets"]):
            return None
        return subtable["sets"][index].get(right)
    class1 = subtable["class1"].get(left, 0)
    class2 = subtable["class2"].get(right, 0)
    count1, count2 = subtable["counts"]
    if class1 >= count1 or class2 >= count2:
        return None
    return subtable["values"][class1][class2]


def gpos_kerning(groups, left, right):
    total = 0
    for subtables in groups:
        for subtable in subtables:
            value = apply_pair(subtable, left, right)
            if value is not None:
                total += value
                break
    return max(-32768, min(32767, total))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("font_path")
    parser.add_argument("output_path")
    parser.add_argument("--face", type=int, default=0)
    parser.add_argument("--outline-stride", type=int, default=1)
    arguments = parser.parse_args()

    font = TTFont(arguments.font_path, fontNumber=arguments.face, lazy=False)
    glyph_order = font.getGlyphOrder()
    glyph_ids = {name: index for index, name in enumerate(glyph_order)}
    glyph_count = font["maxp"].numGlyphs
    lines = [f"font {arguments.face} {Path(arguments.font_path).as_posix()}"]
    lines.append("names " + "\t".join(best_name(font, name_id) for name_id in (1, 2, 4)))

    head, hhea = font["head"], font["hhea"]
    ascender, descender, line_gap = hhea.ascent, hhea.descent, hhea.lineGap
    x_height, cap_height = -1, -1
    if "OS/2" in font:
        os2 = font["OS/2"]
        if os2.fsSelection & 0x80:
            ascender, descender, line_gap = os2.sTypoAscender, os2.sTypoDescender, os2.sTypoLineGap
        if os2.version >= 2:
            x_height, cap_height = os2.sxHeight, os2.sCapHeight
    lines.append(f"metrics {head.unitsPerEm} {ascender} {descender} {line_gap} {x_height} {cap_height}")
    bold, italic = bool(head.macStyle & 1), bool(head.macStyle & 2)
    weight = 700 if bold else 400
    if "OS/2" in font:
        os2 = font["OS/2"]
        bold, italic = bool(os2.fsSelection & 0x20), bool(os2.fsSelection & 0x01)
        weight = os2.usWeightClass if 1 <= os2.usWeightClass <= 1000 else weight
    angle = font["post"].italicAngle if "post" in font else 0.0
    lines.append(f"traits {weight} {int(bold)} {int(italic)} {angle:g}")
    decoration = [0] * 8
    if "OS/2" in font:
        os2 = font["OS/2"]
        decoration[0:6] = [os2.ySubscriptYSize, os2.ySubscriptYOffset, os2.ySuperscriptYSize, os2.ySuperscriptYOffset,
                           os2.yStrikeoutSize, os2.yStrikeoutPosition]
    if "post" in font:
        decoration[6:8] = [font["post"].underlinePosition, font["post"].underlineThickness]
    lines.append("decoration " + " ".join(str(value) for value in decoration))
    lines.append(f"glyphs {glyph_count}")

    table = pick_character_map(font)
    character_map = {}
    if table is not None:
        for code_point, name in sorted(table.cmap.items()):
            glyph = glyph_ids.get(name, 0)
            character_map[code_point] = glyph if glyph < glyph_count else 0
            lines.append(f"cmap {code_point} {character_map[code_point]}")

    hmtx = font["hmtx"]
    for index, name in enumerate(glyph_order):
        advance, bearing = hmtx[name]
        lines.append(f"hmtx {index} {advance} {bearing}")

    groups = kern_subtables(font)
    if groups:
        sample = sorted({character_map[code_point] for code_point in KERN_SAMPLE if character_map.get(code_point, 0) != 0})
        lines.append("kernsample " + " ".join(str(glyph) for glyph in sample))
        for left in sample:
            for right in sample:
                value = gpos_kerning(groups, glyph_order[left], glyph_order[right])
                if value != 0:
                    lines.append(f"kern {left} {right} {value}")
    elif "kern" in font and getattr(font["kern"], "version", None) == 0:
        for subtable in font["kern"].kernTables:
            coverage = getattr(subtable, "coverage", 0)
            if getattr(subtable, "format", None) == 0 and coverage & 1 and not coverage & 6:
                pairs = sorted((glyph_ids[left], glyph_ids[right], value)
                               for (left, right), value in subtable.kernTable.items())
                lines.extend(f"kern {left} {right} {value}" for left, right, value in pairs)
                break

    trueType = "glyf" in font and "loca" in font
    char_strings = None if trueType else font["CFF "].cff.topDictIndex[0].CharStrings
    for index, name in enumerate(glyph_order):
        if index % arguments.outline_stride != 0:
            continue
        pen = CanonicalPen()
        try:
            if trueType:
                draw_truetype(font["glyf"], name, pen)
            else:
                draw_cff(char_strings, name, pen)
            lines.append(" ".join(["outline", str(index)] + pen.tokens))
        except UnsupportedGlyph:
            lines.append(f"outline {index} unsupported")

    output = Path(arguments.output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {output} ({len(glyph_order)} glyphs, {'gpos' if groups else 'kern table'} kerning)")


if __name__ == "__main__":
    main()
