# Battery Inspector

Battery Inspector is a small Ubuntu Touch app that shows detailed battery telemetry the system settings hide, gathered read-only from `/sys/class/power_supply` and, where available, from UPower over the system D-Bus, and refreshed on a configurable 1/2/5/10 s interval with a five-minute in-memory history. It builds against Qt 5.15 and Lomiri.Components 1.3 for the `ubuntu-touch-24.04-1.x` framework, targets arm64, requires no root or AppArmor `unconfined` access, writes nothing to sysfs, and makes no network requests in v1.

## Build

Host build (Qt 5 from Homebrew on Linux/macOS):

```bash
brew install qt@5
cmake -S . -B build-host -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)" -DBUILD_TESTING=ON
cmake --build build-host -j"$(nproc)"
```

Device build and install (clickable, arm64):

```bash
clickable build --arch arm64
clickable install
```

Launch the installed app from the app grid or with `batteryinspector`. Translation `.mo` files are installed under `share/locale` and loaded from the binary's own directory.

## Data Sources

- `/sys/class/power_supply/*` attributes: capacity, status, voltage, current, power, charge, energy, temperature, health, charge cycles, and related values.
- UPower over the system bus (`org.freedesktop.UPower`) for values the kernel does not expose on a given device.

## License

Battery Inspector is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See `LICENSE` for the full text.
