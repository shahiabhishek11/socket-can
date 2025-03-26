#include<iostream>
#include<thread>
#include "../vcan_0/vcan0.cpp"
#include "../vcan_1/vcan1.cpp"




        int main()
        {
         
            std::thread th1(vcan0);
            std::thread th2(vcan1);

            th1.join();
            th2.join();

            return 0;
        }