# socketcan_canard

This repo is based off of Abraham Rodriguez's (abraham_rodriguez@nxp.com) libcanard_S32K1 example. You can find his code here: https://github.com/noxuz/libcanard_S32K1/

socketcan_canard is a basic implementation of Libcanard in Linux using a virtual CAN bus (vcan0) with SocketCAN. The source code contains two main source files: test_canard_tx.c and test_canard_rx.c. The tx file packages a Heartbeat_1_0 message and sends it over the virtual CAN bus. The rx file receives this file and prints out the Heartbeat_1_0 information.



### Copyright© 2021, NXP. All rights reserved.
