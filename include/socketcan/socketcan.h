//TODO
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int open_can_socket(int *s);
int recv_can_data(int *s, struct can_frame *frame);
int send_can_data(int *s, struct can_frame *frame);