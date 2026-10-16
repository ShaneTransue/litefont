"""Write UTF-8 decoding cases for the Utf8 tests, decoded by CPython.

CPython replaces each maximal ill-formed subpart with one U+FFFD, the
practice the Unicode standard recommends, so its output is the expected
result for px::Utf8. The cases are random but seeded, mixing ASCII, valid
sequences and the bytes that make sequences ill-formed.

    python tools/oracle/utf8_cases.py tests/text/utf8-cases.txt

Each line is "<bytes in hex> : <code points in hex>".
"""
import random
import sys

INTERESTING = [0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1, 0xC2, 0xDF, 0xE0, 0xE1, 0xEC, 0xED,
               0xEE, 0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xF8, 0xFC, 0xFE, 0xFF]


def random_piece(rng):
    kind = rng.random()
    if kind < 0.25:
        return bytes([rng.randrange(0x20, 0x7F)])
    if kind < 0.6:
        while True:
            value = rng.choice([rng.randrange(0x80, 0x800), rng.randrange(0x800, 0x10000),
                                rng.randrange(0x10000, 0x110000)])
            if not 0xD800 <= value <= 0xDFFF:
                return chr(value).encode("utf-8")
    if kind < 0.8:
        return bytes([rng.choice(INTERESTING)])
    # A valid sequence cut short or with a damaged continuation byte.
    encoded = bytearray(chr(rng.randrange(0x800, 0x110000) if rng.random() < 0.5 else rng.randrange(0x80, 0x800))
                        .encode("utf-8", "surrogatepass"))
    if rng.random() < 0.5:
        return bytes(encoded[:rng.randrange(1, len(encoded))]) if len(encoded) > 1 else bytes(encoded)
    encoded[rng.randrange(len(encoded))] = rng.choice(INTERESTING + [0x41])
    return bytes(encoded)


def main():
    rng = random.Random(20261015)
    lines = []
    for _ in range(3000):
        data = b"".join(random_piece(rng) for _ in range(rng.randrange(1, 8)))
        decoded = data.decode("utf-8", errors="replace")
        lines.append(data.hex(" ") + " : " + " ".join(f"{ord(c):x}" for c in decoded))
    with open(sys.argv[1], "w", encoding="ascii", newline="\n") as stream:
        stream.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
