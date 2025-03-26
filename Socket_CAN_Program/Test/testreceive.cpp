#include <iostream>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>

int main() {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    // Create a socket
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("Socket");
        return 1;
    }

    // Specify the CAN interface
    strcpy(ifr.ifr_name, "can0");
    ioctl(s, SIOCGIFINDEX, &ifr);

    // Bind the socket to the CAN interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        return 1;
    }

    // Set a filter to receive only the frame with ID 0x122
    struct can_filter rfilter;
    rfilter.can_id = 0x122;
    rfilter.can_mask = CAN_SFF_MASK;
    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    while (true) {
        // Read a frame
        int nbytes = read(s, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            perror("Read");
            return 1;
        }

        if (frame.can_id == 0x122) {
            std::cout << "Received frame with ID 0x122" << std::endl;
            std::cout << "Data: ";
            for (int i = 0; i < frame.can_dlc; i++) {
                std::cout << std::hex << (int)frame.data[i] << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }

    // Close the socket
    close(s);
    return 0;
}
