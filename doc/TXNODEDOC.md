# TX Node Walkthrough

The TX node shows users how to create a Heartbeat_1_0 message, package it into a CanardTransfer, push it onto the CanardInstance transfer stack, convert the CanardFrame to a SocketCAN frame, and send it over the bus. Below is a walkthrough of each section of code to give a deeper understanding.

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
#include <pthread.h>
```
We include just three UAVCAN specific headers within the example TX node. The first one, `<uavcan/node/Heartbeat_1_0.h>` is the header for the base UAVCAN heartbeat message that is required for all UAVCAN nodes. The header contains data structures and functions for populating a Heartbeat_1_0 message, serializing and deserializing it, and more. 

As for Linux specific includes, we have many of the standard C libraries as well as SocketCAN specific headers such as `linux/can.h` and `linux/can/raw.h`. Since we implement the TX node as a multi-threaded application, `pthread.h` is included as well. More info on that later in this walkthrough.

## Global vars and function prototypes

```
// Function prototypes
void *process_canard_TX_stack(void* arg);
static void* memAllocate(CanardInstance* const ins, const size_t amount);
static void memFree(CanardInstance* const ins, void* const pointer);
int open_vcan_socket(void);
```

For our function prototypes we have one for processing the CanardInstance transfer stack, one for opening a SocketCAN (vcan0) socket, and two for memory management through o1heap. The memory management functions are taken from the o1heap documentation [here.](https://github.com/pavel-kirienko/o1heap)

```
// Create an o1heap and Canard instance
O1HeapInstance* my_allocator;
volatile CanardInstance ins;

// Transfer ID
static uint8_t my_message_transfer_id = 0;

// Uptime counter for heartbeat message
uint32_t test_uptimeSec = 0;

// Buffer for serialization of a heartbeat message
size_t hbeat_ser_buf_size = uavcan_node_Heartbeat_1_0_EXTENT_BYTES_;
uint8_t hbeat_ser_buf[uavcan_node_Heartbeat_1_0_EXTENT_BYTES_];

// vcan0 socket descriptor
int s;
int open_vcan_socket(void);
```

The rest of the code here before the main function is just global variables for use within each thread. We instantiate an O1HeapInstance for memory allocation, global vars for message transfer ID, an uptime counter, a heartbeat serialization buffer, and a SocketCAN descriptor for our vcan0 bus.

## Setup within main()

```
int main(void)
{
	// Allocate 4KB of memory for o1heap.
    void *mem_space = malloc(4096);
    
    // Initialize o1heap
    my_allocator = o1heapInit(mem_space, (size_t)4096, NULL, NULL);
    
    printf("Address for s (main): %ls\n", &s);
    int sock_ret = open_vcan_socket();

    // Make sure our socket opens successfully.
    if(sock_ret < 0)
    {
        perror("Socket open");
        return -1;
    }
    
    // Initialize canard as CANFD and node no. 96
    ins = canardInit(&memAllocate, &memFree);
    ins.mtu_bytes = CANARD_MTU_CAN_FD;
    ins.node_id = 96;

    // Initialize thread for processing TX queue
    pthread_t thread_id;
    int exit_thread = 0;
    pthread_create(&thread_id, NULL, &process_canard_TX_stack, (void*)&exit_thread);
```

Within main(), we need to do some setup before actually sending UAVCAN messages. 

First, we need to allocate memory using o1heap for our `CanardInstance`. To do this, we allocate 4KB of memory (a typical amount of memory you'll find on a resource-constrained system like a Cortex-M0 microcontroller) by using `malloc()`. We store a pointer to this memory in a `void*` called mem_space. Then, we call `o1heapInit()` and pass `mem_space`, the size of our allocated memory block as a `size_t` (in this case, 4096), and NULL for the last two parameters. The last two parameters are `critical_section_enter` and `critical_section_leave` of type `O1HeapHook`. We don't need to worry about those arguments for this simple application.

Second, we open our `vcan0` socket using `open_vcan_socket()`. The contents of this function are a basic example of opening a socket with SocketCAN, and you can find more info from this webpage: https://www.beyondlogic.org/example-c-socketcan-code/

Third, we need to initialize our CanardInstance by running calling `canardInit()` and passing our `memAllocate` and `memFree` functions. These functions are example functions provided in the o1heap documentation. We then pass data to the `.mtu_bytes` and `node_id` fields of our `CanardInstance`. In this case, we specify we are using CAN-FD and that our node id will be 96. We chose 96 as an arbitrary number because we will be broadcasting our Heartbeat_1_0 message to all nodes (in this case, we have only one other node in the system). More on that later in this walkthrough.

Finally, we create a new thread for our `process_canard_TX_stack` function to run in the background. This allows the `CanardInstance` transfer stack to send raw CAN messages without blocking our main function, which would cause a delay in transfer, resulting in our RX node receiving messages late. This thread is spawned using a `pthread` which stands for "POSIX thread". You can learn about POSIX threads [here](https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html).

## Main control loop
```
    // Main control loop. Run until break condition is found.
    for(;;)
    {
        // Sleep for 1 second so our uptime increments once every second.
        sleep(1);

        // Initialize a Heartbeat message
        uavcan_node_Heartbeat_1_0 test_heartbeat = {
            .uptime = test_uptimeSec,
            .health = { uavcan_node_Health_1_0_NOMINAL },
            .mode = { uavcan_node_Mode_1_0_OPERATIONAL }
        };

        // Print data from Heartbeat message before it's serialized.
        system("clear");
        printf("Preparing to send the following Heartbeat message: \n");
        printf("Uptime: %d\n", test_uptimeSec);
        printf("Health: %d\n", uavcan_node_Health_1_0_NOMINAL);
        printf("Mode: %d\n", uavcan_node_Mode_1_0_OPERATIONAL);

        // Serialize the data using the included serialize function from the Heartbeat C header.
        int8_t result1 = uavcan_node_Heartbeat_1_0_serialize_(&test_heartbeat, hbeat_ser_buf, &hbeat_ser_buf_size);
```

The main thread runs a loop that performs the necessary steps to push a `CanardTransfer` onto the transfer stack. First, we create a Heartbeat_1_0 message and pass data to the uptime, health, and mode fields. Next, we print the data that we passed to that data structure, and then we serialize the message by placing it into a data buffer. 

```
        // Create a CanardTransfer and give it the required data.
        const CanardTransfer transfer = {
            .timestamp_usec = time(NULL),
            .priority = CanardPriorityNominal,
            .transfer_kind = CanardTransferKindMessage,
            .port_id = uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_,
            .remote_node_id = CANARD_NODE_ID_UNSET,
            .transfer_id = my_message_transfer_id,
            .payload_size = hbeat_ser_buf_size,
            .payload = hbeat_ser_buf,
        };
          
        // Increment our uptime and transfer ID.
        ++test_uptimeSec;
        ++my_message_transfer_id;

        // Stop the loop once we hit 30s of transfer.
        if(test_uptimeSec > 31)
        {
            printf("Reached 30s uptime! Exiting...\n");
            exit_thread = 1;
            break;
        }
        
        // Push our CanardTransfer to the Libcanard instance's transfer stack.
        int32_t result2 = canardTxPush((CanardInstance* const)&ins, &transfer);
        
        // Make sure our push onto the stack was successful.
        if(result2 < 0)
        {
            printf("Pushing onto TX stack failed. Aborting...\n");
            break;
        }
```

Next we create a `CanardTransfer`. The data provided in this example is typical for a simple publisher. Note that we use Linux system time for the timestamp and we use the serialized heartbeat buffer for the payload. Once our `CanardTransfer` is configured we push it on the transfer stack for processing by our background thread.

## Background TX stack process thread

```
/* Function to process the Libcanard TX stack, package into a SocketCAN frame, and send on the bus. */
void *process_canard_TX_stack(void* arg)
{
    printf("Entered thread.\n");
    for(;;)
    {
        // Run every 5ms to prevent using too much CPU.
        usleep(5000);

        // Check to see if main thread has asked this thread to stop.
        int* exit_thread = (int*)arg;

        // Check to make sure there's no frames in the transfer stack, then check to see if this thread should exit.
        // If so, exit the thread.
        if(canardTxPeek((CanardInstance* const)&ins) == NULL && *exit_thread)
        {
            printf("Exiting thread.\n");
            return;
        }

        // Loop through all of the frames in the transfer stack.
        for(const CanardFrame* txf = NULL; (txf = canardTxPeek((CanardInstance* const)&ins)) != NULL;)
        {
            // Make sure we aren't sending a message before the actual time.
            if(txf->timestamp_usec < (unsigned long)time(NULL))
            {
                // Instantiate a SocketCAN CAN frame.
                struct can_frame frame;

                // Give payload size.
                frame.can_dlc = txf->payload_size;
                
                // Give extended can id.
                frame.can_id = txf->extended_can_id;
                
                // Copy transfer payload to SocketCAN frame.
                memcpy(&frame.data[0], txf->payload, txf->payload_size);
                
                // Print RAW can data.
                printf("0x%03X [%d] ",frame.can_id, frame.can_dlc);
                for (uint8_t i = 0; i < frame.can_dlc; i++)
                        printf("%02X ",frame.data[i]);
                printf(" Sent!\n\n");
                    
                // Send CAN Frame.
                if(write(s, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame))
                {
                    perror("Write");
                    return;
                }
                
                // Pop the sent data off the stack and free its memory.
                canardTxPop((CanardInstance* const)&ins);
                ins.memory_free((CanardInstance* const)&ins, (CanardFrame*)txf);
            }
        }
    }
}
```

This function runs in the background as a separate thread to process all CAN frames that need to be sent from the transfer stack. First we check to make sure that there are no frames currently on the stack and that our exit flag is false before moving forward, otherwise we exit the thread. Then, we loop through each frame within the stack, check to make sure the timestamp is not from the future, and then copy data from the `CanardFrame` structure to the `can_frame` (SocketCAN) structure. Then, we send the CAN message on it's way!

That is all for the transmit side! If you have any questions, please contact me at landon.haugh@nxp.com.