# VolumePanning.lv2

An LV2 plugin that converts a mono input to a stereo output with volume, pan, mute, and bypass controls.

## Controls

| Port | Range | Default | Description |
|------|-------|---------|-------------|
| Pan | -1.0 to 1.0 | 0.0 | Stereo position — -1 full left, 0 centre, +1 full right |
| Volume | −60.0 to +20.0 dB | 0.0 | Output level — 0 dB is unity gain, negative values attenuate, positive values boost |
| Mute | 0 / 1 | 0 | Silences output when enabled |
| Mute Invert | 0 / 1 | 0 | Inverts the sense of Mute — when on, the plugin is silent while Mute is off and passes audio while Mute is on |
| Enabled | 0 / 1 | 1 | Bypass — when off, input is passed through to both channels unchanged |

Panning uses a constant-power (equal-power) law so perceived loudness stays consistent across the stereo field.

## Requirements

- GCC or compatible C99 compiler
- LV2 development headers (`lv2` pkg-config package)

On Debian/Ubuntu:

```sh
sudo apt install lv2-dev
```

On Arch:

```sh
sudo pacman -S lv2
```

## Build & Install

```sh
make
make install   # installs to ~/.lv2/volumepanning.lv2/
```

To uninstall:

```sh
make uninstall
```

## Maintainer

Frederick Price <fprice@pricemail.ca>

## License

BSD 3-Clause
