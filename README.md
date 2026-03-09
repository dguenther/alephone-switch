# Aleph One (Switch)

Aleph One is the open source continuation of Bungie™’s _Marathon® 2_ and _Marathon Infinity_ game engines. Aleph One plays _Marathon_, _Marathon 2_, _Marathon Infinity_, and third-party content on a variety of platforms.

Aleph One is available under the terms of the [GNU General Public License (GPL 3)](http://www.gnu.org/licenses/gpl-3.0.html)

This fork is an *unofficial* port of Aleph One to the Switch. For officially supported platforms, including versions for macOS, Windows, and Linux Flatpak, visit
[alephone.lhowon.org](https://alephone.lhowon.org) 

## Installation

1. Download a release from the Releases list.
1. Copy the .nro to `/switch` (on your SD card).
1. Download a scenario ([available here](https://alephone.lhowon.org/scenarios.html)) and extract it to `/switch/alephone`

## Features

* OpenGL renderer (4.3 Core wrapper)
* LAN multiplayer. Tested crossplay between Switch and PC
* Controller support, keyboard/mouse support
* Resolution change on dock/undock

### Needs Testing

* Community-made scenarios 

### Not Implemented
* Film recording
* Internet multiplayer
* Touch support
* Gyro controls

## Build from source

Builds were created using `podman` and the Containerfile in the `switch` folder. You can adapt the `switch-` scripts in the `scripts` folder to your development environment.

If you have `podman` installed, you can:
1. Run `./scripts/switch-container-build.sh` to build an .nro
1. Run `./scripts/switch-deploy.sh 192.168.xxx.xxx` to deploy the .nro over network via nxlink

## CI status

[![Build Status](https://github.com/Aleph-One-Marathon/alephone/actions/workflows/ci-build.yml/badge.svg)](https://github.com/Aleph-One-Marathon/alephone/actions/workflows/ci-build.yml?query=branch%3Amaster+)

## Credits

Thanks to the Aleph One team, please support them at [alephone.lhowon.org](https://alephone.lhowon.org).

Portions of this port (notably OpenGL and networking) were done using AI assistance.