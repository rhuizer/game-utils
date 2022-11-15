#!/usr/bin/env python3
#
# Very simple viewer for the Aperture Image Format used in the Portal games.
# Currently does not support nor use Aperture Menu Format files.
#
import argparse
from PIL import Image

class ApertureFile(object):
    MAGIC = b"APERTURE IMAGE FORMAT (c) 1985"

    def __init__(self, filename: str, width: int, height: int,
            strict: bool=False) -> None:
        self.width = width
        self.height = height
        self.max_y = height
        self.strict = strict

        self._f = open(filename, "rb")
        magic_read = self._f.read(len(ApertureFile.MAGIC))

        if magic_read != ApertureFile.MAGIC:
            raise RuntimeError("Invalid aperture file")
        self.read_eol()
        self.line_skip = int(self.read_line().strip())

    def close(self) -> None:
        if self._f is not None:
            self._f.close()

    def read_line(self) -> bytes:
        s = b""

        while (b := self._f.read(1)) != b"":
            s += b
            if self.is_eol(s):
                return s

        raise RuntimeError("Invalid aperture file")

    def is_eol(self, b: bytes) -> bool:
        if b.endswith(b"\n\n\r"):
            return True

        if self.strict:
            return False

        if b.endswith(b"\n\r") or b.endswith(b"\r\n"):
            return True

        return False

    def read_eol(self) -> bytes:
        # Strict mode.  We read 3 bytes at once.
        if self.strict:
            eol = self._f.read(3)
            if not self.is_eol(eol):
                raise RuntimeError("Invalid aperture file")
            return eol

        # Non-strict handling.  We read two bytes and check those first.
        eol = self._f.read(2)

        # In non-strict mode "\n\r" or "\r\n" are acceptable.
        if self.is_eol(eol):
            return eol

        # Get another byte to see if we may have a three byte eol.
        eol += self._f.read(1)
        if not self.is_eol(eol):
            raise RuntimeError("Invalid aperture file")

        return eol

    def read_one(self) -> int | None:
        b = self._f.read(1)
        if b == b"" or b is None: return None
        return b[0] - 32

    def next_pos(self, x: int, y: int) -> tuple[int, int]:
        # If the next pixel is ok, then return that.
        x = x + 1
        if x < self.width:
            return x, y

        # x back to 0, y adjusted by line_skip.
        x = 0
        y = y - self.line_skip

        # No wrap, we're good.
        if y >= 0:
            return x, y

        # y becomes the largest untouched line.
        self.max_y = self.max_y - 1
        return x, self.max_y

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog = 'apfview',
        description = 'A viewer for the Portal Aperture Image Format'
    )

    parser.add_argument('filename')
    parser.add_argument('--height', type=int, default=200)
    parser.add_argument('--width', type=int, default=320)
    args = parser.parse_args()

    ap = ApertureFile(args.filename, args.width, args.height)
    im = Image.new("RGB", (args.width, args.height), (255, 255, 255))

    x = y = 0
    while True:
        i = ap.read_one()
        if i is None: break

        # Draw count.
        for _ in range(i):
            im.putpixel((x, y), (0, 0, 0))
            x, y = ap.next_pos(x, y)

        i = ap.read_one()
        if i is None: break

        # Skip count.
        for _ in range(i):
            x, y = ap.next_pos(x, y)

    im.show()
