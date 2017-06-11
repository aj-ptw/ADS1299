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

  typedef enum SAMPLE_RATE {
    SAMPLE_RATE_16000,
    SAMPLE_RATE_8000,
    SAMPLE_RATE_4000,
    SAMPLE_RATE_2000,
    SAMPLE_RATE_1000,
    SAMPLE_RATE_500,
    SAMPLE_RATE_250
  };

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
  void activateChannel(byte N);
  void changeChannelLeadOffDetect();
  void changeChannelLeadOffDetect(byte N);
  void deactivateChannel(byte N);
  byte getDeviceID();
  void initialize_ads();
  void printRegisters();
  void resetADS();
  byte RREG(byte);
  void RREGS(byte, byte);
  void writeChannelSettings();

  void printRegisterName(byte _address);

  void WREG(byte _address, byte _value); //

  void updateData();

  //SPI Arduino Library Stuff
  boolean boardUseSRB1;             // used to keep track of if we are using SRB1
  boolean streaming;
  boolean useInBias[NUMBER_OF_CHANNELS];        // used to remember if we were included in Bias before channel power down
  boolean useSRB2[NUMBER_OF_CHANNELS];
  boolean verbose;
  byte channelSettings[NUMBER_OF_CHANNELS][NUMBER_OF_CHANNEL_SETTINGS];  // array to hold current channel settings
  byte defaultChannelSettings[NUMBER_OF_CHANNEL_SETTINGS];  // default channel settings
  byte leadOffSettings[NUMBER_OF_CHANNELS][NUMBER_OF_LEAD_OFF_SETTINGS];  // used to control on/off of impedance measure for P and N side of each channel
  byte sampleCounter;
  int numChannels;
  SAMPLE_RATE curSampleRate;

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
