# DIY Sourcing Examples

This page lists practical example parts and AliExpress search links for a low-cost DIY SnapHeater-compatible build.

Listings on marketplaces change often. Treat these links as search starting points, not as endorsements of a specific seller. Always verify ratings, photos, dimensions, current limits and reviews before buying.

## Quick Safety Rule

Cheap is fine for low-risk parts. Do not buy the cheapest unknown option for:

- power supplies,
- heater wiring,
- heater connectors,
- fuses,
- thermal cutoffs,
- MOSFET modules that carry heater current.

For anything connected to mains AC, prefer certified local or reputable-brand parts. The DIY reference here is intended for low-voltage DC heater builds.

## Example Low-Cost Parts

| Function | Example search/model | What to look for | AliExpress search |
|---|---|---|---|
| Controller | ESP32-C3 SuperMini | USB-C version, clearly labeled pins, ESP32-C3 not ESP8266/ESP32-S2 | [ESP32-C3 SuperMini](https://www.aliexpress.com/wholesale?SearchText=ESP32-C3+SuperMini) |
| Safer small controller option | Seeed Studio XIAO ESP32C3 compatible | Better documentation, small board, more expensive than SuperMini | [XIAO ESP32C3](https://www.aliexpress.com/wholesale?SearchText=XIAO+ESP32C3) |
| Heater MOSFET module | D4184 / AOD4184 MOSFET module | Logic-level input, large terminals, heatsink area, seller states 3.3 V compatible | [D4184 MOSFET module](https://www.aliexpress.com/wholesale?SearchText=D4184+MOSFET+module) |
| Alternative MOSFET module | High-current MOSFET trigger switch module | DC low-side switch, 3.3 V trigger, current rating well above heater current | [MOSFET trigger switch module 3.3V](https://www.aliexpress.com/wholesale?SearchText=MOSFET+trigger+switch+module+3.3V) |
| Fan MOSFET module | Small logic-level MOSFET module | Fan current rating, flyback/TVS protection or add protection externally | [logic level MOSFET module](https://www.aliexpress.com/wholesale?SearchText=logic+level+MOSFET+module) |
| Buck converter | MP1584 or LM2596 buck module | Input voltage above supply, output current margin, adjustable output, heatsinking | [MP1584 buck converter](https://www.aliexpress.com/wholesale?SearchText=MP1584+buck+converter) |
| Chamber sensor | 100K NTC B3950 waterproof probe | 100K, B3950, 1 percent if possible, stainless probe, PTFE/silicone wire preferred | [100K NTC B3950 waterproof](https://www.aliexpress.com/wholesale?SearchText=100K+NTC+B3950+waterproof) |
| PTC-local sensor | 100K NTC B3950 high-temp lead | Same electrical type as firmware config, higher temperature wire insulation | [100K NTC B3950 high temperature](https://www.aliexpress.com/wholesale?SearchText=100K+NTC+B3950+high+temperature) |
| PTC heater module | 12 V or 24 V PTC air heater | Known voltage/current, ceramic PTC, airflow path, do not oversize for first build | [24V PTC air heater](https://www.aliexpress.com/wholesale?SearchText=24V+PTC+air+heater) |
| Fan/blower | 24 V 5015 blower or axial fan | Voltage matches PSU, airflow adequate, ball bearing preferred | [24V 5015 blower fan](https://www.aliexpress.com/wholesale?SearchText=24V+5015+blower+fan) |
| Fuse holder | Automotive blade fuse holder | Current rating above normal load, wire gauge suitable, replaceable fuse | [inline blade fuse holder](https://www.aliexpress.com/wholesale?SearchText=inline+blade+fuse+holder) |
| Thermal cutoff | KSD9700 / KSD301 thermostat or thermal fuse | Normally-closed cutoff, temperature chosen below unsafe enclosure limit | [KSD9700 normally closed thermostat](https://www.aliexpress.com/wholesale?SearchText=KSD9700+normally+closed+thermostat) |
| Main connector | XT30 / XT60 | Genuine or high-quality clone, current rating above fuse, secure soldering | [XT30 connector](https://www.aliexpress.com/wholesale?SearchText=XT30+connector) |
| Wire | Silicone insulated wire | Gauge sized for fuse/current, heat-resistant insulation | [silicone wire 18AWG](https://www.aliexpress.com/wholesale?SearchText=silicone+wire+18AWG) |
| Physical button | Momentary push button | Normally-open momentary button, panel mount if enclosure is used | [momentary push button panel mount](https://www.aliexpress.com/wholesale?SearchText=momentary+push+button+panel+mount) |

## Recommended Budget Architecture

For a first DIY prototype, prefer 24 V DC over 12 V DC for the same heater power because current is lower.

Example:

```text
24 V 100 W heater -> about 4.2 A
12 V 100 W heater -> about 8.3 A
```

Lower current usually means easier wiring, lower connector stress and less MOSFET heating.

## First-Build Power Target

For early validation, keep heater power modest:

```text
recommended first DIY heater: 24 V, 50-100 W
avoid first build: high-power 200-400 W heaters
```

Large heaters can be added later only after fan behavior, sensor placement and cutoff behavior are proven.

## Parts To Prefer

Prefer:

- 24 V low-voltage DC PTC heater modules,
- MOSFET modules with screw terminals,
- silicone insulated wire,
- automotive blade fuses,
- real thermal cutoffs mounted near the heater,
- ESP32-C3 boards with clear pin labels,
- fans with known voltage and current rating.

## Parts To Avoid

Avoid:

- IRF520 modules for heater control if driven directly from 3.3 V GPIO,
- relay modules for fast heater modulation,
- tiny MOSFET boards with no thermal margin,
- solderless breadboards for heater current,
- unmarked power supplies,
- unknown PTC heaters with no voltage/current rating,
- mains AC heater modules for a first DIY build.

## MOSFET Selection Notes

Check these before buying:

- logic-level behavior at 3.3 V gate drive,
- continuous current rating above calculated heater current,
- low on-resistance at logic gate voltage,
- adequate PCB copper area or heatsink,
- screw terminal current rating,
- input pulldown or add your own pulldown.

Do not trust headline current ratings such as "30 A" on tiny low-cost boards without derating.

## Power Supply Notes

The cheapest unbranded open-frame mains power supplies are not recommended for unattended heater projects.

Safer options:

- certified external DC brick from a reputable seller,
- branded enclosed supply such as Mean Well or equivalent,
- bench supply for development testing,
- fuse on the heater branch even if the PSU has protection.

AliExpress search starting points:

- [24V 5A power adapter](https://www.aliexpress.com/wholesale?SearchText=24V+5A+power+adapter)
- [Mean Well 24V power supply](https://www.aliexpress.com/wholesale?SearchText=Mean+Well+24V+power+supply)

Verify authenticity and safety markings. Counterfeit or clone power supplies are possible.

## Example Starter Cart

For a cheap but reasonable first bench build:

- ESP32-C3 SuperMini or XIAO ESP32C3-compatible board
- 24 V 50-100 W PTC air heater
- 24 V fan or blower
- 24 V 5 A or higher DC supply
- D4184/AOD4184-style MOSFET module for heater
- smaller logic-level MOSFET module for fan
- two 100K B3950 NTC probes
- inline blade fuse holder and suitable fuses
- KSD9700 normally-closed thermal cutoff
- XT30 or screw-terminal power connector
- silicone wire sized for current
- momentary OFF/safe-stop button

After buying, follow [VALIDATION_CHECKLIST.md](VALIDATION_CHECKLIST.md) before enabling any heater-related feature.
