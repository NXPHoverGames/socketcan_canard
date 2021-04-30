/*
 * Copyright (c) 2020, NXP. All rights reserved.
 * Distributed under The MIT License.
 * Author: Abraham Rodriguez <abraham.rodriguez@nxp.com>
 * 		   & Landon Haugh <landon.haugh@nxp.com>
 *
 * Description:
 *
 * Receives an UAVCAN Heartbeat message over a virtual SocketCAN bus.
 *
 */

// UAVCAN specific includes
#include "uavcan/node/Heartbeat_1_0.h"
#include "libcanard/canard.h"
#include "o1heap/o1heap.h"

// Linux specific includes
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

// Function prototypes for allocating memory to CanardInstance
static void* memAllocate(CanardInstance* const ins, const size_t amount);
static void memFree(CanardInstance* const ins, void* const pointer);
int open_vcan_socket(void);

// Create an o1heap and Canard instance
O1HeapInstance* my_allocator;
CanardInstance ins;

// vcan0 socket descriptor
int s;

int main(void) {

	void *mem_space = malloc(4096);
	// Initialization of o1heap allocator for libcanard, requires 16-byte alignment, view linker file
	my_allocator = o1heapInit(mem_space, (size_t)4096, NULL, NULL);

	int sock_ret = open_vcan_socket();

	if(sock_ret < 0)
	{
		perror("Socket open");
		return -1;
	}

	// Initialization of a canard instance with the previous allocator
	ins = canardInit(&memAllocate, &memFree);
	ins.mtu_bytes = CANARD_MTU_CAN_FD;
	ins.node_id = 97;

	// Subscribe to heartbeat messages within libCanard
	CanardRxSubscription heartbeat_subscription;

	(void) canardRxSubscribe(&ins,
							 CanardTransferKindMessage,
							 uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_,
							 uavcan_node_Heartbeat_1_0_EXTENT_BYTES_,
							 CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
							 &heartbeat_subscription);
							 
	// SocketCAN specifics
    int nbytes;
    struct can_frame socketcan_frame;

    // CanardFrame for reception
    CanardFrame received_canard_frame;

	// Block waiting for reception interrupts to happen
	for(;;)
	{
	    // Read from CAN bus (this is a blocking call!)
		nbytes = read(s, &socketcan_frame, sizeof(struct can_frame));
		
		// If nbytes < 0, nothing came through, so return.
		// Technically right now this is impossible because the read() function is blocking.
		if(nbytes < 0)
		{
		    perror("Read");
		    return 1;
		}
		
		// Transfer all of the data from the CAN frame to a canard frame
		received_canard_frame.extended_can_id = socketcan_frame.can_id;
		received_canard_frame.payload_size = socketcan_frame.can_dlc;
		received_canard_frame.timestamp_usec = time(NULL);
		received_canard_frame.payload = socketcan_frame.data;

		// Create a CanardTransfer and accept the data
		CanardTransfer transfer;
		const int8_t res1 = canardRxAccept(&ins,
											 &received_canard_frame,
											 0,
											 &transfer);
		
		// If for some reason Libcanard doesn't like that frame, break from the loop	
		if(res1 < 0)
		{
		    printf("Fatal error, exiting\n"); 
		    break; 
		} // Error occurred

		else if(res1 == 1) // A transfer was completed
		{
			// Instantiate a heartbeat message
			uavcan_node_Heartbeat_1_0 RX_hbeat;
			size_t hbeat_ser_buf_size = uavcan_node_Heartbeat_1_0_EXTENT_BYTES_;

			// De-serialize the heartbeat message
			int8_t res2 = uavcan_node_Heartbeat_1_0_deserialize_(&RX_hbeat, transfer.payload, &hbeat_ser_buf_size);

			if(res2 < 0)
			{  
				printf("Error occurred deserializing data. Exiting...\n");
				break;
			} // Error occurred
			
            system("clear");
			printf("Uptime: %d\n", RX_hbeat.uptime);
			printf("Health: %d\n", RX_hbeat.health);
			printf("Mode: %d\n\n", RX_hbeat.mode);

			// Deallocation of memory
			ins.memory_free(&ins, (void*)transfer.payload);

		}
		else
		{
			// The received frame is not the last from a multi-frame transfer
		}
	}

	return 0;
}

/* Open our SocketCAN socket (vcan0) */
int open_vcan_socket(void)
{
    // Open a RAW CAN socket.
    if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
        perror("Socket");
        return -1;
    }
    
    // Construct an if request for vcan0 socket.
    struct ifreq ifr;
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    
    // Create a socket address field for binding.
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    // Bind the socket.
    if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind");
        return -1;
    }

    return 0;
}

static void* memAllocate(CanardInstance* const ins, const size_t amount)
{
    (void) ins;
    return o1heapAllocate(my_allocator, amount);
}

static void memFree(CanardInstance* const ins, void* const pointer)
{
    (void) ins;
    o1heapFree(my_allocator, pointer); 
}


