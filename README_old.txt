CURRENT DEVICE STATUS - Sep 2022
=====================

This version is based on the original one from a1wong/it87
with updates from around the Internet.

It should be a superset of the chipsets found in the official
Linux kernel module, and includes any changes found there.

The level of support depends on what the community provides, as
I am only collating changes, not developing then.

Currently supported devices (from it87.c) are:

 *  Supports: IT8603E  Super I/O chip w/LPC interface
 *            IT8606E  Super I/O chip w/LPC interface
 *            IT8607E  Super I/O chip w/LPC interface
 *            IT8613E  Super I/O chip w/LPC interface
 *            IT8620E  Super I/O chip w/LPC interface
 *            IT8622E  Super I/O chip w/LPC interface
 *            IT8623E  Super I/O chip w/LPC interface
 *            IT8625E  Super I/O chip w/LPC interface
 *            IT8628E  Super I/O chip w/LPC interface
 *            IT8528E  Super I/O chip w/LPC interface
 *            IT8655E  Super I/O chip w/LPC interface
 *            IT8665E  Super I/O chip w/LPC interface
 *            IT8686E  Super I/O chip w/LPC interface
 *            IT8688E  Super I/O chip w/LPC interface
 *            IT8689E  Super I/O chip w/LPC interface
 *            IT8695E  Super I/O chip w/LPC interface
 *            IT8705F  Super I/O chip w/LPC interface
 *            IT8712F  Super I/O chip w/LPC interface
 *            IT8716F  Super I/O chip w/LPC interface
 *            IT8718F  Super I/O chip w/LPC interface
 *            IT8720F  Super I/O chip w/LPC interface
 *            IT8721F  Super I/O chip w/LPC interface
 *            IT8726F  Super I/O chip w/LPC interface
 *            IT8728F  Super I/O chip w/LPC interface
 *            IT8732F  Super I/O chip w/LPC interface
 *            IT8736F  Super I/O chip w/LPC interface
 *            IT8738E  Super I/O chip w/LPC interface
 *            IT8758E  Super I/O chip w/LPC interface
 *            IT8771E  Super I/O chip w/LPC interface
 *            IT8772E  Super I/O chip w/LPC interface
 *            IT8781F  Super I/O chip w/LPC interface
 *            IT8782F  Super I/O chip w/LPC interface
 *            IT8783E/F Super I/O chip w/LPC interface
 *            IT8786E  Super I/O chip w/LPC interface
 *            IT8790E  Super I/O chip w/LPC interface
 *            IT8792E  Super I/O chip w/LPC interface
 *            IT87952E  Super I/O chip w/LPC interface
 *            Sis950   A clone of the IT8705F

Updated Module Parameters
-------------------------

* force_id [short, short]

  Force multiple chip ID to specified value, separated by `,'.
  For example "force_id=0x8689,0x8633".  A value of 0 is ignored
  for that chip.
  Note: A single force_id value (e.g. "force_id=0x8689") is used for
  all chips, to only set the first chip use "force_id=0x8689,0".
  Should only be used for testing.

Frank Crawford
Jan 2023

The original README is below.
---------------------------------------------------------------------

DRIVER REMOVAL NOTICE
=====================

I have been unable to meet support demands for this driver, resulting
in unpleasant experience and frustration for everyone involved.

Consequently, the driver will be removed from github, effective
August 1, 2018. Interested parties are encouraged to clone the driver
before that time and to start maintaining it on their own.


Kernel driver it87
==================

Supported chips:
  * IT8603E/IT8623E
    Prefix: 'it8603'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8613E
    Prefix: 'it8613'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8620E
    Prefix: 'it8620'
    Addresses scanned: from Super I/O config space (8 I/O ports)
  * IT8622E
    Prefix: 'it8622'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8625E
    Prefix: 'it8625'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8628E
    Prefix: 'it8628'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8705F
    Prefix: 'it87'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Once publicly available at the ITE website, but no longer
  * IT8712F
    Prefix: 'it8712'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Once publicly available at the ITE website, but no longer
  * IT8716F/IT8726F
    Prefix: 'it8716'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Once publicly available at the ITE website, but no longer
  * IT8718F
    Prefix: 'it8718'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Once publicly available at the ITE website, but no longer
  * IT8720F
    Prefix: 'it8720'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8721F/IT8758E
    Prefix: 'it8721'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8728F
    Prefix: 'it8728'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8732F
    Prefix: 'it8732'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8771E
    Prefix: 'it8771'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8772E
    Prefix: 'it8772'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8781F
    Prefix: 'it8781'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8782F
    Prefix: 'it8782'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8783E/F
    Prefix: 'it8783'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8786E
    Prefix: 'it8786'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * IT8790E
    Prefix: 'it8790'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: Not publicly available
  * SiS950   [clone of IT8705F]
    Prefix: 'it87'
    Addresses scanned: from Super I/O config space (8 I/O ports)
    Datasheet: No longer be available

Authors:
    Christophe Gauthron
    Jean Delvare <jdelvare@suse.de>
    Guenter Roeck <linux@roeck-us.net>


Building & Installing
---------------------

* make clean
* make
* sudo make install


Using DKMS
----------

To install:

* sudo make dkms

To remove:

* sudo make dkms_clean

Notes:

* The module does not provide a real version number, so `git describe --long`
  is used to create one. This means that anything that changes the git state
  will change the version. `make dkms_clean` should be run before making a
  commit or an update with `git pull` as the Makefile is currently unable to
  track the last installed version to replace it. If this doesn't happen, the
  old version will need to be manually removed from dkms, before installing
  the updated module.
  Something like `dkms remove -m it87 -v <old version> --all`, followed by
  `rm -rf /usr/src/it87-<old version>`, should do.
  `dkms status it87` can be used to list the installed versions.


Module Parameters
-----------------

* update_vbat: int

  0 if vbat should report power on value, 1 if vbat should be updated after
  each read. Default is 0. On some boards the battery voltage is provided
  by either the battery or the onboard power supply. Only the first reading
  at power on will be the actual battery voltage (which the chip does
  automatically). On other boards the battery voltage is always fed to
  the chip so can be read at any time. Excessive reading may decrease
  battery life but no information is given in the datasheet.

* fix_pwm_polarity int

  Force PWM polarity to active high (DANGEROUS). Some chips are
  misconfigured by BIOS - PWM values would be inverted. This option tries
  to fix this. Please contact your BIOS manufacturer and ask him for fix.

* force_id

  Force chip ID to specified value. Should only be used for testing.

* ignore_resource_conflict

  Similar to acpi_enforce_resources=lax, but only affects this driver.
  ACPI resource conflicts are ignored if this parameter is provided and
  set to 1.
  Provided since there are reports that system-wide acpi_enfore_resources=lax
  can result in boot failures on some systems.
  Note: This is inherently risky since it means that both ACPI and this driver
  may access the chip at the same time. This can result in race conditions and,
  worst case, result in unexpected system reboots.

* mmio

  If set to 1, the driver uses MMIO to access the chip if supported. This is
  faster and less risky (untested!).


Hardware Interfaces
-------------------

All the chips supported by this driver are LPC Super-I/O chips, accessed
through the LPC bus (ISA-like I/O ports). The IT8712F additionally has an
SMBus interface to the hardware monitoring functions. This driver no
longer supports this interface though, as it is slower and less reliable
than the ISA access, and was only available on a small number of
motherboard models.


Description
-----------

This driver implements support for the IT8603E, IT8620E, IT8622E, IT8623E,
IT8628E, IT8705F, IT8712F, IT8716F, IT8718F, IT8720F, IT8721F, IT8726F, IT8728F,
IT8732F, IT8758E, IT8771E, IT8772E, IT8781F, IT8782F, IT8783E/F, IT8786E,
IT8790E, and SiS950 chips.

These chips are 'Super I/O chips', supporting floppy disks, infrared ports,
joysticks and other miscellaneous stuff. For hardware monitoring, they
include an 'environment controller' with 3 temperature sensors, 3 fan
rotation speed sensors, 8 voltage sensors, associated alarms, and chassis
intrusion detection.

The IT8712F and IT8716F additionally feature VID inputs, used to report
the Vcore voltage of the processor. The early IT8712F have 5 VID pins,
the IT8716F and late IT8712F have 6. They are shared with other functions
though, so the functionality may not be available on a given system.

The IT8718F and IT8720F also features VID inputs (up to 8 pins) but the value
is stored in the Super-I/O configuration space. Due to technical limitations,
this value can currently only be read once at initialization time, so
the driver won't notice and report changes in the VID value. The two
upper VID bits share their pins with voltage inputs (in5 and in6) so you
can't have both on a given board.

The IT8716F, IT8718F, IT8720F, IT8721F/IT8758E and later IT8712F revisions
have support for 2 additional fans. The additional fans are supported by the
driver.

The IT8716F, IT8718F, IT8720F, IT8721F/IT8758E, IT8732F, IT8781F, IT8782F,
IT8783E/F, and late IT8712F and IT8705F also have optional 16-bit tachometer
counters for fans 1 to 3. This is better (no more fan clock divider mess) but
not compatible with the older chips and revisions. The 16-bit tachometer mode
is enabled by the driver when one of the above chips is detected.

The IT8726F is just bit enhanced IT8716F with additional hardware
for AMD power sequencing. Therefore the chip will appear as IT8716F
to userspace applications.

The IT8728F, IT8771E, and IT8772E are considered compatible with the IT8721F,
until a datasheet becomes available (hopefully.)

The IT8603E/IT8623E is a custom design, hardware monitoring part is similar to
IT8728F. It only supports 3 fans, 16-bit fan mode, and the full speed mode
of the fan is not supported (value 0 of pwmX_enable).

The IT8620E and IT8628E are custom designs, hardware monitoring part is similar
to IT8728F. It only supports 16-bit fan mode. Both chips support up to 6 fans.

The IT8790E supports up to 3 fans. 16-bit fan mode is always enabled.

The IT8732F supports a closed-loop mode for fan control, but this is not
currently implemented by the driver.

Temperatures are measured in degrees Celsius. An alarm is triggered once
when the Overtemperature Shutdown limit is crossed.

Fan rotation speeds are reported in RPM (rotations per minute). An alarm is
triggered if the rotation speed has dropped below a programmable limit. When
16-bit tachometer counters aren't used, fan readings can be divided by
a programmable divider (1, 2, 4 or 8) to give the readings more range or
accuracy. With a divider of 2, the lowest representable value is around
2600 RPM. Not all RPM values can accurately be represented, so some rounding
is done.

Voltage sensors (also known as IN sensors) report their values in volts. An
alarm is triggered if the voltage has crossed a programmable minimum or
maximum limit. Note that minimum in this case always means 'closest to
zero'; this is important for negative voltage measurements. On most chips, all
voltage inputs can measure voltages between 0 and 4.08 volts, with a resolution
of 0.016 volt.  IT8603E, IT8721F/IT8758E and IT8728F can measure between 0 and
3.06 volts, with a resolution of 0.012 volt.  IT8732F can measure between 0 and
2.8 volts with a resolution of 0.0109 volt.  The battery voltage in8 does not
have limit registers.

On the IT8603E, IT8620E, IT8628E, IT8721F/IT8758E, IT8732F, IT8781F, IT8782F,
and IT8783E/F, some voltage inputs are internal and scaled inside the chip:
* in3 (optional)
* in7 (optional for IT8781F, IT8782F, and IT8783E/F)
* in8 (always)
* in9 (relevant for IT8603E only)
The driver handles this transparently so user-space doesn't have to care.

The VID lines (IT8712F/IT8716F/IT8718F/IT8720F) encode the core voltage value:
the voltage level your processor should work with. This is hardcoded by
the mainboard and/or processor itself. It is a value in volts.

If an alarm triggers, it will remain triggered until the hardware register
is read at least once. This means that the cause for the alarm may already
have disappeared! Note that in the current implementation, all hardware
registers are read whenever any data is read (unless it is less than 1.5
seconds since the last update). This means that you can easily miss
once-only alarms.

Out-of-limit readings can also result in beeping, if the chip is properly
wired and configured. Beeping can be enabled or disabled per sensor type
(temperatures, voltages and fans.)

The IT87xx only updates its values each 1.5 seconds; reading it more often
will do no harm, but will return 'old' values.

To change sensor N to a thermistor, 'echo 4 > tempN_type' where N is 1, 2,
or 3. To change sensor N to a thermal diode, 'echo 3 > tempN_type'.
Give 0 for unused sensor. Any other value is invalid. To configure this at
startup, consult lm_sensors's /etc/sensors.conf. (4 = thermistor;
3 = thermal diode)


Fan speed control
-----------------

The fan speed control features are limited to manual PWM mode. Automatic
"Smart Guardian" mode control handling is only implemented for older chips
(see below.) However if you want to go for "manual mode" just write 1 to
pwmN_enable.

If you are only able to control the fan speed with very small PWM values,
try lowering the PWM base frequency (pwm1_freq). Depending on the fan,
it may give you a somewhat greater control range. The same frequency is
used to drive all fan outputs, which is why pwm2_freq and pwm3_freq are
read-only.


Automatic fan speed control (old interface)
-------------------------------------------

The driver supports the old interface to automatic fan speed control
which is implemented by IT8705F chips up to revision F and IT8712F
chips up to revision G.

This interface implements 4 temperature vs. PWM output trip points.
The PWM output of trip point 4 is always the maximum value (fan running
at full speed) while the PWM output of the other 3 trip points can be
freely chosen. The temperature of all 4 trip points can be freely chosen.
Additionally, trip point 1 has an hysteresis temperature attached, to
prevent fast switching between fan on and off.

The chip automatically computes the PWM output value based on the input
temperature, based on this simple rule: if the temperature value is
between trip point N and trip point N+1 then the PWM output value is
the one of trip point N. The automatic control mode is less flexible
than the manual control mode, but it reacts faster, is more robust and
doesn't use CPU cycles.

Trip points must be set properly before switching to automatic fan speed
control mode. The driver will perform basic integrity checks before
actually switching to automatic control mode.


Temperature offset attributes
-----------------------------

The driver supports temp[1-3]_offset sysfs attributes to adjust the reported
temperature for thermal diodes or diode-connected thermal transistors.
If a temperature sensor is configured for thermistors, the attribute values
are ignored. If the thermal sensor type is Intel PECI, the temperature offset
must be programmed to the critical CPU temperature.

Preliminary support
-------------------

Support for IT8607E is preliminary. Voltage readings, temperature readings,
fan control, and fan speed measurements may be wrong and/or missing.
Fan control and fan speed may be enabled and reported for non-existing
fans. Please report any problems and inconsistencies.

Reporting information for unsupported chips
-------------------------------------------

If the chip in your system is not yet supported by the driver, please provide
the following information.

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

Please also report your board type as well as voltages and fan settings from
the BIOS. If possible, connect fans to different fan headers and let us know
if all fans are detected and reported.

This information _might_ give us enough information to add experimental support
for the chip in question. No guarantees, though - unless a datasheet is
available, something is likely to be wrong.

A note on sensors-detect
========================

There is a persistent perception that changes in this driver would have impact
on the output of sensors-detect. This is not the case. sensors-detect is an
independent application. Changes in this driver do not affect sensors-detect,
and changes in sensors-detect do not affect this driver.
