//
//  ADS1299.h
//
//  Created by Conor Russomanno on 6/17/13.
//

#ifndef ____ADS1299__
#define ____ADS1299__

#include <Arduino.h>
#include "Definitions.h"
#include <SPI.h>


class ADS1299 {
public:

    void setup(int, int);
    void setup(int, int, boolean);

    //ADS1299 SPI Command Definitions (Datasheet, Pg. 35)
    //System Commands
    void WAKEUP();
    void STANDBY();
    void RESET();
    void START();
    void STOP();

    //Data Read Commands
    void RDATAC();
    void SDATAC();
    void RDATA();

    //Register Read/Write Commands
    byte getDeviceID();
    void printRegisters();
    byte RREG(byte);
    void RREGS(byte, byte);

    void printRegisterName(byte _address);

    void WREG(byte _address, byte _value); //

    void updateData();

    //SPI Arduino Library Stuff
    boolean verbose;

    byte channelDataRaw[NUMBER_BYTES_PER_SAMPLE];    // array to hold raw channel data
    byte regData[24]; // array is used to mirror register data
    byte xfer(byte _data);

    // debug stuff
    uint8_t counter;
    uint8_t serialPrintInterval;
    void printHex(byte _data);

    unsigned long lastSerialPrint;

    //------------------------//
    void attachInterrupt();
    void detachInterrupt(); // Default
    void begin(); // Default
    void end();
    void setBitOrder(uint8_t);
    void setDataMode(uint8_t);
    void setClockDivider(uint8_t);
    //------------------------//

    float tCLK;
    int DRDY, CS; //pin numbers for "Data Ready" (DRDY) and "Chip Select" CS (Datasheet, pg. 26)

    int outputCount;

//    vector<String> registers;

};

#endif
