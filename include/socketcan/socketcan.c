//TODO
#include "socketcan.h"

/* Send CAN data on the bus
 * s: pointer to socket descriptor
 * frame: pointer to SocketCAN frame struct
 */
int send_can_data(int *s, struct can_frame *frame)
{
    if(write(*s, frame, sizeof(struct can_frame)) != sizeof(struct can_frame))
    {
        perror("Write");
        return -1;
    }
    return 0;
}

/* Receive CAN data on the bus
 * s: pointer to socket descriptor
 * frame: pointer to SocketCAN frame struct
 */
int recv_can_data(int *s, struct can_frame *frame)
{
    if(read(*s, frame, sizeof(struct can_frame)) < 0)
    {
        perror("Read");
        return -1;
    }
    return 0;
}

/* Open our SocketCAN socket (vcan0)
 * s: pointer to socket descriptor
 */
int open_can_socket(int *s)
{
    // Open a RAW CAN socket.
    if((*s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
        perror("Socket");
        return -1;
    }

    // Construct an if request for vcan0 socket.
    struct ifreq ifr;
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(*s, SIOCGIFINDEX, &ifr);

    // Create a socket address field for binding.
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = PF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // Bind the socket.
    if(bind(*s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind");
        return -1;
    }

    return 0;
}
