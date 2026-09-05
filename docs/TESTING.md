# Offline tests

The full host suite currently uses Windows mutex stubs. Use Python 3, native
Clang, and an activated ESP-IDF 5.3.5 environment with IDF_PATH set.
Set CC to your clang executable if it is not on PATH.

Build the default ESP32-C3 configuration first:

```powershell
idf.py set-target esp32c3
idf.py build
```

The full-loop simulation expects generated configuration under
build-heater-compile-test/config. Generate that separate compile-only build
using sdkconfig.defaults and sdkconfig.panda-safe.defaults, with
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=y in that isolated build's menuconfig.
This configuration is for compilation and offline simulation only; do not flash it.

```powershell
idf.py -B build-heater-compile-test -D SDKCONFIG=sdkconfig.compile-test -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.panda-safe.defaults" menuconfig
idf.py -B build-heater-compile-test -D SDKCONFIG=sdkconfig.compile-test build
python -m unittest discover -s tests -p "test_*.py" -v
./tests/run_safety_host_test.ps1 -Compiler clang
```

Tests use production logic with simulated ADC, GPIO, time and persistence.
They do not contact a device. Boot-selection tests extract the installed SDK's
selector, not proprietary firmware. Run from the repository root.

The separate [HIL runner](HIL.md) contacts a device and is not part of the offline
test command. Do not run it as an automatic publication check.
