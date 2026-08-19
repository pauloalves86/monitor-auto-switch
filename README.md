# monitor-auto-switch (`mas`)

A small Linux daemon that turns Synergy into a software KVM. It follows the Synergy log and, when the cursor moves to another computer, switches the monitor input over DDC/CI.

## How it works

1. `mas` watches `~/.local/state/Synergy/synergy.jsonl` with inotify. If only the legacy `synergy.log` exists, it watches that instead.
2. Each `switch from "A" to "B"` / `jump from ...` line is mapped to a VCP 0x60 input value using the config file.
3. **Debounce**: the switch is not applied right away. Every new event replaces the pending target and restarts a timer (`switch_delay_ms`, default 1000 ms).
4. **Reconcile**: when the timer fires, `mas` reads the monitor's current input. If the monitor is already on the target, nothing happens. If the monitor doesn't answer, the last value `mas` set is used instead.

So moving the cursor briefly to another machine and back within the delay leaves the monitor alone.

Monitor access goes directly through **libddcutil**, without spawning `ddcutil` processes.

Supported log formats:

```json
{"source":"core","level":"INFO","msg":"[TIMESTAMP] INFO: switch from \"device1\" to \"device2\" at X,Y"}
```
```
core       [TIMESTAMP] - INFO    - switch from "device1" to "device2" at X,Y
```

## Configuration

`~/.config/monitor-auto-switch/config.yaml` (or `$XDG_CONFIG_HOME/...`):

```yaml
version: 1
switch_delay_ms: 1000          # optional, 0 = switch immediately
default_monitor: asus-vg248qe  # optional, defaults to the first monitor

monitors:
  asus-vg248qe:
    display: 1                 # optional, `ddcutil detect` display number
    inputs:                    # Synergy screen name -> VCP 0x60 value
      mustang-2487237f: "0x0f" # DisplayPort-1
      m5matsect-ef9a27d9: "0x11" # HDMI-1
```

Find input values with `ddcutil capabilities | grep -A10 'Feature: 60'` or `ddcutil getvcp 60`. An older `actions:` section is accepted and ignored.

## Building

Dependencies (Arch): `ddcutil` (≥ 2.2.6), `yaml-cpp`, `nlohmann-json`, `gtest`, `cmake`, `ninja`. If `nlohmann-json` is not installed, CMake downloads it.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Running

```sh
./build/mas                      # uses the default config and log path
./build/mas --dry-run            # log decisions without touching the monitor
./build/mas --config PATH --log PATH
```

Your user needs access to `/dev/i2c-*`. Check with `ddcutil detect`; if needed, add yourself to the `i2c` group.

### As a systemd user service

`~/.config/systemd/user/mas.service`:

```ini
[Unit]
Description=Switch monitor input on Synergy screen changes

[Service]
ExecStart=%h/.local/bin/mas
Restart=on-failure

[Install]
WantedBy=default.target
```

```sh
cmake --install build --prefix ~/.local
systemctl --user enable --now mas
journalctl --user -u mas -f
```
