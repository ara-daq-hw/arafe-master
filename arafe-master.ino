int I2C_ADDRESS = 30;

//Set up pins:
//CARRIER clock for UART comms
const int CARRIER = P3_4;
//Power enable for slave boards
const int EN[4] = {18,16,32,30};  //{LED1, LED2, LED3, LED4}; for debug
//Communications select output:
const int COMMS_SEL[2] = {9,10};



#include <Wire.h>


#define INFO_SIGNATURE 0x03
#define CUR_REVISION 1

typedef struct info_t {
  unsigned char signature;              //< Indicates if the info structure is up to date.
  unsigned char revision;               //< What board revision this is.
  unsigned char power_default;  //< Holds the default values for the power scheme of the slaves.
} info_t;

info_t *my_info = (info_t *) 0x1800;

unsigned char currentRegisterPointer = 0;
unsigned char i2cRegisterMap[8] ={0,0,0,0,0,0,0,0};

int analogPort[8] = { 14, 17, 15, 13, 33, 34, 139, 138};
//  int analogPort = { A1, A13, A2, A0, A6, A7, VCC(A11), TEMPSENSOR}



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
  //Write values to register map
  i2cRegisterMap[1] = my_info->power_default;
  
  //Actually set this up:
  power(0x0, (i2cRegisterMap[1] >> 0 ) &  0x1);
  power(0x1, (i2cRegisterMap[1] >> 1 ) &  0x1);
  power(0x2, (i2cRegisterMap[1] >> 2 ) &  0x1);
  power(0x3, (i2cRegisterMap[1] >> 3 ) &  0x1);
  
  
    //Divide SMCLK speed by 2 --> 8MHz.
//  CSCTL3 |= (1u << 4);
  //Set up 8MHz clock on P3.4 as CARRIER signal:
  //Make it TB1.1: or maybe SMCLK: Right now SMCLK: FIXME: Make it TB1.1 and set up timer. Other modules have ddifficulties with a changed SMCLK!
  P3DIR |= (1u << 4);
  P3SEL0 |= (1u << 4);
  P3SEL1 &= ~(1u << 4);  //1 for SMCLK, 0 for TB1.1
  
 // pinMode(CARRIER, OUTPUT);
 //igitalWrite(CARRIER, LOW);
  //Setup the counter to use for the CARRIER signal

  //Stop timer:
  TB1CTL &= (00u << 4);
  //Clear setup:
  TB1CTL |= (1u << 2);
  
  TB1CTL &= ~(1u << 2);
  //Select input clock: SMCLK
  TB1CTL |= (1u << 9);
  //Select clock divider( on division ):
  TB1CTL |= (00u << 6);
  TB1EX0 |= (000u);
  
  //Set timer length to 0xFF:
//  TB1CTL |= (11u << 11);
  //Select compare mode:
  TB1CCTL1 &= (0u << 8); 
  //Set outmod to Toggle:
  TB1CCTL1 |= (100u << 5);  
  //Set compare number:
//  TB1CCR1 |= (0xf);
  TB1CCR0 |= (0x1);


  //Clear setup:
  TB1CTL |= (1u << 2);

  //Set timer to up mode (this starts the timer):
  TB1CTL |= (01u << 4);









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
  //pinMode(P3_5, OUTPUT);  //FIXME: Do we need to specifically define this as an output.

  //Setup I2C connection
  Wire.begin(I2C_ADDRESS);                 // join i2c bus with address #8: FIXME: Which address should it actually have?
  Wire.onReceive(receiveEvent);  // register write from I2C master
  Wire.onRequest(requestEvent);  // register read request from I2C master


  //Setup serial connections: Baudrate is 9600.
  Serial.begin(9600);            // start serial for debug port to computer.   
  Serial1.begin(9600);           // start serial for slave communication.
  Serial1.setTimeout(1000);      //Serial redBytes will timeout after 1000ms (this is only for information. The default is 1000ms anyway).

}







void loop()
{


  delay(100);
  //All functionality can be accessed via the Serial debug port. 
  waitForSerialDebugInput();

  //This chaecks the control register for the EXEC signal to go high and starts the requested process.
  waitForControl();

}




//This module 
void waitForSerialDebugInput(){
  char DEBUG_BUFFER[10];
  int DEBUG_BYTES = 0;
  int receivedEvent = 0;
  int inhibit=0;
  char data;
  //Write all incoming bytes into a buffer:
  if(Serial.available() ){
      if(Serial.read()=='c'){inhibit=1;}
  }
  while(inhibit==1){ 
   if(0 < Serial.available() ){
    data = Serial.read();
    if(data=='!'){ inhibit=0;}
    else{
          DEBUG_BUFFER[DEBUG_BYTES] = data;  
          Serial.print("Input was:  ");
          Serial.println(int(DEBUG_BUFFER[DEBUG_BYTES]), DEC);
          Serial.print("\n");
          DEBUG_BYTES++;
        }
    }
  }
  if(DEBUG_BYTES>1){
    currentRegisterPointer= (ascii(DEBUG_BUFFER[0]) <<4) | ascii(DEBUG_BUFFER[1]) ;
    currentRegisterPointer&=0x7;
    int howMany = DEBUG_BYTES/2;
    for (int i=1;i<howMany;i++) {
      i2cRegisterMap[currentRegisterPointer++] = (ascii(DEBUG_BUFFER[i*2]) <<4) | ascii(DEBUG_BUFFER[i*2+1]);
      currentRegisterPointer&=0x7;
    }
    
    
  }
}



//ASCII conversion, to be able to use the stupid debug monitor.
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



void waitForControl(){
  
  if(i2cRegisterMap[0] & 0x80){
      Serial.print("Before:");
      Serial.print(i2cRegisterMap[0]);
      Serial.print(", ");
    //Start power control
      power(0x0, (i2cRegisterMap[0] >> 0 ) &  0x1);
      power(0x1, (i2cRegisterMap[0] >> 1 ) &  0x1);
      power(0x2, (i2cRegisterMap[0] >> 2 ) &  0x1);
      power(0x3, (i2cRegisterMap[0] >> 3 ) &  0x1);
      i2cRegisterMap[0]&=~(0x80);
      Serial.print("After:");
      Serial.print(i2cRegisterMap[0]);
      Serial.print("\n");
      delay(100);
  }
  else if(i2cRegisterMap[1] & 0x80){
      Serial.print("Before:");
      Serial.print(", ");
      Serial.print(i2cRegisterMap[1]);
      //Update default power values
      my_info->power_default = i2cRegisterMap[1] & 0xf;
      i2cRegisterMap[1]&=~(0x80);
      Serial.print("After:");
      Serial.print(i2cRegisterMap[1]);
      Serial.print("\n");
      delay(100);
  }  

  else if(i2cRegisterMap[2] & 0x80){
      i2cRegisterMap[3]=0x0;
      Serial.print("Before:");
      Serial.print(", ");
      Serial.print(i2cRegisterMap[2]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[3]);

    //Convert monitoring value
      uint16_t monData = readMonitoring(i2cRegisterMap[2] & 0x7);
      i2cRegisterMap[2] |= ( (monData & 0x3) << 4 );
      i2cRegisterMap[3] |= ( (monData & 0x3ff) >> 2 );
      i2cRegisterMap[2]&=~(0x80);
      Serial.print("After:");
      Serial.print(i2cRegisterMap[2]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[3]);      
      Serial.print("\n");
      delay(100);
  }  

  else if(i2cRegisterMap[4] & 0x80){
      Serial.print("Before:");
      Serial.print(", ");
      Serial.print(i2cRegisterMap[4]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[5]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[6]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[7]);
      //Send command to slave.
      runComms(i2cRegisterMap[4] & 0x3);
      Serial.print("After:");
      Serial.print(i2cRegisterMap[4]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[5]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[6]);
      Serial.print(", ");
      Serial.print(i2cRegisterMap[7]);
      Serial.print("\n");
      delay(100);
  }
  
  
}


uint16_t readMonitoring(uint8_t num){
  int val=0;
  val = analogRead(analogPort[num]);
  Serial.print("Monitoring: ");
  Serial.println(val);
  Serial.print("\n");
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




void power(uint8_t dev, uint8_t on){
  if(on){
    digitalWrite(EN[dev], HIGH); 
  }
  else{
    digitalWrite(EN[dev], LOW); 
  }
}


int runComms(uint8_t dev){
  //1) Select output port
  select_output(dev);

  //2) Start with comparator setup:
//  setup_comparator(dev);

  //3) Start communication:
  Serial1.write('!');
  Serial1.write('M');
  Serial1.write('!');
  Serial1.write(i2cRegisterMap[5]);
  Serial1.write(i2cRegisterMap[6]);
  Serial1.write(0xFF);

  delay(10);
  //2) Start with comparator setup after message has been sent:
  setup_comparator(dev);
  //Wait for response:
  if(0 == waitForResponse(dev) ){

    //Reset control register after succesfull transmission.
    i2cRegisterMap[4]&=~(1u << 7);
  }
  else{//Timeout returns -1
    i2cRegisterMap[4]|=(1u << 6);
    i2cRegisterMap[4]&=~(1u << 7);
  }
  //End with comparator shutdown for power saving (FIXME: do we need this?):
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


const int expectedBytes_UART = 5;
char c[expectedBytes_UART];
int waitForResponse(uint8_t dev){
  Serial.print("wait for response \n");
  memset(c, 0, sizeof(c));
  int timeout = Serial1.readBytes(c, expectedBytes_UART);
  Serial.print("Received: ");
  Serial.print(c[0]);
  Serial.print(c[1]);
  Serial.print(c[2]);
  Serial.print(c[3]);
  if(c[0]=='!' && c[1]=='S' && c[2]=='!'){//Incoming response from slave
    i2cRegisterMap[7] = c[3];
    return 0;
  }
  else{
    return -1;
  }
}



