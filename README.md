# socketcan_canard

## Overview

This repo is based off of Abraham Rodriguez's (abraham_rodriguez@nxp.com) libcanard_S32K1 example. You can find his code here: https://github.com/noxuz/libcanard_S32K1/

Disclaimer: There may be incomplete or incorrect information in this repository. This project was done mainly to help both myself and others learn about UAVCAN and gain a better understanding of it. If there is anything terribly wrong with documentation or code in this repo, please create an issue or PR so I can fix my misconceptions! Thanks :)

Socketcan_canard is a basic implementation of Libcanard in Linux using a virtual CAN bus (vcan0) with SocketCAN. The source code contains two main source files: test_canard_tx.c and test_canard_rx.c. The tx file packages a Heartbeat_1_0 message and sends it over the virtual CAN bus. The rx file receives this file and prints out the Heartbeat_1_0 information.

![alt text](doc/demo_screenshot.png)

## Prerequisities

You'll want to be running a Linux distribution with SocketCAN support, which is pretty much any Linux distro these days.

Clone the repo by running:

```$ git clone https://github.com/landonh12/socketcan_canard```

## Setup

First, you'll want to set up the vcan0 SocketCAN bus. You can do so by running the start_vcan.sh script in the scripts/ directory:

```$ ./scripts/start_vcan.sh```

Next you'll want to build the binaries for the RX and TX nodes. You can do so by running `make`. The binaries will be stored in the `bin/` directory within the root of the project.

## Running the project

To run the project, open up two separate terminals on your Linux machine and run the TX and RX nodes in each:

Term 1:
```$ ./bin/test_canard_tx```

Term 2:
```$ ./bin/test_canard_rx```

The TX node will print the RAW can frame sent over the `vcan0` bus, while the RX node will print Uptime, Health, and Mode fields. The Health and Mode fields should stay 0 while the uptime increments by 1 every second.

# Code documentation

You can find documentation for both the TX and RX nodes in the `doc/` folder. Or, just click [TX](doc/TXNODEDOC.md) or [RX](doc/RXNODEDOC.md).

### Copyright© 2021, NXP. All rights reserved.
