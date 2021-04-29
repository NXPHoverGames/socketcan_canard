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

// Heap info
//extern int __HeapBase[];
//extern int HEAP_SIZE[];

// Function prototypes
void process_canard_TX_queue(void);

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
void open_vcan_socket(void);

int main(void)
{
	
    void *mem_space = malloc(4096);
    // Initialize o1heap
    // __HeapBase and HEAP_SIZE are from the GNU C Compiler linker files for S32K146.
    // HEAP_SIZE is defined as 0x00000400. __HeapBase is defined as '.' (?)
    my_allocator = o1heapInit(mem_space, (size_t)4096, NULL, NULL);
    
    printf("Address for s (main): %ls\n", &s);
    open_vcan_socket();
    
    // Initialize canard as CANFD and node no. 96
    ins = canardInit(&memAllocate, &memFree);
    ins.mtu_bytes = CANARD_MTU_CAN_FD;
    ins.node_id = 96;
    
    for(;;)
    {
        // Initialize a Heartbeat message
        uavcan_node_Heartbeat_1_0 test_heartbeat = {
            .uptime = test_uptimeSec,
            .health = { uavcan_node_Health_1_0_NOMINAL },
            .mode = { uavcan_node_Mode_1_0_OPERATIONAL }
        };

        int8_t result1 = uavcan_node_Heartbeat_1_0_serialize_(&test_heartbeat, hbeat_ser_buf, &hbeat_ser_buf_size);
        
        if(result1 < 0)
        {
            printf("Serializing message failed. Aborting...\n");
            abort();
        } // end if
        
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
          
        ++test_uptimeSec;
        ++my_message_transfer_id;
        
        int32_t result2 = canardTxPush((CanardInstance* const)&ins, &transfer);
        
        if(result2 < 0)
        {
            printf("Pushing onto TX stack failed. Aborting...\n");
            abort();
        }
        
        //sleep(1);
        
        process_canard_TX_queue();
    }
}

void open_vcan_socket(void)
{
    printf("Address for s (open): %ls\n", &s);
    if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
        perror("Socket");
        return;
    }
    
    struct ifreq ifr;
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    
    struct sockaddr_can addr;
    
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind");
        return;
    }
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

void process_canard_TX_queue(void)
{
    for(const CanardFrame* txf = NULL; (txf = canardTxPeek((CanardInstance* const)&ins)) != NULL;)
    {
        if((0U == txf->timestamp_usec) || (txf->timestamp_usec < (unsigned long)time(NULL)))
        {
            // Instantiate a SocketCAN CAN frame
            struct can_frame frame;

            // Give payload size
            frame.can_dlc = txf->payload_size;
            
	    // Give extended can id
            frame.can_id = txf->extended_can_id;
            
	    // Copy transfer payload to SocketCAN frame
            memcpy(&frame.data[0], txf->payload, txf->payload_size);
            
	    // Print RAW can data
	    printf("0x%03X [%d] ",frame.can_id, frame.can_dlc);
	    for (uint8_t i = 0; i < frame.can_dlc; i++)
                printf("%02X ",frame.data[i]);
	    printf("\n");
            
	    // Send CAN Frame
	    if(write(s, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame))
            {
                perror("Write");
                return;
            }
            
	    // Pop the sent data off the stack
            canardTxPop((CanardInstance* const)&ins);
            ins.memory_free((CanardInstance* const)&ins, (CanardFrame*)txf);
        }
    }
}



