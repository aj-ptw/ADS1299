//
//  ADS1299.cpp
//
//  Created by Conor Russomanno on 6/17/13.
//


#include "ADS1299.h"

void ADS1299::setup(int _DRDY, int _CS){
  setup(_DRDY, _CS, false);
}

void ADS1299::setup(int _DRDY, int _CS, boolean verbose){

  // **** ----- SPI Setup ----- **** //

  // Set direction register for SCK and MOSI pin.
  // MISO pin automatically overrides to INPUT.
  // When the SS pin is set as OUTPUT, it can be used as
  // a general purpose output port (it doesn't influence
  // SPI operations).
  //
  verbose = verbose;

  SPI.begin();

  pinMode(SCK, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(SS, OUTPUT);

  digitalWrite(SCK, LOW);
  digitalWrite(MOSI, LOW);
  digitalWrite(SS, HIGH);

  SPI.setDataMode(SPI_MODE1);
  SPI.setFrequency(4000000);
  SPI.setBitOrder(MSBFIRST);

  // **** ----- End of SPI Setup ----- **** //

  // initalize the  data ready and chip select pins:
  DRDY = _DRDY;
  CS = _CS;
  pinMode(DRDY, INPUT);
  pinMode(CS, OUTPUT);

  tCLK = 0.000666; //666 ns (Datasheet, pg. 8)
  outputCount = 0;

  lastSerialPrint = 0;
  counter = 0;
  serialPrintInterval = 10;
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
