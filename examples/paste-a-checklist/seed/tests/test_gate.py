import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gate import same_reservation


class SameReservation(unittest.TestCase):
    def test_retry_at_10_hours_is_same(self):
        self.assertTrue(same_reservation(True, 10))

    def test_retry_at_25_hours_is_new(self):
        self.assertFalse(same_reservation(True, 25))

    def test_not_a_retry_is_new(self):
        self.assertFalse(same_reservation(False, 10))


if __name__ == "__main__":
    unittest.main()
