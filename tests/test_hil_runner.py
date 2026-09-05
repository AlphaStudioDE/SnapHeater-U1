#!/usr/bin/env python3
import importlib.util
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("snapheater_hil", ROOT / "tools" / "hil.py")
hil = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(hil)


class HilRunnerTest(unittest.TestCase):
    def test_dotted_expectations_and_operators(self):
        hil.check_expectations(
            {"http_status": 200, "state": {"owner": "rest", "count": 3}},
            {"http_status": 200, "state.owner": "rest", "state.count": {"gte": 2}},
        )

    def test_variable_substitution(self):
        value = hil.substitute({"json": {"lease_id": "$lease"}}, {"lease": "abc"})
        self.assertEqual(value["json"]["lease_id"], "abc")

    def test_heat_detection(self):
        self.assertTrue(hil.requests_heat({"method": "POST", "json": {"work_on": True}}))
        self.assertFalse(hil.requests_heat({"method": "POST", "json": {"work_on": False}}))

    def test_scenarios_parse_and_start_safe(self):
        for path in sorted(hil.SCENARIO_DIR.glob("*.json")):
            scenario = hil.load_scenario(path)
            self.assertTrue(scenario["steps"], path)
            first = scenario["steps"][0]["request"]
            self.assertEqual(first["path"], "/api/settings", path)
            self.assertTrue(first["json"].get("safe_stop"), path)

    def test_panda_suite_has_no_ota_write(self):
        for path in sorted(hil.SCENARIO_DIR.glob("*.json")):
            raw = json.loads(path.read_text(encoding="utf-8"))
            if "panda" in raw.get("targets", []):
                self.assertFalse(raw.get("requires_ota_write", False), path)


if __name__ == "__main__":
    unittest.main()
