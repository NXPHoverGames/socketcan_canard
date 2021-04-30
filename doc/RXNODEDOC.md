# RX Node Walkthrough

The RX node shows users how to receive a SocketCAN frame, unpack it into a `CanardFrame`, deserialize it, and print it to `stdout`.

### Includes

```
// UAVCAN specific includes
#include <uavcan/node/Heartbeat_1_0.h>
#include <libcanard/canard.h>
#include <o1heap/o1heap.h>

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
```
We include just three UAVCAN specific headers within the example TX node. The first one, `<uavcan/node/Heartbeat_1_0.h>` is the header for the base UAVCAN heartbeat message that is required for all UAVCAN nodes. The header contains data structures and functions for populating a Heartbeat_1_0 message, serializing and deserializing it, and more. 

As for Linux specific includes, we have many of the standard C libraries as well as SocketCAN specific headers such as `linux/can.h` and `linux/can/raw.h`.

## Global vars and function prototypes

```
// Function prototypes
static void* memAllocate(CanardInstance* const ins, const size_t amount);
static void memFree(CanardInstance* const ins, void* const pointer);
int open_vcan_socket(void);
```

For our function prototypes we have one for opening a SocketCAN (vcan0) socket, and two for memory management through o1heap. The memory management functions are taken from the o1heap documentation [here.](https://github.com/pavel-kirienko/o1heap)

```
// Create an o1heap and Canard instance
O1HeapInstance* my_allocator;
volatile CanardInstance ins;

// vcan0 socket descriptor
int s;
```

The rest of the code here before the main function is just global variables for use within the program. We instantiate an `O1HeapInstance` for memory allocation, a `CanardInstance`, and a SocketCAN descriptor for our vcan0 bus.

## Setup within main()

```
int main(void) {

	void *mem_space = malloc(4096);
	// Initialization of o1heap allocator for libcanard, requires 16-byte alignment, view linker file
	my_allocator = o1heapInit(mem_space, (size_t)4096, NULL, NULL);
```

Within main(), we need to do some setup before receiving UAVCAN messages.

We need to allocate memory using o1heap for our `CanardInstance`. To do this, we allocate 4KB of memory (a typical amount of memory you'll find on a resource-constrained system like a Cortex-M0 microcontroller) by using `malloc()`. We store a pointer to this memory in a `void*` called mem_space. Then, we call `o1heapInit()` and pass `mem_space`, the size of our allocated memory block as a `size_t` (in this case, 4096), and NULL for the last two parameters. The last two parameters are `critical_section_enter` and `critical_section_leave` of type `O1HeapHook`. We don't need to worry about those arguments for this simple application.

```
	int sock_ret = open_vcan_socket();

	if(sock_ret < 0)
	{
		perror("Socket open");
		return -1;
	}
```

We open our `vcan0` socket using `open_vcan_socket()`. The contents of this function are a basic example of opening a socket with SocketCAN, and you can find more info from this webpage: https://www.beyondlogic.org/example-c-socketcan-code/

```
	// Initialization of a canard instance with the previous allocator
	ins = canardInit(&memAllocate, &memFree);
	ins.mtu_bytes = CANARD_MTU_CAN_FD;
	ins.node_id = 97;
```

We need to initialize our CanardInstance by running calling `canardInit()` and passing our `memAllocate` and `memFree` functions. These functions are example functions provided in the o1heap documentation. We then pass data to the `.mtu_bytes` and `node_id` fields of our `CanardInstance`. In this case, we specify we are using CAN-FD and that our node id will be 97. We chose 97 as an arbitrary number, as the TX node does not specify a node id for sending. This means that the Heartbeat_1_0 message will be sent to all nodes, making our node id for the RX node unimportant.

```
	// Subscribe to heartbeat messages within libCanard
	CanardRxSubscription heartbeat_subscription;

	(void) canardRxSubscribe(&ins,
							 CanardTransferKindMessage,
							 uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_,
							 uavcan_node_Heartbeat_1_0_EXTENT_BYTES_,
							 CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
							 &heartbeat_subscription);
```

We create a `CanardRXSubscription` to receive our heartbeat message. We specify that the transfer kind is `CanardTransferKindMessage` as the TX node is sending a message, not a service. 

```					 
	// SocketCAN specifics
    int nbytes;
    struct can_frame socketcan_frame;
    
    // CanardFrame for reception
    CanardFrame received_canard_frame;
```

Finally, we create our frame structures for both SocketCAN and Libcanard.


## Main control loop

```
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
```

In the main control loop, we first read from our CAN socket and check to make sure we actually received data.

```
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
```

Next, we copy all of the data from the SocketCAN frame to the `CanardFrame` so we can ask Libcanard to accept the data. If there's an issue with the data, the loop will break and we will exit the program.

```
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
```

If the frame is accepted, we create an instance of a `Heartbeat_1_0` message and deserialize it. Once it is deserialized, we can access the `Heartbeat_1_0` data structure to extract our data and print it to the bus. After that, we just need to free the memory from our `CanardTransfer` payload. 

That is all for the receive side! If you have any questions, please contact me at landon.haugh@nxp.com.