#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run SnapHeater REST HIL scenarios and write a JSON report/transcript.

Scenario mechanics are adapted from plastikman/DragonBreath tools/hil.py.
The transport and commands are SnapHeater-specific.
"""

from __future__ import annotations

import argparse
import copy
import datetime as dt
import json
import pathlib
import time
import urllib.error
import urllib.request
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCENARIO_DIR = ROOT / "tests" / "hil" / "scenarios"


class HilError(RuntimeError):
    pass


def dotted_get(value: Any, path: str) -> Any:
    current = value
    for part in path.split("."):
        if not isinstance(current, dict) or part not in current:
            raise HilError(f"response has no field {path!r}")
        current = current[part]
    return current


def check_expectations(response: dict[str, Any], expected: dict[str, Any]) -> None:
    for path, wanted in expected.items():
        actual = dotted_get(response, path)
        if isinstance(wanted, dict):
            if len(wanted) != 1:
                raise HilError(f"{path}: assertion requires exactly one operator")
            operator, operand = next(iter(wanted.items()))
            operations = {
                "eq": lambda: actual == operand,
                "ne": lambda: actual != operand,
                "gt": lambda: actual > operand,
                "gte": lambda: actual >= operand,
                "lt": lambda: actual < operand,
                "lte": lambda: actual <= operand,
                "contains": lambda: operand in actual,
            }
            if operator == "between":
                passed = isinstance(operand, list) and len(operand) == 2 and operand[0] <= actual <= operand[1]
            elif operator in operations:
                passed = operations[operator]()
            else:
                raise HilError(f"{path}: unknown assertion operator {operator!r}")
        else:
            passed = actual == wanted
        if not passed:
            raise HilError(f"{path}: expected {wanted!r}, got {actual!r}")


def substitute(value: Any, variables: dict[str, Any]) -> Any:
    if isinstance(value, str) and value.startswith("$"):
        name = value[1:]
        if name not in variables:
            raise HilError(f"unknown scenario variable {value}")
        return variables[name]
    if isinstance(value, dict):
        return {key: substitute(item, variables) for key, item in value.items()}
    if isinstance(value, list):
        return [substitute(item, variables) for item in value]
    return value


def load_scenario(path: pathlib.Path, target: str | None = None) -> dict[str, Any]:
    try:
        scenario = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise HilError(f"cannot load scenario {path}: {exc}") from exc
    if not isinstance(scenario, dict) or not isinstance(scenario.get("steps"), list):
        raise HilError(f"{path}: scenario must contain a steps array")
    targets = scenario.get("targets", ["devboard", "panda"])
    if target and target not in targets:
        raise HilError(f"{path}: scenario does not support target {target!r}")
    return scenario


def requests_heat(request: dict[str, Any]) -> bool:
    body = request.get("json", {})
    return request.get("method", "GET") == "POST" and body.get("work_on") is True


class HttpTransport:
    def __init__(self, base_url: str, token: str, transcript) -> None:
        self.base_url = base_url.rstrip("/")
        self.token = token
        self.transcript = transcript

    def request(self, spec: dict[str, Any], timeout_s: float = 8.0) -> dict[str, Any]:
        method = spec.get("method", "GET").upper()
        path = spec.get("path")
        if not isinstance(path, str) or not path.startswith("/"):
            raise HilError("request path must start with /")
        headers = {"Accept": "application/json"}
        data = None
        if "json" in spec:
            data = json.dumps(spec["json"], separators=(",", ":")).encode()
            headers["Content-Type"] = "application/json"
        elif "body_file" in spec:
            data = pathlib.Path(spec["body_file"]).read_bytes()
            headers["Content-Type"] = "application/octet-stream"
        elif "raw_hex" in spec:
            data = bytes.fromhex(spec["raw_hex"])
            headers["Content-Type"] = "application/octet-stream"
        if method != "GET":
            headers["X-DragonBreath-Auth"] = self.token
        if method == "POST" and path in ("/update", "/api/v2/update") and data is not None:
            import hashlib
            headers["X-SnapHeater-SHA256"] = hashlib.sha256(data).hexdigest()
        record = {"at": dt.datetime.now(dt.timezone.utc).isoformat(), "tx": {"method": method, "path": path, "bytes": len(data or b"")}}
        self.transcript.write(json.dumps(record) + "\n")
        self.transcript.flush()
        req = urllib.request.Request(self.base_url + path, data=data, headers=headers, method=method)
        try:
            response = urllib.request.urlopen(req, timeout=timeout_s)
            status, raw = response.status, response.read()
        except urllib.error.HTTPError as exc:
            status, raw = exc.code, exc.read()
        try:
            body = json.loads(raw.decode("utf-8")) if raw else {}
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise HilError(f"HTTP {status} returned non-JSON data") from exc
        if not isinstance(body, dict):
            raise HilError(f"HTTP {status} response is not a JSON object")
        body["http_status"] = status
        self.transcript.write(json.dumps({"at": dt.datetime.now(dt.timezone.utc).isoformat(), "rx": body}) + "\n")
        self.transcript.flush()
        return body


def run_scenario(transport: HttpTransport, scenario: dict[str, Any], target: str,
                 allow_heater: bool, variables: dict[str, Any]) -> dict[str, Any]:
    reports = []
    for index, step in enumerate(scenario["steps"], 1):
        name = step.get("name", f"step {index}")
        report = {"name": name, "passed": False}
        started = time.monotonic()
        try:
            if "wait_s" in step:
                time.sleep(float(step["wait_s"]))
            else:
                request = substitute(copy.deepcopy(step.get("request")), variables)
                if not isinstance(request, dict):
                    raise HilError("step requires request or wait_s")
                if target == "panda" and requests_heat(request) and not allow_heater:
                    raise HilError("positive heat request refused on Panda; pass --allow-heater")
                attempts = max(1, int(step.get("attempts", 1)))
                last_error = None
                for attempt in range(attempts):
                    try:
                        response = transport.request(request, float(step.get("timeout_s", 8)))
                        check_expectations(response, substitute(step.get("expect", {"http_status": 200}), variables))
                        break
                    except (OSError, HilError) as exc:
                        last_error = exc
                        if attempt + 1 == attempts:
                            raise
                        time.sleep(float(step.get("retry_delay_s", 1)))
                else:
                    raise last_error or HilError("request failed")
                for variable, path in step.get("save", {}).items():
                    variables[variable] = dotted_get(response, path)
                report["response"] = response
            report["passed"] = True
            print(f"  PASS  {name}")
        except Exception as exc:
            report["error"] = str(exc)
            reports.append(report)
            print(f"  FAIL  {name}: {exc}")
            return {"name": scenario.get("name", "unnamed"), "passed": False, "steps": reports}
        finally:
            report["duration_s"] = round(time.monotonic() - started, 3)
        reports.append(report)
    return {"name": scenario.get("name", "unnamed"), "passed": True, "steps": reports}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True, help="device base URL, e.g. http://192.168.1.50")
    parser.add_argument("--token", default="web", help="X-DragonBreath-Auth token")
    parser.add_argument("--target", choices=("devboard", "panda"), default="devboard")
    parser.add_argument("--scenario", type=pathlib.Path)
    parser.add_argument("--suite", action="store_true")
    parser.add_argument("--allow-heater", action="store_true")
    parser.add_argument("--allow-ota-write", action="store_true")
    parser.add_argument("--firmware", type=pathlib.Path)
    parser.add_argument("--output-dir", type=pathlib.Path)
    args = parser.parse_args()
    if bool(args.scenario) == bool(args.suite):
        parser.error("select exactly one of --scenario or --suite")
    if args.firmware and not args.allow_ota_write:
        parser.error("--firmware requires --allow-ota-write")
    paths = sorted(SCENARIO_DIR.glob("*.json")) if args.suite else [args.scenario]
    selected = [(path, load_scenario(path, args.target)) for path in paths if args.target in load_scenario(path).get("targets", ["devboard", "panda"])]
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    output = args.output_dir or ROOT / "hil-results" / stamp
    output.mkdir(parents=True, exist_ok=True)
    variables = {"firmware": str(args.firmware.resolve())} if args.firmware else {}
    with (output / "http.jsonl").open("w", encoding="utf-8") as transcript:
        transport = HttpTransport(args.url, args.token, transcript)
        results = []
        for path, scenario in selected:
            if scenario.get("requires_ota_write") and not args.allow_ota_write:
                print(f"  SKIP  {path.name}: requires --allow-ota-write")
                continue
            print(f"\n{scenario.get('name', path.name)}")
            results.append(run_scenario(transport, scenario, args.target, args.allow_heater, variables))
            if not results[-1]["passed"]:
                break
    report = {"target": args.target, "url": args.url, "passed": bool(results) and all(item["passed"] for item in results), "results": results}
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"results: {output}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
