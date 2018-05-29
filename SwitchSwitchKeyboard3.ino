
#define RF false
#define DEBUG true

#include <Keyboard.h>

#if RF
  #include <SPI.h>
  #include "nRF24L01.h"
  #include "RF24.h"
#endif

//Revision History
//
//4/11/2018:  Added a new state to send alternate keystrokes.  KJF

//Switch Switch Keyboard
//Purpose:
//  The basic intent of this device is to take relay switch events (open/closed) to various destinations for 2 switches.
//  Right now, there are three destinations defined:
//    1.  Physical switches.
//        This just relays the switch stats to a device physically connected using 2n2222a open collector transistors.
//    2.  GBoard Keyboard.
//        If you plug the device into a host via USB, it is recognized as a keyboard.  
//        It will send keyboard strokes when the switches are closed.  Close one switch, it sends one character.
//        Close another switch, it sends another.
//        These characters a specified in the code.
//        These keyboard strokes are "pulsed", using an interval specified in the code.
//    3.  RF.
//        Sends data into the atmosphere using a NRF24L01.
//        One switch sends one set of data.  The other switch sends the other.
//        A seperate device can "catch" this data, and do what it would like with them.
//        The intent is for the other device to mimic the switch states.
//        *******
//        Right now, only one RF destination is defined.
//        It would be easy to create additional states for additional intended destinations.


//Pin Assignments:
//------------------------------------------------------------------------------------------
//These pin assignments are specific to the Arduino Leonardo.
//The NRF24L01 RF component uses these specific pins located in the ICSP BUS of the Leonardo:
//MOSI -> MOSI
//MISO -> MISO
//SCK -> SCK
//
//VCC goes to 3.3V on Ardunio.
//Ground goes to Ground.
const int RFCS = 7;  //CE for NRF24L01
const int RFCE = 8;  //CSN for NRF24L01

const int DitIn = 5;  //This is the digital pin to connect the dit switch
const int DahIn = 6; //This is the digitial pin to connect the dah switch

const int DitOut = 2;   //When in TM mode, this sends the dit switch states to the TM.
const int DahOut = 3;   //When in TM mode, this sends the dah switch states to the TM.

const int SpeakerOut = 9;  //The speaker pin.

const int DitPot = A0;  //Analog port for Dit Potent to adjust speed.
const int DahPot = A1;  //Analog port for Dah Potent to adjust speed.

const int SwitchBut = 12; // Switch states with button.

const int SWITCH_BY_TIME = 1;
const int SWITCH_BY_SWITCH = 2;
//----------------------------------------------------------------------------------------------


//Configurations
//----------------------------------------------------------------------------------------------

//The amount of time (ms) before scrolling through the outpuut destinations.
unsigned long STATE_SWITCH_TIME = 3000;

//These are the codes that the receiver is expecting to receive for DIT's and DAH's.
//Eventually, this should be changed to a value set by the end user, such as through DIP switches.
//But for now, this will need to be changed on both the TX and the RX code.
const int DitID = 'A';
const int DahID = 'B';

//When the device is in mode STATE_GB, these define the frequency for sending the Dit and Dah keyboard characters
const int DitTime = 400;
const int DahTime = 450;

//These set what character to send when in GB mode.
const char DitChar = '.';
const char DahChar = '-';

//const char ScanCode1 = '`';
//const char ScanCode2 = '~';
const char ScanCode1 = KEY_F11;
const char ScanCode2 = KEY_F12;


const int DitTone = 880;
const int DahTone = 500;

int SwitchWith = SWITCH_BY_SWITCH;

const int MUTE = false;

const int SpeedSense = 4; //This is the factor used to adjust the sensitivity of the Potet for speed adjustments.
                          //The potent returns a value between about 0 and 1000.
                          //We subtract the value of the potent divided by the SpeedSense value.
                          //A lower number makes the sensitivity of the potent greater.

                          

//Switches can be noisy.  When a switch is closed, it can produce many intermittened openings and closures which should be ignored.
//Debouncing the switches can eliminate this noise.
const long debounceDelay = 100;    // the debounce time; increase if the output flickers

//-------------------------------------------------------------------------------------------------


//Variable Assignments and Declarations
//-------------------------------------------------------------------------------------------------
int msg[1];  //Stores the message to be set via RF

long lastDebounceTime = 0;  // the last time the output pin was toggled

//The available states that are achieved by holding down a switch.
//TODO:  These should be put into an array.
const int STATE_PSY = 1;
const int STATE_GB = 2;
const int STATE_SCAN = 3;
const int STATE_RF1 = 4;

//Tracks what state is it in
int State = STATE_PSY;

int CurrentDitTime = DitTime;
int CurrentDahTime = DahTime;

int LastSwitchBut = 1;
int ThisSwitchBut = 1;


//These variables keep track of how long a switch was held.
unsigned long DitDownStart = 0;
unsigned long DahDownStart = 0;

char buf[4];

//When we are in a keyboard mode, we want to send the first character as soon as a switch is hit.  But only once.
//This keeps track of that for us.
int DitPlayFirst = true;
int DahPlayFirst = true;

#if RF
  RF24 radio(RFCS,RFCE);  //Declare the radio channel
  const uint64_t pipe = 0xE8E8F0F0E1LL;  //Define a pipe to send the radio communicato through
#endif

//---------------------------------------------------------------------------------------------------

void setup() 
{

#if RF
  radio.begin();
  radio.openWritingPipe(pipe);
#endif

  //Set up our pins
  pinMode(DitIn, INPUT_PULLUP);
  pinMode(DahIn, INPUT_PULLUP);
  pinMode(DitOut,OUTPUT);
  pinMode(DahOut,OUTPUT);
  pinMode(SpeakerOut, OUTPUT);

  pinMode(SwitchBut, INPUT_PULLUP);

#if Debug
  //Set for debugging.  
  Serial.begin(9600);
#endif

  //Begin the keyboard handler
  Keyboard.begin();
}





void loop() 
{



  ThisSwitchBut = digitalRead(SwitchBut);

  if((ThisSwitchBut == LOW) && (LastSwitchBut != ThisSwitchBut) && SwitchWith == SWITCH_BY_SWITCH)
  {
#if DEBUG
    dbgln("Switch button hit");
#endif
    IncrementState();
    //LastSwitchBut = ThisSwitchBut;
    delay(500);
  }

  LastSwitchBut = ThisSwitchBut;
  
  

  //If the Dit swtich is closed
  
  if (digitalRead(DitIn) == LOW)
    { 

   
      
    //If it is in PYSICAL state   
    //`````````````````````````````````````````````````
    if (State == STATE_PSY)
    {
#if DEBUG      
      dbgln("Dit was Hit");
      dbgln("We are in the physical state, so send HIGH to the DitOut Pin");
#endif
      //Close the physical Switch
      digitalWrite(DitOut,HIGH);
    }
    //``````````````````````````````````````````````````


#if RF
    //If it is in RF1 State   
    //```````````````````````````````````````````````````
    if (State == STATE_RF1)
    {

#if DEBUG      
      dbgln("Dit was Hit");
      dbg("We are in the RF State, so send ");
      dbg(DitID);
      dbg(" to the pipe");
#endif      
      //Send the RF data
      msg[0] = DitID;
      radio.write(msg,1);

    }
    //```````````````````````````````````````````````````
#endif
   

    //If it in GB state, AND the debounce interval has not elapsed.
    //``````````````````````````````````````````````````````````````````````````````````````
    
    if (State == STATE_GB || State == STATE_SCAN)
    {
#if DEBUG       
      dbgln("Dit was Hit");
      dbgln("We are in STATE_GB or STATE_SCAN");
      dbg("Debounce Delay set to: ");
      String(debounceDelay).toCharArray(buf,4);
      dbg(buf); 
      dbg(" Now: ");
      String(millis() - lastDebounceTime).toCharArray(buf,4);
      dbgln(buf);
#endif

      if(millis() - lastDebounceTime > debounceDelay)
      {
#if DEBUG        
        dbgln("We are within are Debounce tollerance.");
#endif        
       
        if (DitPlayFirst){
#if DEBUG
          dbgln("Playing First Dit Char");
#endif  
        
          DitPlayFirst = false;
          if (State == STATE_GB){
            Keyboard.write(DitChar);
          } 
          else if (State == STATE_SCAN){
            Keyboard.write(ScanCode1);    
          }
          playsnd(DitTone,CurrentDitTime / 2);

            
          
          delay(debounceDelay/2);
          lastDebounceTime = millis();
          
        }
        
        
        while ((digitalRead(DitIn) == LOW)  
        //within the tolarances of the Debounce      
        && (millis() - lastDebounceTime) > debounceDelay 
        //And we have not exceeded the switch hold down period.
        && (millis() - DitDownStart < STATE_SWITCH_TIME))
        {
#if DEBUG
          dbg("Our potent is set to ");

          dbg(".  This will set our speed to: ");
#endif          
          CurrentDitTime = DitTime - analogRead(DitPot) / SpeedSense;

#if DEBUG 
   
          String(1000/CurrentDitTime).toCharArray(buf,4);
          dbgln(buf);
          dbg("Output our DitChar, which is: ");
          dbgln(&DitChar);
#endif          
          if (millis() - lastDebounceTime > CurrentDitTime)
          {
            
            if (State == STATE_GB){
            Keyboard.write(DitChar);
            }
            else if (State == STATE_SCAN)
            {
              Keyboard.write(ScanCode1);
              }

            playsnd(DitTone, CurrentDitTime /2);
            
            //KJF:  Sigh... If only it were this easy.  Doing a delay is easy.  But, it screws up the timing.
            //Someone is going to want to hit the other switch quickly after this is hit, and they'll have to sit there.
            //And wait for this interval to pass.
            //delay(CurrentDitTime);
            
            lastDebounceTime = millis();
          }
          
        }
        lastDebounceTime = millis();
      }
    }
    


    //````````````````````````````````````````````````````````````````````````````````````````


    //The first time we come through this with the switch closed
    if (DitDownStart == 0)
    {
      //Set DitDownStart (keeps track of how long switch was closed)
      DitDownStart = millis();
      //And the debounce timer
      lastDebounceTime = millis();
    }

 
    

    //If the switch was held longer than the STATE_SWITCH_TIME  
    //KJF 4/30/2018...  Added a condition that allows a different way of switching.
    //  We are only doing this if the user is switching by holding down the switch.
    if ((millis() - DitDownStart >= STATE_SWITCH_TIME) && SwitchWith == SWITCH_BY_TIME)
      {
        IncrementState();
        DitDownStart = 0;
      }
     }    //if (digitalRead(DitIn) == LOW)
  
   else
   // I.E.:  The Dit switfch is NOT held down.
   // So, we are going to clear out some sfuff
   {
    
    DitDownStart = 0;
    if (State == STATE_PSY)
      {
        digitalWrite(DitOut,LOW);
        DitDownStart = 0;
      }

    if (State == STATE_RF1)
      {
        DitDownStart = 0;
      }

    if (State == STATE_GB || State == STATE_SCAN)
      {
        Keyboard.release(DitChar);
        // digitalWrite(DitOut,LOW);
        DitDownStart = 0;
        DitPlayFirst = true;

      }

      
     }

   

  //If the Dah switch is closed
  if (digitalRead(DahIn) == LOW)
  
  {
    //If it is in PHYSICAL state
   //```````````````````````````````````````````````````
    if (State == STATE_PSY)
    {
      dbgln("Dah was Hit");
      dbgln("We are in the physical state, so send HIGH to the DitOut Pin");
      //Close the physical switch
      digitalWrite(DahOut,HIGH);
    }
    //```````````````````````````````````````````````````


#if RF
    //If it is in RF1 State
    //```````````````````````````````````````````````````
    if (State == STATE_RF1)
    {
      dbgln("Dah was Hit");
      dbg("We are in the RF State, so send ");
      dbg(DahID);
      dbg(" to the pipe");
      //Send the RF data
      msg[0] = DahID;
      radio.write(msg,1);
    }
    //```````````````````````````````````````````````````
#endif
    

    //If it in GB state, AND the debounce interval has not elapsed.
    //``````````````````````````````````````````````````````````````````````````````````````
    if (State == STATE_GB  || State == STATE_SCAN)
    {
#if DEBUG       
      dbgln("Dah was Hit");
      dbgln("We are in STATE_GB or STATE_SCAN");
      dbg("Debounce Delay set to: ");
      String(debounceDelay).toCharArray(buf,4);
      dbg(buf); 
      dbg(" Now: ");
      String(millis() - lastDebounceTime).toCharArray(buf,4);
      dbgln(buf);
#endif
    
    
      if(millis() - lastDebounceTime > debounceDelay)
      {
#if DEBUG        
        dbgln("We are within are Debounce tollerance.");
#endif  
        if (DahPlayFirst){

#if DEBUG
          dbgln("Playing First Dah Char");
#endif          
          DahPlayFirst = false;
          
          if (State == STATE_GB){
          Keyboard.write(DahChar);
          }
          else if (State == STATE_SCAN)
          {
            Keyboard.write(ScanCode2);
          }

          playsnd(DahTone, CurrentDahTime /2);
          
          lastDebounceTime = millis();
          delay(debounceDelay/2);
          
        }
        

      //We are going to keep on outputting characters as long as the switch is closed.
      while ((digitalRead(DahIn) == LOW)  
        //within the tolarances of the Debounce      
        && (millis() - lastDebounceTime) > debounceDelay 
        //And we have not exceeded the switch hold down period.
        && (millis() - DahDownStart < STATE_SWITCH_TIME))
        {
#if DEBUG        
          dbg("Our potent is set to ");
          String(analogRead(DitPot)).toCharArray(buf,4);
          dbg(buf);
          dbg(".  This will set our speed to: ");
#endif         
          CurrentDahTime = DahTime - analogRead(DitPot) / SpeedSense;
#if DEBUG
          dbg("Current Dah Time: ");
          String(CurrentDahTime).toCharArray(buf,4);
          dbgln(buf);
          dbg("millis - lastdebouncetime: " );
          String(millis() - lastDebounceTime).toCharArray(buf,4);
          dbgln(buf);
          dbg("Output our DahChar, which is: ");
          dbgln(&DahChar);
#endif    
          if (millis() - lastDebounceTime > CurrentDahTime)
          {     
          
          if (State == STATE_GB){
            Keyboard.write(DahChar);
          }
          else if (State == STATE_SCAN){
            Keyboard.write(ScanCode2);
          }

          playsnd(DahTone, CurrentDahTime /2);
          
         
          //KJF:  Sigh... If only it were this easy.  Doing a delay is easy.  But, it screws up the timing.
          //Someone is going to want to hit the other switch quickly after this is hit, and they'll have to sit there.
          //And wait for this interval to pass.
          //delay(CurrentDahTime);
          lastDebounceTime = millis();
          }
        }
      
      //Keep track of the debounce
      lastDebounceTime = millis();
    }
    //````````````````````````````````````````````````````````````````````````````````````````
    }




    //The first time we come through this with the switch closed
    if (DahDownStart == 0)
    {
      //Set DitDownStart (keeps track of how long switch was closed)
      DahDownStart = millis();
      //And the debounce timer
      lastDebounceTime = millis();
    }
    
    //If the switch was held longer than the STATE_SWITCH_TIME   
    //KJF 4/30/2018...  Added a condition that allows a different way of switching.
    //  We are only doing this if the user is switching by holding down the switch.
    if ((millis() - DitDownStart >= STATE_SWITCH_TIME) && SwitchWith == SWITCH_BY_TIME)
      {
        IncrementState();
        DahDownStart = 0;
       }
  }
  
   // I.E.:  The Dah switfch is NOT held down.
   // So, we are going to clear out some sfuff
  else
   {
    DahDownStart = 0;
    if (State == STATE_PSY)
    {
      digitalWrite(DahOut,LOW);
      DahDownStart = 0;
    }

    if (State == STATE_RF1)
    {
      DahDownStart = 0;
    }

    if (State == STATE_GB || State == STATE_SCAN)
    {
      Keyboard.release(DahChar);
      //digitalWrite(DahOut,LOW);
      DahDownStart = 0;
      DahPlayFirst = true;
    }

   }

  }


void dbg(const char *msg){

#if DEBUG
  Serial.print(msg);
#endif  
}

void dbgln(const char *msg){

#if DEBUG
  Serial.println(msg);
#endif  
}

void playsnd(int freq,int dur)
//This function just plays the tone. 
//I am adding some functionality of the optionally compiled debug data.
//And the optional mut flag.  Plus, I don't have to call the speaker pin each time.
{
  if (!MUTE){
#if DEBUG
  dbg("Play a tone.  Frequency: ");
  String(freq).toCharArray(buf,4);
  dbg(buf);
  dbg(" MS:");  
  String(dur).toCharArray(buf,4);
  dbgln(buf);
#endif
  tone(SpeakerOut, freq, dur);
  }
  else{
#if DEBUG
    dbgln("Sound is muted.  I will do nothing");
#endif    
  }
}
  


void IncrementState() {
  char strState[4];
  dbgln("Entered IncrementSate Function");
    //Increment our State
  dbg("State was ");
  String(State).toCharArray(strState,4);
  dbgln(strState);
  State++;
  dbg("Now it is: ");
  String(State).toCharArray(strState,4);
  dbgln(strState);

  
  //We are switching states.
  //Let's not keep any keys held down.
  
  dbgln("Releasing Keyboard, and our transistors");
  Keyboard.releaseAll();
  digitalWrite(DitOut,LOW);
  digitalWrite(DahOut,LOW);
  
  playsnd(1000/State, 750);
  
  //And, we only have 3 states right now, so cycle back to the First one if we hit a 3rd.
  if (State == 4)
  {
    State = 1;
  }

}

