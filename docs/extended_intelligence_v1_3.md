# SnapHeater U1 v1.3 Extended Intelligence Pack

This version adds the second feature pack requested for the SnapHeater U1 firmware. The goal is to turn the heater firmware into a chamber climate assistant for Snapmaker U1, not just an ON/OFF heater.

## Added modules

- Chamber Warm-Up Prediction: estimates time to target using observed chamber temperature rise.
- Heat Soak Timer: optional timer that starts after the chamber reaches the target band.
- Chamber Stability Lock: reports readiness only after stable temperature/soak behavior.
- Filter Life Counter: counts fan runtime and warns when filter life limit is reached.
- Heater Wear / Aging Trend: compares future health-test results to the first baseline.
- Airflow / Blocked Filter Detection: warns when PTC gets hot but chamber does not warm as expected.
- PLA Protection Mode: warning-only protection when U1 reports PLA and chamber target is too high.
- Smart Resume After Pause: starts a short recovery window when printing resumes after pause/error.
- Post-Print Pickup Mode: framework for keep-warm-until-open / notify-only pickup behavior.
- Print Risk Score: warning-only material/chamber risk estimate.
- Start Print Warning: warns if printing starts before chamber/heat soak is ready.
- Local Recipes: active recipe slot/name placeholders for Android-managed recipe profiles.
- Export / Import Settings: REST status JSON can be exported; POST /api/settings can import matching fields.
- Firmware Demo Mode: simulates U1 state/material/progress for app demos without live printer data.
- Safety Score / Setup Validation: scores sensors, Moonraker, safety features and heater build state.

## Philosophy

The new logic is warning-first. It does not cancel heating or stop the print because of mismatch/risk warnings. Local hardware faults such as sensor errors or overtemperature still disable heater output.

## BLE / REST examples

```json
{"heat_soak_enabled":true,"heat_soak_min":15,"heat_soak_band_c":2}
```

```json
{"filter_life_counter_enabled":true,"filter_life_limit_h":120}
```

```json
{"pla_protection_enabled":true,"ack_pla_protection":true}
```

```json
{"demo_mode_enabled":true}
```

```json
{"local_recipes_enabled":true,"active_recipe_slot":2,"active_recipe_name":"ASA Large Print"}
```

```json
{"ack_print_risk_warning":true,"ack_start_print_warning":true,"ack_setup_warning":true}
```

## Android UI hints

The Android app should show these as optional sections:

- Ready prediction and heat soak
- Filter/service counters
- Material safety warnings
- Print risk panel
- Setup safety checklist
- Demo mode switch for videos/testing
- Recipes import/export screen
