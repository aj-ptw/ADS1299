//
//  ADS1299.cpp
//
//  Created by Conor Russomanno on 6/17/13.
//


#include "ADS1299.h"

void ADS1299::setup(int _DRDY, int _CS){
  setup(_DRDY, _CS, false);
}

void ADS1299::setup(int _DRDY, int _CS, boolean _verbose){

  // **** ----- SPI Setup ----- **** //

  // Set direction register for SCK and MOSI pin.
  // MISO pin automatically overrides to INPUT.
  // When the SS pin is set as OUTPUT, it can be used as
  // a general purpose output port (it doesn't influence
  // SPI operations).
  //
  verbose = _verbose;

  Serial.println("setup");

  SPI.begin();

  pinMode(SCK, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(SS, OUTPUT);

  digitalWrite(SCK, LOW);
  digitalWrite(MOSI, LOW);
  digitalWrite(SS, HIGH);

  SPI.setDataMode(SPI_MODE1);
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.setBitOrder(MSBFIRST);

  // **** ----- End of SPI Setup ----- **** //

  // initalize the  data ready and chip select pins:
  DRDY = _DRDY;
  CS = _CS;
  pinMode(CS, OUTPUT);

  tCLK = 0.000666; //666 ns (Datasheet, pg. 8)
  outputCount = 0;

  lastSerialPrint = 0;
  counter = 0;
  serialPrintInterval = 10;

  initialize_ads();
}

void ADS1299::activateChannel(byte N)
{
  byte setting, startChan, endChan, targetSS;

  startChan = 8; endChan = 16;

  N = constrain(N-1,startChan,endChan-1);  // 0-7 or 8-15

  SDATAC();  // exit Read Data Continuous mode to communicate with ADS
  setting = 0x00;
  //  channelSettings[N][POWER_DOWN] = NO; // keep track of channel on/off in this array  REMOVE?
  setting |= channelSettings[N][GAIN_SET]; // gain
  setting |= channelSettings[N][INPUT_TYPE_SET]; // input code
  if(useSRB2[N] == true){channelSettings[N][SRB2_SET] = YES;}else{channelSettings[N][SRB2_SET] = NO;}
  if(channelSettings[N][SRB2_SET] == YES) {bitSet(setting,3);} // close this SRB2 switch
  WREG(CH1SET+(N-startChan),setting);
  // add or remove from inclusion in BIAS generation
  if(useInBias[N]){channelSettings[N][BIAS_SET] = YES;}else{channelSettings[N][BIAS_SET] = NO;}
  setting = RREG(BIAS_SENSP);       //get the current P bias settings
  if(channelSettings[N][BIAS_SET] == YES){
    bitSet(setting,N-startChan);    //set this channel's bit to add it to the bias generation
    useInBias[N] = true;
  }else{
    bitClear(setting,N-startChan);  // clear this channel's bit to remove from bias generation
    useInBias[N] = false;
  }
  WREG(BIAS_SENSP,setting); delay(1); //send the modified byte back to the ADS
  setting = RREG(BIAS_SENSN);       //get the current N bias settings
  if(channelSettings[N][BIAS_SET] == YES){
    bitSet(setting,N-startChan);    //set this channel's bit to add it to the bias generation
  }else{
    bitClear(setting,N-startChan);  // clear this channel's bit to remove from bias generation
  }
  WREG(BIAS_SENSN,setting); delay(1); //send the modified byte back to the ADS

  setting = 0x00;
  if(boardUseSRB1 == true) setting = 0x20;
  WREG(MISC1,setting);     // close all SRB1 swtiches
}

//  deactivate the given channel.
void ADS1299::deactivateChannel(byte N) {
  byte setting;
  byte startChan = 0;
  byte endChan = 8;

  SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS
  N = constrain(N-1,startChan,endChan-1);  //subtracts 1 so that we're counting from 0, not 1

  setting = RREG(CH1SET+(N-startChan)); delay(1); // get the current channel settings
  bitSet(setting,7);     // set bit7 to shut down channel
  bitClear(setting,3);   // clear bit3 to disclude from SRB2 if used
  WREG(CH1SET+(N-startChan),setting); delay(1);     // write the new value to disable the channel

  //remove the channel from the bias generation...
  setting = RREG(BIAS_SENSP); delay(1); //get the current bias settings
  bitClear(setting,N-startChan);                  //clear this channel's bit to remove from bias generation
  WREG(BIAS_SENSP,setting); delay(1);   //send the modified byte back to the ADS

  setting = RREG(BIAS_SENSN); delay(1); //get the current bias settings
  bitClear(setting,N-startChan);                  //clear this channel's bit to remove from bias generation
  WREG(BIAS_SENSN,setting); delay(1);   //send the modified byte back to the ADS

  leadOffSettings[N][0] = leadOffSettings[N][1] = NO;
  changeChannelLeadOffDetect(N+1);
}

// change the lead off detect settings for all channels
void ADS1299::changeChannelLeadOffDetect()
{
  byte setting, startChan, endChan;

  startChan = 0; endChan = 8;

  SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS
  byte P_setting = RREG(LOFF_SENSP);
  byte N_setting = RREG(LOFF_SENSN);

  for(int i=startChan; i<endChan; i++){
    if(leadOffSettings[i][PCHAN] == ON){
      bitSet(P_setting,i-startChan);
    }else{
      bitClear(P_setting,i-startChan);
    }
    if(leadOffSettings[i][NCHAN] == ON){
      bitSet(N_setting,i-startChan);
    }else{
      bitClear(N_setting,i-startChan);
    }
    WREG(LOFF_SENSP,P_setting);
    WREG(LOFF_SENSN,N_setting);
  }
}

// change the lead off detect settings for specified channel
void ADS1299::changeChannelLeadOffDetect(byte N)
{
  byte setting, startChan, endChan;

  startChan = 0; endChan = 8;

  N = constrain(N-1,startChan,endChan-1);
  SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS
  byte P_setting = RREG(LOFF_SENSP);
  byte N_setting = RREG(LOFF_SENSN);

  if(leadOffSettings[N][PCHAN] == ON){
    bitSet(P_setting,N-startChan);
  }else{
    bitClear(P_setting,N-startChan);
  }
  if(leadOffSettings[N][NCHAN] == ON){
    bitSet(N_setting,N-startChan);
  }else{
    bitClear(N_setting,N-startChan);
  }
  WREG(LOFF_SENSP,P_setting);
  WREG(LOFF_SENSN,N_setting);
}

void ADS1299::initialize_ads() {
  Serial.println("initialize_ads");
  // recommended power up sequence requiers >Tpor (~32mS)
  delay(50);
  pinMode(ADS_RST,OUTPUT);
  digitalWrite(ADS_RST,LOW);  // reset pin connected to both ADS ICs
  delayMicroseconds(4);   // toggle reset pin
  digitalWrite(ADS_RST,HIGH); // this will reset the Daisy if it is present
  delayMicroseconds(20);  // recommended to wait 18 Tclk before using device (~8uS);
  // initalize the  data ready chip select and reset pins:
  pinMode(DRDY, INPUT); // we get DRDY asertion from the on-board ADS
  delay(40);
  resetADS(); // reset the on-board ADS registers, and stop DataContinuousMode
  delay(10);
  WREG(CONFIG1,(ADS1299_CONFIG1_DAISY_NOT | curSampleRate)); // turn off clk output if no daisy present
  numChannels = 8;    // expect up to 8 ADS channels
  delay(40);

  // DEFAULT CHANNEL SETTINGS FOR ADS
  defaultChannelSettings[POWER_DOWN] = NO;        // on = NO, off = YES
  defaultChannelSettings[GAIN_SET] = ADS_GAIN24;     // Gain setting
  defaultChannelSettings[INPUT_TYPE_SET] = ADSINPUT_NORMAL;// input muxer setting
  defaultChannelSettings[BIAS_SET] = YES;    // add this channel to bias generation
  defaultChannelSettings[SRB2_SET] = YES;       // connect this P side to SRB2
  defaultChannelSettings[SRB1_SET] = NO;        // don't use SRB1

  for(int i=0; i<numChannels; i++){
    for(int j=0; j<6; j++){
      channelSettings[i][j] = defaultChannelSettings[j];  // assign default settings
    }
    useInBias[i] = true;    // keeping track of Bias Generation
    useSRB2[i] = true;      // keeping track of SRB2 inclusion
  }
  boardUseSRB1 = false;

  writeChannelSettings(); // write settings to the on-board and on-daisy ADS if present

  WREG(CONFIG3,0b11101100); delay(1);  // enable internal reference drive and etc.
  for(int i=0; i<numChannels; i++){  // turn off the impedance measure signal
    leadOffSettings[i][PCHAN] = OFF;
    leadOffSettings[i][NCHAN] = OFF;
  }

  streaming = false;
}

// write settings for ALL 8 channels for a given ADS board
// channel settings: powerDown, gain, inputType, SRB2, SRB1
void ADS1299::writeChannelSettings() {
  boolean use_SRB1 = false;
  byte setting, startChan, endChan, targetSS;

  startChan = 0; endChan = 8;

  SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS

  for(byte i=startChan; i<endChan; i++){ // write 8 channel settings
    setting = 0x00;
    if(channelSettings[i][POWER_DOWN] == YES){setting |= 0x80;}
    setting |= channelSettings[i][GAIN_SET]; // gain
    setting |= channelSettings[i][INPUT_TYPE_SET]; // input code
    if(channelSettings[i][SRB2_SET] == YES){
      setting |= 0x08;    // close this SRB2 switch
      useSRB2[i] = true;  // remember SRB2 state for this channel
    }else{
      useSRB2[i] = false; // rememver SRB2 state for this channel
    }
    WREG(CH1SET+(i-startChan),setting);  // write this channel's register settings

    // add or remove this channel from inclusion in BIAS generation
    setting = RREG(BIAS_SENSP);                   //get the current P bias settings
    if(channelSettings[i][BIAS_SET] == YES){
      bitSet(setting,i-startChan); useInBias[i] = true;    //add this channel to the bias generation
    }else{
      bitClear(setting,i-startChan); useInBias[i] = false; //remove this channel from bias generation
    }
    WREG(BIAS_SENSP,setting); delay(1);           //send the modified byte back to the ADS

    setting = RREG(BIAS_SENSN);                   //get the current N bias settings
    if(channelSettings[i][BIAS_SET] == YES){
      bitSet(setting,i-startChan);    //set this channel's bit to add it to the bias generation
    }else{
      bitClear(setting,i-startChan);  // clear this channel's bit to remove from bias generation
    }
    WREG(BIAS_SENSN,setting); delay(1);           //send the modified byte back to the ADS

    if(channelSettings[i][SRB1_SET] == YES){
      use_SRB1 = true;  // if any of the channel setting closes SRB1, it is closed for all
    }
  } // end of CHnSET and BIAS settings
  if(use_SRB1){
    for(int i=startChan; i<endChan; i++){
      channelSettings[i][SRB1_SET] = YES;
    }
    WREG(MISC1,0x20);     // close SRB1 swtich
    boardUseSRB1 = true;
  }else{
    for(int i=startChan; i<endChan; i++){
      channelSettings[i][SRB1_SET] = NO;
    }
    WREG(MISC1,0x00);    // open SRB1 switch
    boardUseSRB1 = false;
  }
}

void ADS1299::resetADS() {
  int startChan = 1;
  int stopChan = 8;
  RESET();             // send RESET command to default all registers
  SDATAC();            // exit Read Data Continuous mode to communicate with ADS
  delay(100);
  // turn off all channels
  for (int chan=startChan; chan <= stopChan; chan++) {
    deactivateChannel(chan);
  }
}

//System Commands
void ADS1299::WAKEUP() {
  digitalWrite(CS, LOW); //Low to communicate
  xfer(_WAKEUP);
  digitalWrite(CS, HIGH); //High to end communication
  delayMicroseconds(3);  //must way at least 4 tCLK cycles before sending another command (Datasheet, pg. 35)
}
void ADS1299::STANDBY() {
  digitalWrite(CS, LOW);
  xfer(_STANDBY);
  digitalWrite(CS, HIGH);
}
void ADS1299::RESET() {
  digitalWrite(CS, LOW);
  xfer(_RESET);
  delayMicroseconds(12); //must wait 18 tCLK cycles to execute this command (Datasheet, pg. 35)
  digitalWrite(CS, HIGH);
}
void ADS1299::START() {
  digitalWrite(CS, LOW);
  xfer(_START);
  digitalWrite(CS, HIGH);
}
void ADS1299::STOP() {
  digitalWrite(CS, LOW);
  xfer(_STOP);
  digitalWrite(CS, HIGH);
}
//Data Read Commands
void ADS1299::RDATAC() {
  digitalWrite(CS, LOW);
  xfer(_RDATAC);
  digitalWrite(CS, HIGH);
  delayMicroseconds(3);
}
void ADS1299::SDATAC() {
  digitalWrite(CS, LOW);
  xfer(_SDATAC);
  digitalWrite(CS, HIGH);
  delayMicroseconds(10);
}
void ADS1299::RDATA() {
  digitalWrite(CS, LOW);
  xfer(_RDATA);
  digitalWrite(CS, HIGH);
}

//print out the state of all the control registers
void ADS1299::printRegisters()
{
  boolean prevverbosityState = verbose;
  verbose = true;						// set up for verbosity output
  RREGS(0x00,0x0C);     	// read out the first registers
  delay(10);  						// stall to let all that data get read by the PC
  RREGS(0x0D,0x17-0x0D);	// read out the rest
  verbose = prevverbosityState;
}

byte ADS1299::getDeviceID() {      // simple hello world com check
  byte data = RREG(ID_REG);
  if(verbose){            // verbosity otuput
    Serial.print("On Board ADS ID ");
    printHex(data); Serial.println();
  }
  return data;
}

byte ADS1299::RREG(byte _address) { //  reads ONE register at _address
  byte opcode1 = _address + 0x20;   //  RREG expects 001rrrrr where rrrrr = _address
  digitalWrite(CS, LOW);            //  open SPI
  xfer(opcode1);                    //  opcode1
  xfer(0x00);                       //  opcode2
  regData[_address] = xfer(0x00);   //  update mirror location with returned byte
  digitalWrite(CS, HIGH);           //  close SPI
  if (verbose){                   //  verbose output
    printRegisterName(_address);
    printHex(_address);
    Serial.print(", ");
    printHex(regData[_address]);
    Serial.print(", ");
    for(byte j = 0; j<8; j++){
      Serial.print(bitRead(regData[_address], 7-j));
      if(j!=7) Serial.print(", ");
    }

    Serial.println();
  }
  return regData[_address];     // return requested register value
}

// Read more than one register starting at _address
void ADS1299::RREGS(byte _address, byte _numRegistersMinusOne) {

  byte opcode1 = _address + 0x20;   //  RREG expects 001rrrrr where rrrrr = _address
  digitalWrite(CS, LOW);        //  open SPI
  xfer(opcode1);          //  opcode1
  xfer(_numRegistersMinusOne);  //  opcode2
  for(int i = 0; i <= _numRegistersMinusOne; i++){
    regData[_address + i] = xfer(0x00);   //  add register byte to mirror array
  }
  digitalWrite(CS, HIGH);       //  close SPI
  if(verbose){            //  verbose output
    for(int i = 0; i<= _numRegistersMinusOne; i++){
      printRegisterName(_address + i);
      printHex(_address + i);
      Serial.print(", ");
      printHex(regData[_address + i]);
      Serial.print(", ");
      for(int j = 0; j<8; j++){
        Serial.print(bitRead(regData[_address + i], 7-j));
        if(j!=7) Serial.print(", ");
      }
      Serial.println();
      delay(30);
    }
  }
}

void ADS1299::WREG(byte _address, byte _value) { //  Write ONE register at _address
  byte opcode1 = _address + 0x40;   //  WREG expects 010rrrrr where rrrrr = _address
  digitalWrite(CS, LOW);      //  open SPI
  xfer(opcode1);          //  Send WREG command & address
  xfer(0x00);           //  Send number of registers to read -1
  xfer(_value);         //  Write the value to the register
  digitalWrite(CS, HIGH);      //  close SPI
  regData[_address] = _value;     //  update the mirror array
  if(verbose){            //  verbosity output
    Serial.print("Register ");
    printHex(_address);
    Serial.println(" modified.");
  }
}

void ADS1299::updateData(){
  byte boardStat, inByte;
  byte byteCounter = 0;
  if(digitalRead(DRDY) == LOW){
    digitalWrite(CS, LOW);      //  open SPI
    for(int i=0; i<3; i++){
      inByte = xfer(0x00);    //  read status register (1100 + LOFF_STATP + LOFF_STATN + GPIO[7:4])
      boardStat = (boardStat << 8) | inByte;
    }
    for(int i = 0; i < ADS_CHANS_PER_BOARD; i++){
      for(int j = 0; j < ADS_BYTES_PER_CHAN; j++){   //  read 24 bits of channel data in 8 3 byte chunks
        inByte = xfer(0x00);
        channelDataRaw[byteCounter] = inByte;  // raw data goes here
        byteCounter++;
      }
    }
    digitalWrite(CS, HIGH); // close SPI
    counter++;
    if (counter == serialPrintInterval) {
      counter = 0;
      Serial.print(outputCount);
      Serial.print(", ");
      for (int i=0;i<9; i++) {
        printHex(channelDataRaw[i]);
        if(i!=8) Serial.print(", ");

      }
      Serial.println();
      outputCount++;
    }
  }
}

// String-Byte converters for ADS
void ADS1299::printRegisterName(byte _address) {
  switch(_address){
    case ID_REG:
    Serial.print("ADS_ID, "); break;
    case CONFIG1:
    Serial.print("CONFIG1, "); break;
    case CONFIG2:
    Serial.print("CONFIG2, "); break;
    case CONFIG3:
    Serial.print("CONFIG3, "); break;
    case LOFF:
    Serial.print("LOFF, "); break;
    case CH1SET:
    Serial.print("CH1SET, "); break;
    case CH2SET:
    Serial.print("CH2SET, "); break;
    case CH3SET:
    Serial.print("CH3SET, "); break;
    case CH4SET:
    Serial.print("CH4SET, "); break;
    case CH5SET:
    Serial.print("CH5SET, "); break;
    case CH6SET:
    Serial.print("CH6SET, "); break;
    case CH7SET:
    Serial.print("CH7SET, "); break;
    case CH8SET:
    Serial.print("CH8SET, "); break;
    case BIAS_SENSP:
    Serial.print("BIAS_SENSP, "); break;
    case BIAS_SENSN:
    Serial.print("BIAS_SENSN, "); break;
    case LOFF_SENSP:
    Serial.print("LOFF_SENSP, "); break;
    case LOFF_SENSN:
    Serial.print("LOFF_SENSN, "); break;
    case LOFF_FLIP:
    Serial.print("LOFF_FLIP, "); break;
    case LOFF_STATP:
    Serial.print("LOFF_STATP, "); break;
    case LOFF_STATN:
    Serial.print("LOFF_STATN, "); break;
    case GPIO:
    Serial.print("GPIO, "); break;
    case MISC1:
    Serial.print("MISC1, "); break;
    case MISC2:
    Serial.print("MISC2, "); break;
    case CONFIG4:
    Serial.print("CONFIG4, "); break;
    default:
    break;
  }
}

//SPI communication methods
byte ADS1299::xfer(byte _data) {
  byte inByte;
  inByte = SPI.transfer(_data);
  return inByte;
}

// Used for printing HEX in verbose feedback mode
void ADS1299::printHex(byte _data) {
  Serial.print("0x");
  if(_data < 0x10) Serial.print("0");
  Serial.print(_data, HEX);
}


//-------------------------------------------------------------------//
//-------------------------------------------------------------------//
//-------------------------------------------------------------------//

////UNNECESSARY SPI-ARDUINO METHODS
//void ADS1299::attachInterrupt() {
//    SPCR |= _BV(SPIE);
//}
//
//void ADS1299::detachInterrupt() {
//    SPCR &= ~_BV(SPIE);
//}
//
//void ADS1299::begin() {
//    // Set direction register for SCK and MOSI pin.
//    // MISO pin automatically overrides to INPUT.
//    // When the SS pin is set as OUTPUT, it can be used as
//    // a general purpose output port (it doesn't influence
//    // SPI operations).
//
//    pinMode(SCK, OUTPUT);
//    pinMode(MOSI, OUTPUT);
//    pinMode(SS, OUTPUT);
//
//    digitalWrite(SCK, LOW);
//    digitalWrite(MOSI, LOW);
//    digitalWrite(SS, HIGH);
//
//    // Warning: if the SS pin ever becomes a LOW INPUT then SPI
//    // automatically switches to Slave, so the data direction of
//    // the SS pin MUST be kept as OUTPUT.
//    SPCR |= _BV(MSTR);
//    SPCR |= _BV(SPE);
//}
//
//void ADS1299::end() {
//    SPCR &= ~_BV(SPE);
//}
//
////void ADS1299::setBitOrder(uint8_t bitOrder)
////{
////    if(bitOrder == LSBFIRST) {
////        SPCR |= _BV(DORD);
////    } else {
////        SPCR &= ~(_BV(DORD));
////    }
////}
////
////void ADS1299::setDataMode(uint8_t mode)
////{
////    SPCR = (SPCR & ~SPI_MODE_MASK) | mode;
////}
////
////void ADS1299::setClockDivider(uint8_t rate)
////{
////    SPCR = (SPCR & ~SPI_CLOCK_MASK) | (rate & SPI_CLOCK_MASK);
////    SPSR = (SPSR & ~SPI_2XCLOCK_MASK) | ((rate >> 2) & SPI_2XCLOCK_MASK);
//}
