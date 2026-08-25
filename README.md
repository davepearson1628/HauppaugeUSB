#!/usr/bin/env -S retext --preview
[//]: # (Install retext from your distribution then ./README.md will look prettier.)

# MythTV Hauppauge HD-PVR2 / Colossus2 support

> **Fork status:** This repository is a maintained fork of John Poet's
> original HauppaugeUSB project. The original author is no longer able to
> continue development because he no longer owns the hardware.
>
> Development of this fork is currently being tested primarily with the
> Hauppauge HD PVR 2 Gaming Edition Plus and Colossus 2.

A wrapper around the Hauppauge HDPVR2/Colossus2 Linux "[driver](http://www.hauppauge.com/site/support/linux.html?#tabs-3)"

This project can be used at the command line as well as an "External Recorder"
for MythTV.

----

## News

### August 2026 - Automatic AAC / AC3 audio detection

This repository is a fork of the excellent original HauppaugeUSB work by
John Poet. His project provided the foundation for using the Hauppauge
HD PVR 2 and Colossus 2 devices under Linux and with MythTV.

This fork builds on that work with a number of changes intended to make
the driver easier to configure and use with modern setups.

The main addition is a new automatic audio mode (`codec=12`). The
application detects whether the incoming digital audio is PCM stereo or
Dolby Digital and automatically selects the appropriate encoder:

* PCM stereo input is encoded as AAC.
* Dolby Digital / AC3 input is encoded as AC3.

This allows a single configuration file to be used for channels which
alternate between stereo PCM and Dolby Digital audio, without maintaining
separate AAC and AC3 configurations.

Automatic audio detection has currently been tested successfully with the
following hardware and audio connections:

* **Hauppauge HD PVR 2 Gaming Edition Plus - HDMI:** PCM stereo is
  automatically encoded as AAC and Dolby Digital is automatically encoded
  as AC3.
* **Hauppauge HD PVR 2 Gaming Edition Plus - S/PDIF:** PCM stereo is
  automatically encoded as AAC and Dolby Digital is automatically encoded
  as AC3.
* **Hauppauge Colossus 2 - HDMI:** PCM stereo is automatically encoded as
  AAC and Dolby Digital is automatically encoded as AC3.
* **Hauppauge Colossus 2 - S/PDIF:** PCM stereo is automatically encoded as
  AAC and Dolby Digital is automatically encoded as AC3.

**HDMI audio note:** Automatic detection over HDMI depends on the audio
format actually supplied by the connected source device. HDMI audio
negotiation may cause some sources or intermediate HDMI equipment to output
PCM even when Dolby Digital content is available.

For the HD PVR 2 using HDMI audio:

```ini
input=3
audio=3
codec=12
```

For the HD PVR 2 using S/PDIF audio:

```ini
input=3
audio=1
codec=12
```

For the Colossus 2 using HDMI audio:

```ini
input=3
audio=3
codec=12
```

For the Colossus 2 using S/PDIF audio:

```ini
input=3
audio=1
codec=12
```

### August 2026 - One-shot snapshot capture

This fork also adds an optional one-shot snapshot facility. It allows another
application or script to request a JPEG image from the video stream currently
being captured by `hauppauge2`.

The snapshot is taken from the already-running capture stream. The Hauppauge
device is not opened a second time, and the normal MPEG-TS output continues
unchanged. This means the feature can be used both when `hauppauge2` is
writing directly to an output stream and when it is operating as a MythTV
External Recorder.

Enable it in the configuration file with:

```ini
mdp=true
mdp_image=/run/hauppauge2/mdp-live.jpg
mdp_delay=3000
```

The options are:

* `mdp` - enables or disables one-shot snapshot support.
* `mdp_image` - destination filename for the requested JPEG image.
* `mdp_delay` - delay, in milliseconds, between receiving a snapshot request
  and beginning the snapshot capture. The built-in default is 1500 ms.

A longer `mdp_delay` can be useful after a channel change when the video source
or an on-screen display needs time to settle. Testing with a Virgin TV360 set
top box found 3000 ms to be more reliable.

To request an image, create a file whose name is the configured `mdp_image`
with `.request` appended.

For example:

```bash
rm -f /run/hauppauge2/mdp-live.jpg
touch /run/hauppauge2/mdp-live.jpg.request
```

With the example configuration above, `hauppauge2` consumes:

```text
/run/hauppauge2/mdp-live.jpg.request
```

and writes a fresh JPEG to:

```text
/run/hauppauge2/mdp-live.jpg
```

The image is produced only in response to a request; it is not continuously
updated.

The snapshot facility does not perform image recognition itself. It simply
provides a current frame to an external application or script. This can be
used for purposes such as detecting an on-screen prompt after a channel
change without interfering with the stream being sent to MythTV, ffmpeg or
another application.

Snapshot generation uses `ffmpeg`, so `ffmpeg` must be installed if
`mdp=true` is used.

### Other improvements

This fork also includes several build and usability improvements:

* `make clean` no longer deletes the HauppaugeUSB application source files.
* The installation prefix can be changed rather than being permanently
  hard-coded.
* The supplied `sample.conf` has been updated for HDMI video, S/PDIF audio,
  automatic audio codec selection and optional snapshot support.
* Registry parameter handling has been corrected so that automatically
  detected audio settings can override values loaded from the configuration
  file.
* `hauppauge2 --help` now documents `codec=12` as the AUTO audio mode and
  the `mdp`, `mdp_image` and `mdp_delay` snapshot options.

The original fixes remain, including the correction for interlaced field
ordering and AC3 audio support.

----

## Installing

### Install dependencies

#### Fedora

```
sudo dnf install make gcc gcc-c++ kernel-devel libstdc++-devel boost-devel libusbx-devel
```

If you intend to use one-shot snapshot support, also install `ffmpeg` using
the appropriate repository for your Fedora installation.

#### Ubuntu

```
sudo apt-get install libboost-log-dev libboost-program-options-dev libboost-thread-dev libboost-filesystem-dev libusb-1.0-0-dev build-essential
```

If you intend to use one-shot snapshot support:

```
sudo apt-get install ffmpeg
```

#### MythTV

If you want to use this with MythTV, fixes/31 or later is recommended for
the best stability. It will work with earlier versions, but is not quite
as solid.

### Grab the "driver" from Hauppauge

```
mkdir -p ~/src/Hauppauge
cd ~/src/Hauppauge
curl -O https://s3.amazonaws.com/hauppauge/linux/hauppauge_hdpvr2_157321_patched_2016-09-26.tar.gz
tar -xzf hauppauge_hdpvr2_157321_patched_2016-09-26.tar.gz
```

### Grab this repository

```
cd ~/src/Hauppauge
git clone https://github.com/davepearson1628/HauppaugeUSB.git
```

### Link the Hauppauge source tree

```
cd ~/src/Hauppauge/HauppaugeUSB
ln -s ../hauppauge_hdpvr2_157321_patched_2016-09-26 Hauppauge
```

### Patch the Hauppauge source to get it working

```
cd ~/src/Hauppauge/HauppaugeUSB/Hauppauge
for fl in 01-NewLine.patch \
          02-string.patch \
          03-EnableRegisteredParameters.patch \
          04-SplitLoggingLevels.patch \
          05-FirmwareLocation.patch \
          06-AVOutputCallback.patch \
          07-ThreadName.patch \
          08-HDMI-AudioDetection.patch
do
    patch -p1 < ~/src/Hauppauge/HauppaugeUSB/Patches/"${fl}"
done
```

### Rename Common/Rx/ADV7842/Wrapper.c to Wrapper.cpp so it can include C++ headers

```
cd ~/src/Hauppauge/HauppaugeUSB/Hauppauge
mv Common/Rx/ADV7842/Wrapper.c Common/Rx/ADV7842/Wrapper.cpp
```

### Build it

The default installation directory is:

```
/opt/Hauppauge
```

Build and install with:

```
cd ~/src/Hauppauge/HauppaugeUSB
make
sudo make install
```

The installation directory can be changed using `PREFIX`. For example, to
install a test build alongside an existing installation:

```
sudo make install PREFIX=/opt/Hauppauge-new
```

The build can safely be cleaned and rebuilt with:

```
make clean
make
```

----

## Using it

### Permissions

hauppauge2 needs permission to use the device file(s). Create a udev rules
file. Something like:

```
nano /etc/udev/rules.d/99-Hauppauge.rules
```

And add appropriate rules:

```
# Device 1
SUBSYSTEMS=="usb",ATTRS{idVendor}=="2040",ATTR{serial}=="E505-00-00AF4321",MODE="0660",GROUP="video",SYMLINK+="hdpvr2_1",TAG+="systemd",RUN="/bin/sh -c 'echo -1 > /sys$devpath/power/autosuspend'"

# Device 2
SUBSYSTEMS=="usb",ATTRS{idVendor}=="2040",ATTR{serial}=="E585-00-00AF1234",MODE="0660",GROUP="video",SYMLINK+="colossus2-1",TAG+="systemd",RUN="/bin/sh -c 'echo -1 > /sys$devpath/power/autosuspend'"
```

At the least, you will need to adjust the serial number(s) to match your
device(s). Any user which wants to run hauppauge2 needs to be a member of
the GROUP specified.

### Running it

You can get the optional arguments with:

```
/opt/Hauppauge/bin/hauppauge2 --help
```

A lot of the options don't work unless just the right combination is
selected. The program does not currently protect you from choosing bad
combinations, because in many cases they *should* work, but have not been
implemented yet.

----

### Command line examples

#### List detected devices

```
$ /opt/Hauppauge/bin/hauppauge2 --list
[Bus: 5 Port: 1]  2040:0xe585 E585-00-00AF4321 Colossus 2
Number of possible configurations: 1  Device Class: 0  VendorID: 8256  ProductID: 58757
Manufacturer: Hauppauge
Serial: E585-00-00AF4321
Interfaces: 1 ||| Number of alternate settings: 1 | Interface Number: 0 | Number of endpoints: 6 | Descriptor Type: 5 | EP Address: 129 | Descriptor Type: 5 | EP Address: 132 | Descriptor Type: 5 | EP Address: 136 | Descriptor Type: 5 | EP Address: 1 | Descriptor Type: 5 | EP Address: 2 | Descriptor Type: 5 | EP Address: 134 |


[Bus: 3 Port: 4]  2040:0xe505 E505-00-00AF1234 HD PVR 2 Gaming Edition Plus w/SPDIF w/MIC
Number of possible configurations: 1  Device Class: 0  VendorID: 8256  ProductID: 58629
Manufacturer: Hauppauge
Serial: E505-00-00AF1234
Interfaces: 1 ||| Number of alternate settings: 1 | Interface Number: 0 | Number of endpoints: 6 | Descriptor Type: 5 | EP Address: 129 | Descriptor Type: 5 | EP Address: 132 | Descriptor Type: 5 | EP Address: 136 | Descriptor Type: 5 | EP Address: 1 | Descriptor Type: 5 | EP Address: 2 | Descriptor Type: 5 | EP Address: 134 |
```

#### Capture from HDMI video and S/PDIF audio with AC3 codec

```
/opt/Hauppauge/bin/hauppauge2 -s E585-00-00AF4321 -a 1 -d 2 -o /tmp/test.ts
```

#### Capture from Component video and S/PDIF AAC audio

```
/opt/Hauppauge/bin/hauppauge2 -s E505-00-00AF1234 -i 1 -a 1 -o /tmp/test.ts
```

#### Use a configuration file

The configuration file is just a list of `option=value` statements which
mimics using the long form on the command line. A `sample.conf` is included
which you can copy and modify.

```
cp /opt/Hauppauge/etc/sample.conf hdpvr2-1.conf
nano hdpvr2-1.conf
/opt/Hauppauge/bin/hauppauge2 -c hdpvr2-1.conf
```

### Automatic AAC / AC3 audio selection

This fork adds `codec=12`, which enables automatic audio codec selection for
supported digital audio inputs.

A typical configuration for HDMI video and S/PDIF audio is:

```
# input: 0=COMPOSITE, 1=COMPONENT, 2=SDI, 3=HDMI
input=3

# audio: 0=RCA, 1=SPDIF, 2=SDI, 3=HDMI
audio=1

# codec: 1=MPEG, 2=AC3, 3=AAC, 6=MP3, 8=PCM, 9=PASSTHROUGH, 12=AUTO
codec=12
```

For HDMI audio instead:

```
input=3
audio=3
codec=12
```

With `codec=12`, hauppauge2 examines the incoming digital audio format when
the device is initialized.

For PCM input it selects the AAC encoder. For Dolby Digital input it selects
the AC3 encoder.

This is particularly useful with set-top boxes where some channels provide
stereo PCM and others provide Dolby Digital. Previously these could require
different hauppauge2 configuration files. AUTO mode allows the same
configuration to be used for both.

If automatic detection cannot positively identify the incoming format,
the application falls back to AAC.

The connected source device should be configured to output the original
audio format where possible rather than permanently converting all audio
to PCM or Dolby Digital.

### One-shot snapshot capture

Snapshot support can be enabled in the same configuration file:

```ini
mdp=true
mdp_image=/run/hauppauge2/mdp-live.jpg
mdp_delay=3000
```

The request file is the configured image filename with `.request` appended.

For example:

```bash
rm -f /run/hauppauge2/mdp-live.jpg
touch /run/hauppauge2/mdp-live.jpg.request
```

The existing `hauppauge2` process will consume the request and create:

```text
/run/hauppauge2/mdp-live.jpg
```

The snapshot is taken from the active MPEG-TS capture without changing or
interrupting the normal output stream.

`mdp_delay` is specified in milliseconds. The default is 1500 ms. Increase
it if the snapshot is intended to capture an on-screen display which takes
time to appear after a channel change.

The snapshot output path must be writable by the user running `hauppauge2`,
and `ffmpeg` must be available in `PATH`.

----

## Using with MythTV

#### Configuration file

First step is to create an appropriate configuration file:

```
cd /opt/Hauppauge/etc
cp sample.conf hdpvr2-1.conf
nano hdpvr2-1.conf
```

At the minimum, you need to set the serial to the correct value for your
device. You can use the list option to see what devices are detected:

```
/opt/Hauppauge/bin/hauppauge2 --list
```

For a device using HDMI video and S/PDIF audio, the automatic audio
configuration can be used:

```
input=3
audio=1
codec=12
```

For HDMI audio:

```
input=3
audio=3
codec=12
```

This allows the same MythTV External Recorder configuration to capture
either AAC stereo or AC3 Dolby Digital depending on the incoming audio.

Optional one-shot snapshot support can also be enabled in a MythTV External
Recorder configuration:

```ini
mdp=true
mdp_image=/run/hauppauge2/mdp-live.jpg
mdp_delay=3000
```

An external channel-change or automation script can then request a fresh
frame from the already-running MythTV capture without opening the Hauppauge
device a second time.

#### Configure MythTV

Stop mythbackend, and run mythtv-setup to configure the new recorder:

```
systemctl stop mythbackend
mythtv-setup
```

##### 2. Capture Cards

1. Select "New Capture Card"
2. For the "card type", choose "External (blackbox) recorder"
3. For the "file path", use the full path of the hauppauge2 app and
   give it the location of your configuration file. Something like:

```
/opt/Hauppauge/bin/hauppauge2 -c /opt/Hauppauge/etc/hdpvr2-1.conf
```

4. Set the "Tuning timeout" to at least 15000. The HD-PVR2 / Colossus2 can
   take over 5 seconds just to get ready to record. Combine that with the
   time it takes your STB (Set Top Box) to change channels and produce a
   steady output, and it can easily take 15 seconds to "tune" a channel.

##### 3. Recording Profiles

A profile is not used.

##### 4. Video sources

If you don't already have a source for guide information setup, do so now.

##### 5. Input connections

1. Select the External Recorder you created under "2. Capture Cards".
2. Set the "Input name" to "MPEG2TS".
3. Set the "Display name" to whatever you like. E.g. "Colossus2-1".
4. Select the appropriate video source.
5. Set the External channel change command appropriately. This is whatever
   script you have setup to control your STB (set top box).
6. Do **not** "Preset tuner to channel".
7. There is no reason to "Scan for channels". These will be retrieved from
   your video guide provider.
8. If you have not done so already, then "Fetch channels from listing
   source."
9. If you wish, set the "Starting channel".

###### 10. Interactions between inputs

1. If you want to enable "multirec", then set Max recordings to 3.
2. Check "Schedule as group" to enable the faster, optimized scheduler routines.

The rest of the options are optional, set them as you wish.

#### Changing channels

Myth's "External Recorder" is multirec capable, meaning that you can have
overlapping recordings. If a new recording is on the same channel as the
current recording, mythbackend will use the same instance of the hauppauge2
app to retrieve the data. However, the hauppauge2 app does not currently
support changing channels, so you must use MythTV's "External Channel
Change" script capabilities.

Unfortunately, MythTV's "External Channel Change" script mechanism is
**not** multirec aware. This means that mythbackend will call it, even if
hauppauge2 is already recording on the correct channel. If possible, you
should craft your channel change script such that it *just* returns if it is
able to detect that it is already on the correct channel.

#### Logging

When mythbackend invokes this app, it will pass loglevel and logpath
arguments. This app will pay attention to both, but the loglevel can be
overridden in the config file using the override-loglevel option. A good
loglevel for mythbackend is INFO, but that is quite verbose when used with
the Hauppauge "driver". I suggest setting override-loglevel to NOTICE which
results in enough status information to verify that it is working.

##### Log rotation

You will probably want to add log rotation. The log file location will be
the same as the rest of the MythTV logs. The log filenames will look like:

```
hauppauge2-<serial#>.log
```

#### Run it

If everything is configured correctly, you should now be able to restart
mythbackend and have it use this input.

----

## Troubleshooting

After a fresh reboot, the Colossus2 will on occasion drop off the USB bus,
the first time it is used. When hauppauge2 is run, the first thing it sends
out to the log is the Bus and Port of the device. This allows you to reset
the USB bus for that device to get it back.

For example, if the first line in the log is:

```
2018-02-06 15:39:50.822985 C [main] hauppauge2.cpp:266 (main) - Initializing [Bus: 5, Port: 4] E585-00-FFFF4321
```

You can usually get the device back by doing:

```
export BUS=5
echo "0"    | sudo dd of="/sys/bus/usb/devices/usb${BUS}/power/autosuspend_delay_ms"
echo "auto" | sudo dd of="/sys/bus/usb/devices/usb${BUS}/power/control"
echo "on"   | sudo dd of="/sys/bus/usb/devices/usb${BUS}/power/control"
```

After that, the Colossus2 seems to work reliably until the next reboot.
