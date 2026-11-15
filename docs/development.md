# Development

## Host build and tests

The host build uses Qt 5.15 and is enough for every C++ test and for checking
the QML syntax. It does not run the Lomiri UI, because Lomiri.Components is
only present on the device.

```bash
brew install qt@5
cmake -S . -B build-host -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)" -DBUILD_TESTING=ON
cmake --build build-host -j"$(nproc)"
ctest --test-dir build-host --output-on-failure
qmllint qml/Main.qml qml/components/*.qml
```

The test suite covers sysfs parsing, unit conversion and merging, history
windows, JSON reports, UPower key normalization, and the provider. The
`translations` test fails if any `i18n.tr()` string is missing from
`po/ru.po`.

## Arm64 build without Docker

`scripts/dev-build-arm64.sh` runs CMake, CTest, `cmake --install` and
`click build` inside an Ubuntu 24.04 arm64 rootfs with proot + qemu. The
rootfs needs:

```bash
apt-get install -y cmake g++ qtbase5-dev qtdeclarative5-dev \
    qtdeclarative5-dev-tools gettext python3 click
```

Example:

```bash
ARM64_ROOTFS=$HOME/ut-rootfs \
PROOT=/path/to/proot \
QEMU=/path/to/qemu-aarch64-static \
scripts/dev-build-arm64.sh
```

The resulting `batteryinspector_1.0.0_arm64.click` is written to the repo
root and is ignored by git.

## Device install

```bash
adb push batteryinspector_1.0.0_arm64.click /home/phablet/
adb shell pkcon install-local --allow-untrusted /home/phablet/batteryinspector_1.0.0_arm64.click
```

Launch it from the app grid, or use `lomiri-app-launch` if you know the
hook name. The app writes its report to:

```
~/.local/share/batteryinspector.sprainbrains/report.json
```

Pull it with:

```bash
adb pull /home/phablet/.local/share/batteryinspector.sprainbrains/report.json
```

## Adding a device

1. Install the app on the device.
2. Open it, tap the info action, then "Save report".
3. Send `report.json` (it contains sysfs attributes and UPower properties,
   no personal data).

The Amber/Yandex Phone (sdm660) case is special: the kernel splits battery
data between `battery` (`qpnp-smb2`: status, health, charge type) and `bms`
(`qpnp-fg-gen3`: design capacity, cycle count, `time_to_*_avg`, OCV,
resistance). The provider merges both.

## Translations

The UI uses gettext through `i18n.tr()`. To update the template and the
Russian catalogue:

```bash
xgettext --from-code=UTF-8 --language=JavaScript \
    --keyword=tr --keyword=i18n.tr --keyword=_ \
    -o po/batteryinspector.sprainbrains.pot \
    qml/Main.qml qml/components/*.qml
msgmerge --update po/ru.po po/batteryinspector.sprainbrains.pot
python3 scripts/check-translations.py .
```

The desktop entry is generated with:

```bash
msgfmt --desktop -l ru \
    --template=batteryinspector.desktop.in \
    -o build-host/batteryinspector.desktop po/ru.po
```

## AppArmor

The app uses a read-only policy:

```json
"read_path": [
  "/sys/class/power_supply/",
  "/sys/class/power_supply/**",
  "/sys/devices/**/power_supply/",
  "/sys/devices/**/power_supply/**"
]
```

UPower is optional. If the system bus is unavailable or blocked, the app
continues on sysfs and reports `UPower: unavailable`. The policy never uses
`unconfined`.

## Known limitations

- The first release targets `ubuntu-touch-24.04-1.x`; a 20.04 build is not
  tested.
- UPower is not a hard dependency. Some devices expose fewer fields.
- The live graph is in-memory only and resets when the app restarts.
- No network access, no background service, no charge control.
