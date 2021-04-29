/*
 * Copyright (c) 2020, NXP. All rights reserved.
 * Distributed under The MIT License.
 * Author: Abraham Rodriguez <abraham.rodriguez@nxp.com>
 * 		   & Landon Haugh <landon.haugh@nxp.com>
 *
 * Description:
 *
 * Transmits an UAVCAN Heartbeat message over a virtual SocketCAN bus.
 *
 */

#include <uavcan/node/Heartbeat_1_0.h>
#include <libcanard/canard.h>
#include <o1heap/o1heap.h>
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

// Function prototypes
void *process_canard_TX_stack(void* arg);

// Create an o1heap and Canard instance
O1HeapInstance* my_allocator;
volatile CanardInstance ins;

// Function prototypes for allocating memory to CanardInstance
static void* memAllocate(CanardInstance* const ins, const size_t amount);
static void memFree(CanardInstance* const ins, void* const pointer);

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
        
        // Make sure the serialization was successful.
        if(result1 < 0)
        {
            printf("Serializing message failed. Aborting...\n");
            break;
        }
        
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
    }

    // Main control loop exited. Wait for our spawned process thread to exit and then free our allocated memory space for o1heap.
    pthread_join(thread_id, NULL);
    free(mem_space);
    return 0;
}

/* Open our SocketCAN socket (vcan0) */
int open_vcan_socket(void)
{
    // Open a RAW CAN socket.
    printf("Address for s (open): %ls\n", &s);
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

/* Standard memAllocate and memFree from o1heap examples. */
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



