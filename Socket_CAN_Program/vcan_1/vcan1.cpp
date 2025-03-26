#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "../include/TripDetails.h"

#ifndef _INIT_SOCKET_CAN_
#define _INIT_SOCKET_CAN_

int initCANSocket(const char* interface) {
    int s;
    struct sockaddr_can addr;  //sockaddr_can specify the address details when binding the socket
    struct ifreq ifr; //ifreq structure used to control interface settings or name

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) 
    {
        perror("Socket is not created -------");
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
        perror(" unable to Bind the socket address-");
        return -1;
    }
    //Binds the socket s with addess


    return s; //returns the open socket instance with the specific interface and address.
}
#endif





void transmitTripData1(int socket) {
    struct can_frame frame; //can_frame used for transmitting data over a CAN bus
    frame.can_id = 0x130;  // Sets the CAN Identifier (can_id) of the frame to '0x123' .This identifier used to determine the prority and messafe content on the CAN bus
    frame.can_dlc = 8; // / Sets the Data length code of the frame 8 bytes. It means 8 bytes of data will be sent in a single frame

    //while (true) {
       // int randomData = generateRandomInteger();
        TripDetails td;
        // td.Trip_Total_Distance=120;
         td.Trip_Driving_Score=100;
        // td.Trip_End_Latitude=32567;
        // td.Trip_End_Longitude=23456;
        // td.Trip_Average_Speed=56;
        td.Trip_Top_Speed=123;
       // td.Trip_start_epoch_time=localtime(&now);
       // td.Trip_end_epoch_time=localtime(&now);
        std::memcpy(frame.data, &td, sizeof(td));// Copies the generated random integer('randomData') into the 'data' field of the 'frame'. The 'data' field in 'can_frame' typically holds the actual data payload to be transmitted over the CAN bus.

        if (write(socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
            perror("Cannot Write the data in CAN structure because of Structure mismatch");
        } else {
            std::cout << "\n VCAN1 Transmitted  data to CAN0 " << std::endl;
        }/**
        Writes the entire contents of 'frame'(which is of type 'can_frame') to the CAN socket specified by 'socket'. It sends the frame of data over the CAN bus.
         */

       // std::this_thread::sleep_for(std::chrono::seconds(1));  // 2 sec difference between data transmitted on CAN bus
   // }
}


int vcan1()
{

    const char* canInterface="can0";
    int s=initCANSocket(canInterface);


    if(s<0)
    {
        perror("Couldn't initialize the Socket ");
        return -1;
    }

    transmitTripData1(s);

    close(s);


return 0;

}


