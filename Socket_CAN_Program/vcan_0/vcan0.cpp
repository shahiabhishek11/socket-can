#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/ioctl.h>


#ifndef _INIT_SOCKET_CAN_
#define  _INIT_SOCKET_CAN_
// Function to initialize CAN socket
int initCANSocket(const char* interface) {
    int s;
    struct sockaddr_can addr;  //sockaddr_can specify the address details when binding the socket
    struct ifreq ifr; //ifreq structure used to control interface settings or name

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) 
    {
        perror("Socket------VCAN0 is not created -------");
        return -1;
    }
    /***
     * Socket function opens the CAN socket
     * 1)PF_CAN :  specifies the protoco; family for CAN Sockets
     * 2)SOCK_RAW : indicates that this is a raw socket providing access to lower level protocols
     * 3)CAN_RAW8 : specifies the raw CAN protocol
     * 
     * if it returns the negative value means it is not able to open the Linux Socket
     */

    strcpy(ifr.ifr_name, interface); //simply coping the interface name vcan0 in Socket Interfcae name
    ioctl(s, SIOCGIFINDEX, &ifr);
    /**
    IOCTL performs an I/O control operation on socket s
    SIOCGIFINDEX is an ioctl command used for call the interface index combined with ifr.ifr_name. The index is stored in 'ifr.ifrindex'  */

    addr.can_family = AF_CAN;// assign address fmaily of 'addr' to AF_CAN, which is used for CAN bus sockets.
    addr.can_ifindex = ifr.ifr_ifindex; //Sets the interface index of 'addr' to the index retrived earlier from 'ifr'

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror(" unable to Bind the socket address---------VCAN0-----");
        return -1;
    }
    //Binds the socket s with addess


    return s; //returns the open socket instance with the specific interface and address.
}

#endif





int receiveData1(int s) {

  struct can_frame frame;
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
    



    return 0;
}






int vcan0()
{
    
    const char* canInterface = "can0";
   // std::thread th1(initCANSocket,canInterface);
    int canSocket = initCANSocket(canInterface);
    if (canSocket < 0) {
        std::cerr << "Failed to initialize CAN socket" << std::endl;
        return -1;
    }
    std::cout<<"\n Before the receive Data \n";
     receiveData1(canSocket);
    std::cout<<"\n After the receive Data \n";

    close(canSocket);
    return 0;

}







