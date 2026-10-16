import unittest
from unittest.mock import patch
import struct
from sector.research.probe_contacts_readonly import (
    context,
    describe_node,
    pool_index,
    vector_count,
    surface_link,
)


class ContactProbeTests(unittest.TestCase):
    def test_surface_index_and_membership(self):
        self.assertTrue(surface_link(1, 2, 123, 123, 1)["surface_cache_matches"])
        self.assertFalse(surface_link(1, 2, 123, 456, 1)["surface_cache_matches"])
        self.assertFalse(surface_link(0, 2, 0, 0, 1)["surface_cache_matches"])
        with self.assertRaises(ValueError):
            surface_link(2, 2, 123, 123, 1)

    def test_surface_vector_stride_and_null_empty(self):
        self.assertEqual(vector_count(struct.pack("<3Q", 0, 0, 0), 88, 1024), 0)
        self.assertEqual(vector_count(struct.pack("<3Q", 65536, 65712, 65712), 88, 1024), 2)
        with self.assertRaises(ValueError):
            vector_count(struct.pack("<3Q", 65536, 65544, 65544), 88, 1024)

    def test_main_menu_with_stale_practice_label_refused(self):
        navigation = {
            "state": dict(
                appBuild=14150,
                settingMode="SETTING_GRANDPRIX",
                gameSession="PRACTICE1",
                gameState="GSTATE_DYN",
                navigationState="NAV_MAIN_MENU",
            )
        }
        game = dict(playerVehicleLoaded=True, isReplayActive=False)
        with patch(
            "sector.research.probe_contacts_readonly.get_json", side_effect=[navigation, game]
        ):
            with self.assertRaises(ValueError):
                context()

    def test_no_pool_pointer_or_surface_pointer_in_payload(self):
        row = describe_node(5, (1, 0), (0.002, 0, 1, 0, 5.4), True)
        self.assertEqual(row["normal_length"], 1)
        self.assertEqual(row["pool_index"], 5)
        self.assertTrue(row["surface_reference_present"])
        self.assertNotIn("address", row)

    def test_expired_depth_is_not_mistaken_for_corrupt_data(self):
        # Native refresh writes a large negative depth sentinel before rebuilding.
        row = describe_node(0, (1, 0), (-3.402823466e38, 0, 1, 0, 0), False)
        self.assertLess(row["candidate_signed_depth_d0"], 0)

    def test_misaligned_before_pool_and_large_indices_refused(self):
        pool = 0x100000
        self.assertEqual(pool_index(pool + 0x110 * 503, pool), 503)
        for address in (pool - 0x110, pool + 1, pool + 32768 * 0x110):
            with self.assertRaises(ValueError):
                pool_index(address, pool)

    def test_nonfinite_geometry_refused(self):
        for value in (float("nan"), float("inf"), -float("inf")):
            with self.assertRaises(ValueError):
                describe_node(0, (1, 0), (0, 0, value, 0, 0), True)


if __name__ == "__main__":
    unittest.main()
