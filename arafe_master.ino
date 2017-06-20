//ARAFE master design
//Design by: Patrick Alison, Brian Clark, Thomas Meures
//meures@icecube.wisc.edu, clark.2668@osu.edu, allison.122@osu.edu
//Accombanying documentation:
//****ARAFE slave protocol:
//****ARAFE master register map:
//
//Description:
//**** This firmware module sets up the functionality for the ARAFE master board to receive communications via I2C or a serial debug port to
//**** control the ARAFE-PC boards and communicate with them. All communications are stored in registers. These registers are constantly checked
//**** and action is taken if a control bit in a register is high.
//The incoming communications from the serial debug port and I2C are interpreted in the following way:
//**** byte 1: pointer to register, to which to the following data should be written to.
//**** byte 2..x: data to be written to the register. If there are several data bytes, the pointer will increment with every written byte.
//************** NOTE: Only in case of a Slave control communication several bytes need to be written (SLAVECTL, COMMAND, ARG).
//Difference between I2C and Serial debug port:
//****I2C: data is delimited by standard I2C protocol. NOTE: The pointer and all data must be part of one I2C transfer.
//****Serial debug port: This is a custom design: Start delimiter "c", end delimiter "!". Everything in between will be treated as one transfer.
//********************** NOTE: When typing in a serial monitor, the pointer and all data bytes must be put in as 2-digit hexadecimal numbers. 
//********************** Example: 0x8 has to be typed as 08.
#define REG_MAX 8
unsigned char currentRegisterPointer = 0;
unsigned char i2cRegisterMap[REG_MAX] ={0,0,0,0,0,0,0,0};
//The register map is:
//***** Register 0: POWERCTL
//************* Bits [3:0]: indicates which slaves are currently powered on
//************* Bit [7]: Actually update power based on bits [3:0]. Clear when update is complete.
//***** Register 1: POWERDFLT
//************* Bits [3:0]: indicates which slaves are currently powered on by default
//************* Bit [7]: Actually update the non-volatile copy of this register. Clear when update is complete.
//***** Register 2: MONCTL
//************* Bits [3:0]: Monitoring value to convert (see next page)
//************* Bits [5:4]: Low 2 bits of the conversion
//************* Bit [7]: Actually convert the monitoring value. Clear when MONITOR/MONCTL are updated.
//***** Register 3: MONITOR
//************* Bits [7:0]: High 8 bits for the conversion
//***** Register 4: SLAVECTL
//************* Bits [1:0]: Destination for slave command
//************* Bit [7]: Actually send the command. Clear when response received or timeout received.
//************* Bit [6]: Set if command timed out
//***** Register 5: COMMAND
//************* Bits [7:0]: Command to send to slave
//***** Register 6: ARG
//************* Bits [7:0]: Argument to send to slave
//***** Register 7: ACK
//************* Bits [7:0] Acknowledged value received

//This allows to see some extra communications:
#define DEBUG_MODE 0

//Slave address for I2C communication:
int I2C_ADDRESS = 30;

//Set up pins:
//CARRIER for UART communication with ARAFE_PC boards
const int CARRIER = P3_4;
//Power enable pins for slave boards:
const int EN[4] = {18,16,32,30};  //{LED1, LED2, LED3, LED4}; for debug
//Communications select output:
const int COMMS_SEL[2] = {9,10};
//Analog ports for monitoring:
int analogPort[8] = { 14, 17, 15, 13, 33, 34, 139, 138};
//0:    15V_MON
//1:    CUR0, current to Slave 0
//2:    CUR1, current to Slave 1
//3:    CUR2, current to Slave 2
//4:    CUR3, current to Slave 3
//5:    !FAULT
//6:    3.3VCC
//7:    device temperature
//8:    firmware version
//9:    board ID (assigned via serial port only)
//10-15: unused currently

//Need Wire library for I2C comms.
#include <Wire.h>
#include <Cmd.h>
const char *cmd_banner = ">>> ARAFE-Master Command Interface";
const char *cmd_prompt = "ARAFE> ";
const char *cmd_unrecog = "Unknown command.";
#define FIRMWARE_VERSION 2

//The following structure is set up to store and recall a default start setup:
//The signature: Is checked on startup, to see if a setup has been stored already. If not, all slaves are kept powered off.
#define INFO_SIGNATURE 0x03
//The is the firmware revision: For now this is just there but ignored.
#define CUR_REVISION 1

//The info structure:
typedef struct info_t {
  unsigned char signature;              //< Indicates if the info structure is up to date.
  unsigned char revision;               //< What board revision this is.
  unsigned char power_default;  //< Holds the default values for the power scheme of the slaves.
  unsigned char serno;
} info_t;

//The location to store this in non-volatile memory: The address 0x1800 points to the info section of the memory.
info_t *my_info = (info_t *) 0x1800;

void enableXtal() {
}


void setup()
{

  //This is the power control part. Always start with power off:
  digitalWrite(EN[0], LOW);
  digitalWrite(EN[1], LOW);
  digitalWrite(EN[2], LOW);
  digitalWrite(EN[3], LOW);
  pinMode(EN[0], OUTPUT);
  pinMode(EN[1], OUTPUT);
  pinMode(EN[2], OUTPUT);
  pinMode(EN[3], OUTPUT);
  
  
  // Have the default power values ever been programmed? If not, set it up.
  if (my_info->signature != INFO_SIGNATURE) {
    my_info->revision = CUR_REVISION;
    
    my_info->power_default = 0x0;   //All power off.
    // when done, set the signature
   my_info->signature = INFO_SIGNATURE;
  }
  //Write values to default power register:
  i2cRegisterMap[1] = my_info->power_default;
  
  //Actually set this up:
  power(0x0, (i2cRegisterMap[1] >> 0 ) &  0x1);
  power(0x1, (i2cRegisterMap[1] >> 1 ) &  0x1);
  power(0x2, (i2cRegisterMap[1] >> 2 ) &  0x1);
  power(0x3, (i2cRegisterMap[1] >> 3 ) &  0x1);
  
  
  //Divide SMCLK speed by 2 --> 8MHz. Not executed!!
  //CSCTL3 |= (1u << 4);
    
  //Set up 4MHz clock on P3.4 as CARRIER signal:
  //**** Use timer-B. Set it up with no clock divider and in Toggle mode. The maximum frequency achievable in this way is 4MHz. 
  //**** For higher frequencies, mode needs to be set to up or down.
  //Make the CARRIER pin TB1.1:
  P3DIR |= (1u << 4);
  P3SEL0 |= (1u << 4);
  P3SEL1 &= ~(1u << 4);  //1 for SMCLK, 0 for TB1.1
  
  //Setup the counter to use for the CARRIER signal
  //Stop counter:
  TB1CTL &= (00u << 4);
  //Clear setup:
  TB1CTL |= (1u << 2);
  //Unclear: I don't thhink this is needed. Should happen automatically.
  TB1CTL &= ~(1u << 2);
  //Select input clock: SMCLK
  TB1CTL |= (1u << 9);
  //Select clock divider( no division ):
  TB1CTL |= (00u << 6);
  TB1EX0 |= (000u);
  
  //Set timer length to 0xFF:
  //This is the default setting
  // Otherwise, change:
  // TB1CTL |= (11u << 11);
  //Select compare mode:
  TB1CCTL1 &= (0u << 8); 
  //Set outmod to Toggle:
  TB1CCTL1 |= (100u << 5);  
  //Set compare number:
  TB1CCR0 |= (0x1);

  //Clear setup again: Probably not needed.
  TB1CTL |= (1u << 2);
  //Set timer to up mode (this starts the counter):
  TB1CTL |= (01u << 4);



  //Make PJ.4, 5 I/O ports. Could move this to the pin-definition file.
  PJSEL0 &= ~(1u << 4);
  PJSEL1 &= ~(1u << 4);
  PJSEL0 &= ~(1u << 5);
  PJSEL1 &= ~(1u << 5);

  //These two pins control the multiplexer for the communcations bus to the slave devices.
  digitalWrite(COMMS_SEL[0], LOW);
  digitalWrite(COMMS_SEL[1], LOW);
  pinMode(COMMS_SEL[0], OUTPUT);
  pinMode(COMMS_SEL[1], OUTPUT);

  //Connect CDOUT (comparator D output) to P3.5:
  P3DIR |= (1u << 5);
  P3SEL0 |= (1u << 5);
  P3SEL1 |= (1u << 5);

  //Setup I2C connection
  Wire.begin(I2C_ADDRESS);       // join i2c bus with address given above.
  Wire.onReceive(receiveEvent);  // register write from I2C master
  Wire.onRequest(requestEvent);  // register read request from I2C master


  //Setup serial connections: Baudrate is 9600.
  cmdInit(9600);
  cmdAdd("r", cmdRead);
  cmdAdd("w", cmdWrite);
  cmdAdd("sn", cmdAssign);
  cmdAdd("d", cmdDump);
  cmdAdd("?", cmdHelp);
  Serial1.begin(9600);           // start serial for slave communication.
  Serial1.setTimeout(1000);      //Serial redBytes will timeout after 1000ms (this is only for information. The default is 1000ms anyway).

  analogReference(INTERNAL1V5);
}

int cmdAssign(int argc, char **argv) {
  unsigned int serno;
  argc--;
  argv++;
  if (!argc) {
    Serial.println("sn needs a board id");
    return 0;
  }
  serno = strtoul(*argv, NULL, 0);
  if (serno < 256) {
    my_info->serno = serno;
  } else {
    Serial.println("serial number must be 8 bits (less than 256)");
  }
  return 0;
}
  
int cmdHelp(int argc, char **argv) {
  argc--;
  argv++;
  if (!argc) {
    Serial.println("r: r [register number] - read register");
    Serial.println("w: w [register number] [value] - write value to register");
    Serial.println("sn: sn [serial number] - assign serial number");
    Serial.println("d: d - print all registers");
    Serial.println("?: ? [regs|mons] - prints help. ? regs/? mons gives more info.");
  } else {
    if (!strcmp(*argv, "reghelp")) {
      Serial.println("All CTL registers use the top bit (0x80) to initiate action, and clear it when complete.");
      Serial.println("0  [POWERCTL]: [3:0] power on individual slaves");
      Serial.println("1   [DFLTCTL]: [3:0] slaves which come on automatically at power on");
      Serial.println("2    [MONCTL]: [3:0] mon value to convert, [5:4] low 2 bits of conversion");
      Serial.println("3   [MONITOR]: [7:0] high 8 bits of conversion");
      Serial.println("4  [SLAVECTL]: [1:0] slave to address, [6]: set if command timed out");
      Serial.println("5   [COMMAND]: command to send slave");
      Serial.println("6       [ARG]: argument to send slave");
      Serial.println("7       [ACK]: returned byte from slave");
    } else if (!strcmp(*argv, "monhelp")) {
      Serial.println("0: 15V_MON");
      Serial.println("1: CUR0");
      Serial.println("2: CUR1");
      Serial.println("3: CUR2");
      Serial.println("4: CUR3");
      Serial.println("5: FAULT");
      Serial.println("6: 3.3V");
      Serial.println("7: device temp");
      Serial.println("8: firmware version");
      Serial.println("9: serial number");
    }
  }
  return 0;
}

int cmdDump(int argc, char **argv) {
  int i;
  for (i=0;i<REG_MAX;i++) {
    Serial.print(i, DEC);
    Serial.print(": ");
    Serial.println(i2cRegisterMap[i], HEX);
  }
}

int cmdRead(int argc, char **argv) {
  unsigned int reg;
  argc--;
  argv++;
  if (!argc) {
    Serial.println("r needs a register to read from");
    return 0;
  }  
  reg = strtoul(*argv, NULL, 0);
  if (reg < REG_MAX) {
    Serial.println(i2cRegisterMap[reg], DEC);
  } else {
    Serial.print("register must be less than ");
    Serial.println(REG_MAX, DEC);
  }
  return 0;
}

int cmdWrite(int argc, char **argv) {
  unsigned int reg;
  unsigned int val;
  argc--;
  argv++;
  if (argc < 2) {
    Serial.println("w needs a register to write to and value to write");
    return 0;
  }
  reg = strtoul(*argv, NULL, 0);
  if (reg < REG_MAX) {
    if (val < 256) {
      i2cRegisterMap[reg] = val;
    } else {
      Serial.println("value must be 8 bits (less than 256)");
    }
  } else {
    Serial.println("register must be less than ");
    Serial.println(REG_MAX, DEC);
  }
  return 0;
}

//Start the program:
void loop()
{

  cmdPoll();

  //All functionality can be accessed via the Serial debug port.
  //  waitForSerialDebugInput();

  //This chaecks the control register for the EXEC signal to go high and starts the requested process.
  waitForControl();

}




//This module handles the serial input if any:
//void waitForSerialDebugInput(){
//  char DEBUG_BUFFER[10];
//  int DEBUG_BYTES = 0;
//  int receivedEvent = 0;
//  int inhibit=0;
//  char data;
//  //Write all incoming bytes into a buffer:
//  if(Serial.available() ){
//      if(Serial.read()=='c'){inhibit=1;}
//  }
//  while(inhibit==1){ 
//   if(0 < Serial.available() ){
//    data = Serial.read();
//    if(data=='!'){ inhibit=0;}
//    else{
//          DEBUG_BUFFER[DEBUG_BYTES] = data;
//#if DEBUG_MODE  
//          Serial.print("Input was:  ");
//          Serial.println(int(DEBUG_BUFFER[DEBUG_BYTES]), DEC);
//          Serial.print("\n");
//#endif
//          DEBUG_BYTES++;
//        }
//    }
//  }
//  if(DEBUG_BYTES>1){
//    //Write buffer into register space:
//    currentRegisterPointer= (ascii(DEBUG_BUFFER[0]) <<4) | ascii(DEBUG_BUFFER[1]) ;
//    currentRegisterPointer&=0x7;
//    int howMany = DEBUG_BYTES/2;
//    for (int i=1;i<howMany;i++) {
//      i2cRegisterMap[currentRegisterPointer++] = (ascii(DEBUG_BUFFER[i*2]) <<4) | ascii(DEBUG_BUFFER[i*2+1]);
//      currentRegisterPointer&=0x7;
//    }
//    
//    
//  }
//}



//ASCII conversion, to be able to use the stupid debug monitor. Probably I am just to stupid to understand it.
uint8_t ascii(char data){
  if(int(data)>47 && int(data)<58){//This is 0 to 9
    return int(data) - 48;
  }
  else if(int(data)>64 && int(data)<71){
    return int(data) - 65 +10; 
  }
  else if(int(data)>96 && int(data)<103){
    return int(data) - 97 +10; 
  }
}


// This module checks if any of the registers has changed, more precisely if any of the control bits are high. In that 
// case it takes the appropriate action.
void waitForControl(){
  
  if(i2cRegisterMap[0] & 0x80){
#if DEBUG_MODE
      Serial.print("Before:");
      Serial.print(i2cRegisterMap[0]);
      Serial.print(", ");
#endif
    //Start power control
      power(0x0, (i2cRegisterMap[0] >> 0 ) &  0x1);
      power(0x1, (i2cRegisterMap[0] >> 1 ) &  0x1);
      power(0x2, (i2cRegisterMap[0] >> 2 ) &  0x1);
      power(0x3, (i2cRegisterMap[0] >> 3 ) &  0x1);
      i2cRegisterMap[0]&=~(0x80);
#if DEBUG_MODE
      Serial.print("After:");
      Serial.print(i2cRegisterMap[0]);
      Serial.print("\n");
#endif
//      delay(100);
  }
  else if(i2cRegisterMap[1] & 0x80){
#if DEBUG_MODE
      Serial.print("Before:");
      Serial.print(", ");
      Serial.print(i2cRegisterMap[1]);
#endif
      //Update default power values
      my_info->power_default = i2cRegisterMap[1] & 0xf;
      i2cRegisterMap[1]&=~(0x80);
#if DEBUG_MODE
      Serial.print("After:");
      Serial.print(i2cRegisterMap[1]);
      Serial.print("\n");
#endif
//      delay(100);
  }  

  else if(i2cRegisterMap[2] & 0x80){
      i2cRegisterMap[3]=0x0;
#if DEBUG_MODE
      Serial.print("Before:");
      Serial.print(", ");
      Serial.print(i2cRegisterMap[2]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[3]);
#endif
    //Convert monitoring value
      if (i2cRegisterMap[2] & 0x08) {
        uint8_t intMon = i2cRegisterMap[2] & 0x7;
        // These are internal monitoring values.
        if (intMon == 0) {
          // firmware version
          i2cRegisterMap[3] = FIRMWARE_VERSION;
          i2cRegisterMap[2] &= ~(0x80);
        } else if (intMon == 1) {
          // board ID
          i2cRegisterMap[3] = my_info->serno;
          i2cRegisterMap[2] &= ~(0x80);
        }
      } else {
        uint16_t monData = readMonitoring(i2cRegisterMap[2] & 0x7);
        i2cRegisterMap[2] |= ( (monData & 0x3) << 4 );
        i2cRegisterMap[3] |= ( (monData & 0x3ff) >> 2 );
        i2cRegisterMap[2]&=~(0x80);
 #if DEBUG_MODE
        Serial.print("After:");
        Serial.print(i2cRegisterMap[2]);
        Serial.print(", ");
        Serial.print(i2cRegisterMap[3]);      
        Serial.print("\n");
 #endif      
      }
  }  

  else if(i2cRegisterMap[4] & 0x80){
#if DEBUG_MODE
      Serial.print("Before:");
      Serial.print(", ");
      Serial.print(i2cRegisterMap[4]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[5]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[6]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[7]);
#endif
      //Send command to slave.
      runComms(i2cRegisterMap[4] & 0x3);
#if DEBUG_MODE
      Serial.print("After:");
      Serial.print(i2cRegisterMap[4]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[5]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[6]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[7]);
      Serial.print("\n");
#endif
//      delay(100);
  }
}


uint16_t readMonitoring(uint8_t num){
  int val=0;
  val = analogRead(analogPort[num]);
#if DEBUG_MODE
  Serial.print("Monitoring: ");
  Serial.println(val);
  Serial.print("\n");
#endif
  return uint16_t(val);
}
  

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany) {
  unsigned int i;
  if (!howMany) return;
  currentRegisterPointer= Wire.read();
  currentRegisterPointer&=0x7;
  howMany--;
  for (i=0;i<howMany;i++) {
    i2cRegisterMap[currentRegisterPointer++] = Wire.read();
    currentRegisterPointer&=0x7;
  }
}
void requestEvent() {
  Wire.write(i2cRegisterMap[currentRegisterPointer]);
}



///This function wrappes the power control of the slave devices:
void power(uint8_t dev, uint8_t on){
  if(on){
    digitalWrite(EN[dev], HIGH); 
  }
  else{
    digitalWrite(EN[dev], LOW); 
  }
}

//Here the communication to the slave is actually sent:
int runComms(uint8_t dev){
  //1) Select output port
  select_output(dev);

  //2) Start communication:
  Serial1.write('!');
  Serial1.write('M');
  Serial1.write('!');
  Serial1.write(i2cRegisterMap[5]);
  Serial1.write(i2cRegisterMap[6]);
  Serial1.write(0xFF);


  //3) Start with comparator setup after message has been sent:
  delay(10); //Wait a moment because transmit echo is picked up by the RX line. 10us should be sufficient.
  setup_comparator(dev);
  //4) Wait for response:
  if(0 == waitForResponse(dev) ){

    //Reset control register after succesfull transmission.
    i2cRegisterMap[4]&=~(1u << 7);
  }
  else{//Timeout returns -1
    i2cRegisterMap[4]|=(1u << 6);
    i2cRegisterMap[4]&=~(1u << 7);
  }
  //5) End with comparator shutdown for power saving (FIXME: do we need this?):
  shutdown_comparator();
  return 0;
}

//Set the signal to the bus multiplexer.
void select_output(uint8_t dev){
  if(dev==0x0){ 
    write_comms_sel(0x2); 
  }
  else if(dev==0x1){ 
    write_comms_sel(0x3); 
  }
  else if(dev==0x2){ 
    write_comms_sel(0x0); 
  }
  else if(dev==0x3){ 
    write_comms_sel(0x1); 
  }
}


//Same as above, just trying to make the code a bit smoother.
void write_comms_sel(uint8_t sel){
  if(sel & 0x1){
    digitalWrite(COMMS_SEL[0], HIGH );
  } 
  else{
    digitalWrite(COMMS_SEL[0], LOW );
  }

  if(sel & 0x2){
    digitalWrite(COMMS_SEL[1], HIGH );
  } 
  else{
    digitalWrite(COMMS_SEL[1], LOW );
  }
}

//Comparator needs to be set up to receive return from the right slave:
void setup_comparator(uint8_t dev){

  //2) Set the Comparator_D input channel:
  if(dev == 0){
    CDCTL0 |= (11u << 8);//V-=CD3
    CDCTL0 |= (1111u << 0);//V+=CD15
    //   Serial.print("setup 0");
  }
  if(dev == 1){
    CDCTL0 |= (101u << 8);//V-=CD5
    CDCTL0 |= (100u << 0);//V+=CD4
  }
  if(dev == 2){
    CDCTL0 |= (111u << 8);//V-=CD7
    CDCTL0 |= (110u << 0);//V+=CD6
  }
  if(dev == 3){
    CDCTL0 |= (1001u << 8);//V-=CD9
    CDCTL0 |= (1000u << 0);//V+=CD8
  }

  //Enable input channels:
  CDCTL0 |= (1u <<15);//V-
  CDCTL0 |= (1u <<7);//V+
  //Switch on comparator:
  CDCTL1 |= (1u <<10);
}

void shutdown_comparator(){
  // End: Switch off comparator:
  CDCTL1 &= ~(1u << 10);
  //Disable input:
  CDCTL0 &= ~(1u <<15);//V-
  CDCTL0 &= ~(1u <<7);//V+
}

//We always expect 5 bytes back. This is used for the timeout. It works for now, needs to be updated if the comms protocol changes:
const int expectedBytes_UART = 5;
char c[expectedBytes_UART];
int waitForResponse(uint8_t dev){
#if DEBUG_MODE
  Serial.print("wait for response \n");
#endif
  memset(c, 0, sizeof(c));
  //FIXME: Change this to look for a delimiter, rather then just the right number of bytes. Maybe both!
  int timeout = Serial1.readBytes(c, expectedBytes_UART);
#if DEBUG_MODE
  Serial.print("Received: ");
  Serial.print(c[0]);
  Serial.print(c[1]);
  Serial.print(c[2]);
  Serial.print(c[3]);
#endif
  if(c[0]=='!' && c[1]=='S' && c[2]=='!'){//Incoming response from slave
    i2cRegisterMap[7] = c[3];
    return 0;
  }
  else{
    return -1;
  }
}



