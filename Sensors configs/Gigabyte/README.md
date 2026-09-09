# Gigabyte it87 sensors.d installation

This package installs only the sensor mappings for the current Gigabyte
motherboard into:

    /etc/sensors.d/gigabyte-it87.conf

The installer reads the raw SIV ID from the active `it87` hwmon name. The
`it87` driver therefore needs to expose names using the raw SIV suffix, for
example:

    it8689_a0040507

If a motherboard has more than one supported Super I/O, every matching stanza
for that SIV ID is installed. Mappings for unrelated SIV IDs are not copied to
`/etc/sensors.d`.

## Install

Run:

```bash
sudo ./install-sensorsd.sh
```

The installer detects the CPU platform and SIV ID automatically.

To install into another directory, such as for testing:

```bash
./install-sensorsd.sh --dest /tmp/sensors.d
```

## Manual SIV selection

If automatic SIV detection is unavailable, provide the raw eight-digit SIV ID:

```bash
sudo ./install-sensorsd.sh --siv-id a0040507
```

The `0x` prefix is also accepted:

```bash
sudo ./install-sensorsd.sh --siv-id 0xA0040507
```

The CPU vendor can also be overridden when necessary:

```bash
sudo ./install-sensorsd.sh --vendor amd --siv-id a0040507
```

or:

```bash
sudo ./install-sensorsd.sh --vendor intel --siv-id 70050607
```

## Intel X299

Some Intel X299 SIV IDs have processor-dependent voltage mappings. For those
boards, select the processor family explicitly:

```bash
sudo ./install-sensorsd.sh --x299-variant kabylakex
```

or:

```bash
sudo ./install-sensorsd.sh --x299-variant skylakex
```

The installer refuses to guess the X299 processor mapping when one of these
SIV IDs is detected.

## Uninstall

Remove the installed Gigabyte `it87` sensors configuration with:

```bash
sudo ./install-sensorsd.sh --uninstall
```

This removes only:

    /etc/sensors.d/gigabyte-it87.conf

It does not remove or modify other files in `/etc/sensors.d`.

If the configuration was installed to a custom destination, pass the same
path when uninstalling:

```bash
./install-sensorsd.sh --uninstall --dest /tmp/sensors.d
```

Running `--uninstall` when the file is already absent is safe and exits
successfully.

## Command-line options

```text
--dest DIR
    sensors.d destination directory. Default: /etc/sensors.d

--vendor auto|amd|intel
    CPU platform selection. Default: auto

--siv-id HEXID
    Override automatic SIV detection. Accepts 12345678 or 0x12345678.

--x299-variant auto|kabylakex|skylakex
    Select the processor-dependent X299 mapping when required.

--uninstall
    Remove the installed gigabyte-it87.conf from the selected destination.

-h, --help
    Show command-line help.
```

## Installed configuration behavior

Only `chip` stanzas matching the active SIV ID are written to the installed
configuration. If a catalog stanza is shared by several SIV IDs, its installed
copy is narrowed to the current SIV ID. Multi-Super-I/O motherboards retain all
matching stanzas belonging to that same SIV ID.
