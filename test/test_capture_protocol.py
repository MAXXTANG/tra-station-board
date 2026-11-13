import sys
from pathlib import Path
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from capture_cyd_gallery import Board


class Wire:
    def __init__(self):
        self.sent = []

    def write(self, data):
        self.sent.append(data)


class CaptureProtocolTests(unittest.TestCase):
    def board(self, lines):
        board = Board.__new__(Board)
        board.serial = Wire()
        stream = iter(lines)
        board.line = lambda: next(stream)
        return board

    def row(self, packet, checksum=None):
        return f'ROW 0 {packet.hex()} {zlib.crc32(packet) if checksum is None else checksum:08x}'

    def test_full_width_with_split_run(self):
        packet = bytes.fromhex('fff7be41f7be')
        board = self.board([self.row(packet), 'CAPTURE_END'])
        image, retries = board.rows(320, 1)
        self.assertEqual(image.size, (320, 1))
        self.assertEqual(image.getpixel((319, 0)), (246, 246, 246))
        self.assertEqual(board.serial.sent, [b'A0\n'])
        self.assertEqual(retries, 0)

    def test_corrupt_row_is_retried(self):
        packet = bytes.fromhex('020000')
        board = self.board([self.row(packet, 0), self.row(packet), 'CAPTURE_END'])
        image, retries = board.rows(2, 1)
        self.assertEqual(image.tobytes(), bytes(6))
        self.assertEqual(board.serial.sent, [b'R0\n', b'A0\n'])
        self.assertEqual(retries, 1)

    def test_incomplete_frame_never_returns_image(self):
        board = self.board(['CAPTURE_END'])
        with self.assertRaises(AssertionError):
            board.rows(320, 240)

    def test_bad_width_is_rejected(self):
        packet = bytes.fromhex('010000')
        board = self.board([self.row(packet), 'CAPTURE_ABORT'])
        with self.assertRaises(RuntimeError):
            board.rows(320, 1)
        self.assertEqual(board.serial.sent, [b'R0\n'])


if __name__ == '__main__':
    unittest.main()
