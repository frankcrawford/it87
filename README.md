# About the project

The `it87` kernel module provides support for [certain ITE Super I/O chips](#supported-chips). This fork introduces support for more chips and functionality to the module.

# Installing the module
## Installing with `make`
### Build
1. `make clean`
2. `make`

### Install for current kernel 
- `sudo make install`
### Install to DKMS
- `sudo make dkms`
### Remove from DKMS
- `sudo make dkms_clean`

### Notes:
* The module does not provide a real version number, so `git describe --long` is used to create one. This means that anything that changes the git state will change the version. `make dkms_clean` should be run before making a commit or an update with `git pull` as the Makefile is currently unable to track the last installed version to replace it. If this doesn't happen, the old version will need to be manually removed from dkms, before installing the updated module.

	Something like `dkms remove -m it87 -v <old version> --all`, followed by `rm -rf /usr/src/it87-<old version>`, should do.

	`dkms status it87` can be used to list the installed versions.

## Installing as a package: `.apk` (AKMS)/`.rpm` (akmods)/`.deb` (DKMS)
**Note:** `.apk` refers to Alpine Linux packages. The `.rpm` package also works with OSTree systems like Fedora Silverblue.

* Pre-packaged versions via CI can be found on the repo's releases page: **[Latest Release](../../releases/latest)** (**[All Releases](../../releases)**, **[Workflow Status](../../actions)**)
* A quick start guide for building packages locally can be found **[here](/packagetool_quickstart.md)**.

## Arch Linux AUR Package
* **https://aur.archlinux.org/packages/it87-dkms-git**

	**Note:** This package is maintained externally and may use a different repository than this one.

# Module Parameters
* `fix_pwm_polarity=`*\[bool\]*

	If `true`, force PWM polarity to active high **(DANGEROUS)**. Some chips are misconfigured by the motherboard firmware, causing PWM values to be inverted. This option tries to correct this. Please contact your motherboard manufacturer and ask them for a fix.

* `force_id=`*\[short, short\]*

	Force multiple chip ID to specified value, separated by `,`. For example `force_id=0x8689,0x8633`. A value of `0` is ignored for that chip.
	
	**Note:** A single force_id value (e.g. `force_id=0x8689`) is used for all chips, to only set the first chip use `force_id=0x8689,0`. Should only be used for testing.

* `ignore_resource_conflict=`*\[bool\]*

	Similar to `acpi_enforce_resources=lax`, but only affects this driver. Provided since there are reports that system-wide `acpi_enfore_resources=lax` can result in boot failures on some systems.
	
	**Note:** This is inherently risky since it means that both ACPI and this driver may access the chip at the same time. This can result in race conditions and, worst case, result in unexpected system reboots.

* `mmio=`*\[bool\]*

	If `true`, the driver uses MMIO to access the chip if supported. This is faster and less risky (untested!).

* `update_vbat=`*\[bool\]*

	`false` if vbat should report power on value, `true` if vbat should be updated after each read. Default is `false`. On some boards the battery voltage is provided by either the battery or the onboard power supply. Only the first reading at power on will be the actual battery voltage (which the chip does automatically). On other boards the battery voltage is always fed to the chip so can be read at any time. Excessive reading may decrease battery life but no information is given in the datasheet.

# Hardware Support
**Please note: The information below has been largely transferred from an older version of the readme and may be inaccurate or incomplete.**

## Hardware Interfaces
All the chips supported by this driver are LPC Super-I/O chips, accessed through the LPC bus (ISA-like I/O ports). The IT8712F additionally has an SMBus interface to the hardware monitoring functions. This driver no longer supports this interface though, as it is slower and less reliable than the ISA access, and was only available on a small number of motherboard models.

## Supported Chips
*Note about datasheets: Supposedly some used to be publicly available from the manufacturer. Nowadays they might be still available from third party sites, inclusion in this list is TBD.*

*Note about the list: At the moment the list is incomplete and may contain errors.*

| Chip | Prefix | Comments | Datasheet
| ---- | ------ | -------- | ---------
| IT8603E | `it8603` | | TBD |
| IT8606E | `it8606`  | | TBD |
| IT8607E | `it8607` | | TBD |
| IT8613E | `it8613` | | TBD |
| IT8620E | `it8620` | | TBD |
| IT8622E | `it8622` | | TBD |
| IT8623E | `it8603` | | TBD |
| IT8625E | `it8625` | | TBD |
| IT8628E | `it8628` | | TBD |
| IT8528E | ??? | | TBD |
| IT8655E | `it8655` | | TBD |
| IT8665E | `it8665` | | TBD |
| IT8686E | `it8686` | | TBD |
| IT8688E | `it8688` | | TBD |
| IT8689E | `it8689` | | TBD |
| IT8695E | ??? | | TBD |
| IT8705F | `it87` | | TBD |
| IT8712F | `it8712` | | TBD |
| IT8716F | `it8716` | | TBD |
| IT8718F | `it8718` | | TBD |
| IT8720F | `it8720` | | TBD |
| IT8721F | `it8721` | | TBD |
| IT8726F | `it8716` | | TBD |
| IT8728F | `it8728` | | TBD |
| IT8732F | `it8732` | | TBD |
| IT8736F | `it8736` | | TBD |
| IT8738E | `it8738` | | TBD |
| IT8758E | `it8721` | | TBD |
| IT8771E | `it8771` | | TBD |
| IT8772E | `it8772` | | TBD |
| IT8781F | `it8781` | | TBD |
| IT8782F | `it8782` | | TBD |
| IT8783E | `it8783` | | TBD |
| IT8783F | `it8783` | | TBD |
| IT8786E | `it8786` | | TBD |
| IT8790E | `it8790` | | TBD |
| IT8792E | `it8792` | | TBD |
| IT8795E | `it8792` | | TBD |
| IT87952E | `it87952` | | TBD |
| Sis950 | `it87` | A clone of the IT8705F | TBD |

These chips are "Super I/O chips", supporting floppy disks, infrared ports, joysticks and other miscellaneous stuff. For hardware monitoring, they include an "environment controller" with temperature sensors, fan rotation speed sensors, voltage sensors, associated alarms, and chassis intrusion detection.

* IT8712F and IT8716F additionally feature VID inputs, used to report the Vcore voltage of the processor. The early IT8712F have 5 VID pins, the IT8716F and late IT8712F have 6. They are shared with other functions though, so the functionality may not be available on a given system.

* IT8718F and IT8720F also features VID inputs (up to 8 pins) but the value is stored in the Super-I/O configuration space. Due to technical limitations, this value can currently only be read once at initialization time, so the driver won't notice and report changes in the VID value. The two upper VID bits share their pins with voltage inputs (in5 and in6) so you can't have both on a given board.

* IT8716F, IT8718F, IT8720F, IT8721F/IT8758E and later IT8712F revisions have support for 2 additional fans. The additional fans are supported by the driver.

* IT8716F, IT8718F, IT8720F, IT8721F/IT8758E, IT8732F, IT8781F, IT8782F, IT8783E/F, and late IT8712F and IT8705F also have optional 16-bit tachometer counters for fans 1 to 3. This is better (no more fan clock divider mess) but not compatible with the older chips and revisions. The 16-bit tachometer mode is enabled by the driver when one of the above chips is detected.

* IT8726F is just bit enhanced IT8716F with additional hardware for AMD power sequencing. Therefore the chip will appear as IT8716F to userspace applications.

* IT8728F, IT8771E, and IT8772E are considered compatible with the IT8721F, until a datasheet becomes available (hopefully.)

* IT8603E/IT8623E is a custom design, hardware monitoring part is similar to IT8728F. It only supports 3 fans, 16-bit fan mode, and the full speed mode of the fan is not supported (value 0 of pwmX_enable).

* IT8620E and IT8628E are custom designs, hardware monitoring part is similar to IT8728F. It only supports 16-bit fan mode. Both chips support up to 6 fans.

* IT8790E supports up to 3 fans. 16-bit fan mode is always enabled.

* IT8732F supports a closed-loop mode for fan control, but this is not currently implemented by the driver.

* IT87xx only updates its values each 1.5 seconds; reading it more often will do no harm, but will return 'old' values.

Temperatures are measured in degrees Celsius. An alarm is triggered once when the Overtemperature Shutdown limit is crossed.

Fan rotation speeds are reported in RPM (rotations per minute). An alarm is triggered if the rotation speed has dropped below a programmable limit. When 16-bit tachometer counters aren't used, fan readings can be divided by a programmable divider (1, 2, 4 or 8) to give the readings more range or accuracy. With a divider of 2, the lowest representable value is around 2600 RPM. Not all RPM values can accurately be represented, so some rounding is done.

Voltage sensors (also known as IN sensors) report their values in volts. An alarm is triggered if the voltage has crossed a programmable minimum or maximum limit. Note that minimum in this case always means 'closest to zero'; this is important for negative voltage measurements. On most chips, all voltage inputs can measure voltages between 0 and 4.08 volts, with a resolution of 0.016 volt.  IT8603E, IT8721F/IT8758E and IT8728F can measure between 0 and 3.06 volts, with a resolution of 0.012 volt.  IT8732F can measure between 0 and 2.8 volts with a resolution of 0.0109 volt.  The battery voltage in8 does not have limit registers.

On the IT8603E, IT8620E, IT8628E, IT8721F/IT8758E, IT8732F, IT8781F, IT8782F, and IT8783E/F, some voltage inputs are internal and scaled inside the chip:
* in3 (optional)
* in7 (optional for IT8781F, IT8782F, and IT8783E/F)
* in8 (always)
* in9 (relevant for IT8603E only)

The driver handles this transparently so user-space doesn't have to care.

The VID lines (IT8712F/IT8716F/IT8718F/IT8720F) encode the core voltage value: the voltage level your processor should work with. This is hardcoded by the mainboard and/or processor itself. It is a value in volts.

If an alarm triggers, it will remain triggered until the hardware register is read at least once. This means that the cause for the alarm may already have disappeared! Note that in the current implementation, all hardware registers are read whenever any data is read (unless it is less than 1.5 seconds since the last update). This means that you can easily miss once-only alarms.

Out-of-limit readings can also result in beeping, if the chip is properly wired and configured. Beeping can be enabled or disabled per sensor type (temperatures, voltages and fans.)


To change sensor N to a thermistor, 'echo 4 > tempN_type' where N is 1, 2, or 3. To change sensor N to a thermal diode, 'echo 3 > tempN_type'. Give 0 for unused sensor. Any other value is invalid. To configure this at startup, consult lm_sensors's /etc/sensors.conf. (4 = thermistor; 3 = thermal diode)

# Miscellaneous Information

## Fan speed control
The fan speed control features are limited to manual PWM mode. Automatic "Smart Guardian" mode control handling is only implemented for older chips [(see below.)](#automatic-fan-speed-control-old-interface) However if you want to go for "manual mode" just write 1 to pwmN_enable.

If you are only able to control the fan speed with very small PWM values, try lowering the PWM base frequency (pwm1_freq). Depending on the fan, it may give you a somewhat greater control range. The same frequency is used to drive all fan outputs, which is why pwm2_freq and pwm3_freq are read-only.

## Automatic fan speed control (old interface)
The driver supports the old interface to automatic fan speed control which is implemented by IT8705F chips up to revision F and IT8712F chips up to revision G.

This interface implements 4 temperature vs. PWM output trip points.

The PWM output of trip point 4 is always the maximum value (fan running at full speed) while the PWM output of the other 3 trip points can be freely chosen. The temperature of all 4 trip points can be freely chosen. Additionally, trip point 1 has an hysteresis temperature attached, to prevent fast switching between fan on and off.

The chip automatically computes the PWM output value based on the input temperature, based on this simple rule: if the temperature value is between trip point N and trip point N+1 then the PWM output value is the one of trip point N. The automatic control mode is less flexible than the manual control mode, but it reacts faster, is more robust and doesn't use CPU cycles.

Trip points must be set properly before switching to automatic fan speed control mode. The driver will perform basic integrity checks before actually switching to automatic control mode.

## Temperature offset attributes
The driver supports `temp[1-3]_offset` sysfs attributes to adjust the reported
temperature for thermal diodes or diode-connected thermal transistors.
If a temperature sensor is configured for thermistors, the attribute values
are ignored. If the thermal sensor type is Intel PECI, the temperature offset must be programmed to the critical CPU temperature.

## Preliminary support
Support for IT8607E is preliminary. Voltage readings, temperature readings,
fan control, and fan speed measurements may be wrong and/or missing.
Fan control and fan speed may be enabled and reported for non-existing
fans. Please report any problems and inconsistencies.

## Reporting information for unsupported chips
If the chip in your system is not yet supported by the driver, please provide the following information.

First, run sensors-detect. It will tell you something like

      Probing for Super-I/O at 0x2e/0x2f
      ...
      Trying family `ITE'...                                      Yes
      Found unknown chip with ID 0x8665
	(logical device 4 has address 0x290, could be sensors)

With this information, run the following commands.

	sudo isadump -k 0x87,0x01,0x55,0x55 0x2e 0x2f 7
	sudo isadump 0x295 0x296

and report the results.

The addresses in the first command are from "Probing for Super-I/O at
0x2e/0x2f". Use those addresses in the first command.

    sudo isadump -k 0x87,0x01,0x55,0x55 0x2e 0x2f 7

The addresses in the second command are from "has address 0x290".
Add 5 and 6 to this address for the next command.

    sudo isadump 0x295 0x296

Next, force-install the driver by providing one of the already supported chips
as forced ID. Useful IDs to test are 0x8622, 0x8628, 0x8728, and 0x8732, though
feel free to test more IDs. For each ID, instantiate the driver as follows
(this example is instantiating driver with ID 0x8622).

	sudo modprobe it87 force_id=0x8622

After entering this command, run the "sensors" command and provide the output.
Then unload the driver with

	sudo modprobe -r it87

Repeat with different chip IDs, and report each result.

Please also report your board type as well as voltages and fan settings from the BIOS. If possible, connect fans to different fan headers and let us know if all fans are detected and reported.

This information _might_ give us enough information to add experimental support for the chip in question. No guarantees, though - unless a datasheet is available, something is likely to be wrong.

## A note on sensors-detect:
There is a persistent perception that changes in this driver would have impact
on the output of sensors-detect. This is not the case. sensors-detect is an
independent application. Changes in this driver do not affect sensors-detect, and changes in sensors-detect do not affect this driver.

# Past and present maintainers

* Christophe Gauthron
* Jean Delvare \<jdelvare@suse.de\>
* Guenter Roeck \<linux@roeck-us.net\>
* *Incomplete List!*