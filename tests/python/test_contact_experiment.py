import unittest
from sector.observers.run_contact_experiment import Result, ready, stationary


class ContactExperimentTests(unittest.TestCase):
    def test_stationary_neutral_pits_required(self):
        sample = dict(gear=0, in_pits=True, brake=0, throttle=0, velocity_local_mps=[0, 0, 0])
        self.assertTrue(stationary(sample))
        for key, value in [
            ("gear", 1),
            ("in_pits", False),
            ("throttle", 0.5),
            ("velocity_local_mps", [0.011, 0, 0]),
            ("velocity_local_mps", [float("nan"), 0, 0]),
        ]:
            self.assertFalse(stationary({**sample, key: value}))

    def test_every_wheel_must_have_recent_good_observations(self):
        samples = (Result * 4)()
        for s in samples:
            s.samples = 64
            s.tick_ms = 1000
            s.post_count = 50
            s.post_positive = 50
        self.assertTrue(ready(samples, 1100))
        samples[3].post_bad = 1
        self.assertFalse(ready(samples, 1100))
        samples[3].post_bad = 0
        self.assertFalse(ready(samples, 1600))
        samples[3].position_delta = 0.1
        self.assertFalse(ready(samples, 1100))


if __name__ == "__main__":
    unittest.main()
