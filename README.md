# Battery Inspector

Battery Inspector is an Ubuntu Touch app that shows the battery telemetry the
system settings hide. It reads `/sys/class/power_supply` read-only and, where
available, supplements it with UPower over the system D-Bus.

It is built for Ubuntu Touch 24.04 with Qt 5.15 and Lomiri.Components 1.3,
targets arm64, requires no root, and never uses AppArmor `unconfined`.

## What it shows

- Charge percentage, charging state, time to empty/full.
- Voltage, current, power, and temperature.
- Health, wear, charge cycles, full and design capacity/energy, technology.
- Charger type, charge counter, input current limit, constant charge current,
  maximum voltage, charger temperature.
- Every readable sysfs attribute of every power supply, plus every UPower
  property, in a filterable raw table.
- A five-minute live graph for voltage, current, power, or charge.
- A JSON report for diagnostics and adding support for new devices.
- English and Russian UI.

On the Yandex Phone (Amber, sdm660) the kernel splits data between `battery`
(`qpnp-smb2`) and `bms` (`qpnp-fg-gen3`); the app merges both, including the
design capacity, cycle count, OCV, resistance, and average time estimates that
live in `bms`.

## Install

Build or download the arm64 click, then:

```bash
adb push batteryinspector.sprainbrains_1.0.0_arm64.click /home/phablet/
adb shell pkcon install-local --allow-untrusted /home/phablet/batteryinspector.sprainbrains_1.0.0_arm64.click
```

Launch it from the app grid.

## Build

Host build and tests (Qt 5 from Homebrew):

```bash
brew install qt@5
cmake -S . -B build-host -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)" -DBUILD_TESTING=ON
cmake --build build-host -j"$(nproc)"
ctest --test-dir build-host --output-on-failure
```

Clickable release build:

```bash
clickable build --arch arm64
```

For a host without Docker, `scripts/dev-build-arm64.sh` builds and tests the
arm64 package inside an Ubuntu 24.04 arm64 rootfs with proot + qemu. See
`docs/development.md`.

## Diagnostics

Open the info action and use:

- **Copy report** — copies the JSON report to the clipboard.
- **Save report** — writes `~/.local/share/batteryinspector.sprainbrains/report.json`.

Pull the report from the device with:

```bash
adb pull /home/phablet/.local/share/batteryinspector.sprainbrains/report.json
```

The report contains sysfs/UPower telemetry only; no personal data. It is the
easiest way to add support for another device.

A developer can also run:

```bash
batteryinspector --dump --pretty
```

## Data sources

- `/sys/class/power_supply/*` — kernel attributes, including `battery` and
  `bms` on Amber.
- `org.freedesktop.UPower` — optional normalized values over the system bus.
  If the bus is unavailable or blocked, the app continues on sysfs.

## Privacy

No network access, no background service, no analytics, no writes to sysfs.

## Known limitations

- The first release targets `ubuntu-touch-24.04-1.x`; a 20.04 build is not
  tested.
- UPower is optional; some devices expose fewer fields.
- The live graph is in-memory only and resets when the app restarts.
- No charge control or charging thresholds.

## OpenRepos

The package is a standard arm64 click for `ubuntu-touch-24.04-1.x`. The
`manifest.json` and AppArmor policy are ready for publication; fill in the
maintainer email first.

## License

GPL-3.0-or-later. See `LICENSE`.
