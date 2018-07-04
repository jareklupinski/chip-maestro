#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <MIDI.h>
#include "Channel.h"

const byte initTable[] PROGMEM = {0x00, 0x30, 0x01, 0x08, 0x02, 0x00, 0x03, 0x00, 0x04, 0x30, 0x05, 0x08, 0x06, 0x00, 0x07, 0x00, 0x08, 0x80, 0x09, 0x00, 0x0A, 0x00, 0x0B, 0x00, 0x0C, 0x30, 0x0D, 0x00, 0x0E, 0x00, 0x0F, 0x00, 0x10, 0x00, 0x11, 0x00, 0x12, 0x00, 0x13, 0x00, 0x14, 0x00, 0x15, 0x1F, 0x16, 0x00, 0x17, 0x40}; //Sets all APU registers to expected values.

MIDI_CREATE_DEFAULT_INSTANCE();

//Indicates whether all notes should decay upon release (true) or be muted instantly (false).
bool globalDecay = false;
//Toggles sweep mode, where simultaneous note presses perform sweeps, and designates a solo channel index (also used for arpeg mode).
bool sweepMode;
int soloChannel = 0;
//Toggles arpeggio mode, where simultaneous note presses create arpeggios between those notes, and selects an arpeggiation period.
bool arpegMode;
int arpegPeriod = 128;
int arpegDir = 0;
bool arpegUp = false;
//Indicates if Chip Maestro is in programming mode, enabled with the D0 key.
bool programming = false;
//Indicates whether or not to play option tones
bool optionTones = true;

//Stores instances of our Channel class. Triangle, Pulse 1, Pulse 2, Noise and DMC, respectively.
Channel channels[] = {Channel(-1, -1), Channel(-1, -1), Channel(-1, -1), Channel(-1, -1)};
int numChannels;

//A lock that ensures safe data transfer to the APU.
volatile boolean sendLock;

//Keeps track of all pressed keys, the index of the current arpeg note, and the number of loops it's been playing.
byte notesDown[16];
int arpegTick;

void handleNoteOff (byte channel, byte note, byte velocity){
  play(channel, note, velocity);
}
void handleNoteOn (byte channel, byte note, byte velocity){
  digitalWrite(10, HIGH);
    
  if (note == 14) //enter or exit programming mode
  {
    if (programming == false)
    {
      numChannels = 0;
      programming = true;
    }
    else
      asm volatile ("  jmp 0"); //resets the arduino, allowing the channels to be initialized
  }

  else if (programming) //program
  {
    bool added = false;
    if (velocity > 0)
    {
      byte chan = 0x00;
      switch (note)
      {
        case 16:
          chan = 0x01;
          break;
        case 17:
          chan = 0x02;
          break;
        case 19:
          chan = 0x03;
          break;
        case 21:
          chan = 0x04;
          break;
      }
      if (chan != 0x00)
      {
        for (int i = 0; i < numChannels; i++)
          if (EEPROM.read(2 + i) == chan)
            added = true;
        if (!added)
        {
          EEPROM.write(2+numChannels, chan);
          int optTone = 72 + (numChannels * 4) - (numChannels / 2);
          if (numChannels == 3)
            optTone = 84;
          if (chan == 0x01)
            optPlayChannel(0, 0, optTone, 200);
          else if (chan == 0x02)
            optPlayChannel(1, 0, optTone, 200);
          else if (chan == 0x03)
            optPlayChannel(1, 1, optTone, 200);
          else if (chan == 0x04)
            optPlayChannel(2, 0, optTone, 200);
          numChannels++;
        }
      }
      for (int i = numChannels; i < 4; i++) //clear blank channels in EEPROM
        EEPROM.write(2 + i, 0x00);
    }
  }
  
  else{
    byte volIn = 0;

    volIn = (byte)(velocity / 8);
      
    if (volIn > 0 && note >= 10 && note <= 19) //option keys
    {
      if (note == 10) //Option key A#-1: Factory reset
      {
        EEPROM.write(0, 0xFF);
        
        asm volatile ("  jmp 0");
      }
      if (note == 11) //Option key B-1: Toggle option tones
      {
        int currentTone = EEPROM.read(12);
        currentTone = (currentTone + 1) % 2;
        optionTones = currentTone;
        if (currentTone == 1)
        {
          //MARIO COIN SOUND
          for (int i = 0; i < 4; i++)
            if (channels[i].type == 1 && channels[i].index == 0)
            {
              clearChannels();
              channels[i].note = 83;
              channels[i].vol = 0x08;
              channels[i].setPacket();
              toNES(channels[i].packet, 6);
              delay(100);
              channels[i].note = 88;
              channels[i].setPacket();
              toNES(channels[i].packet, 6);
              delay(100);
              channels[i].decaying = true;
            }
        }
        else
        {
         for (int i = 0; i < 4; i++)
            if (channels[i].type == 1 && channels[i].index == 0)
            {
              clearChannels();
              channels[i].note = 88;
              channels[i].vol = 0x08;
              channels[i].setPacket();
              toNES(channels[i].packet, 6);
              delay(100);
              channels[i].note = 83;
              channels[i].setPacket();
              toNES(channels[i].packet, 6);
              delay(100);
              channels[i].decaying = true;
            } 
        }
        EEPROM.write(12, currentTone);
      }
      if (note == 12) //Option key C0: Toggle keyboard mode
      {
        byte currentMode = EEPROM.read(1);
        currentMode = (currentMode + 1) % 3;
        arpegMode = false;
        sweepMode = false;
        switch (currentMode)
        {
          case 0:
            optPlayChord();
            break;
          case 1:
            arpegMode = true;
            optPlayArpeg();
            break;
          case 2:
            sweepMode = true;
            optPlaySweep();
            break;
        }
        EEPROM.write(1, currentMode);
      }
      else if (note == 13 || note == 15) //duty cycles
      {
        //set square index
        int index = 0;
        if (note == 15)
          index = 1;
        //toggle next duty cycle
        byte currentDuty = EEPROM.read(7 + index);
        currentDuty = (currentDuty + 1) % 4;
        //find correct pulse channel and set duty cycle
        for (int i = 0; i < 4; i++)
          if (channels[i].type == 1 && channels[i].index == index)
          {
            channels[i].dutyCycle = currentDuty;;
            if (currentDuty != 0x03)
              optPlayTone(i, 72 + 4 * currentDuty - (currentDuty / 2), 200);
            else
              optPlayTone(i, 84, 200);
          }
        EEPROM.write(7 + index, currentDuty);
      }
      else if (note == 16) //solo channel toggle
      {
        int currentSolo = EEPROM.read(6);
        currentSolo = (currentSolo + 1) % numChannels;
        soloChannel = currentSolo;
        if (currentSolo != 3)
          optPlayTone(currentSolo, 72 + 4 * currentSolo - (currentSolo / 2), 200);
        else
          optPlayTone(currentSolo, 84, 200);
        EEPROM.write(6, currentSolo);
      }
      else if (note == 17) //arpeg direction toggle
      {
        int currentDir = EEPROM.read(9);
        currentDir = (currentDir + 1) % 3;
        arpegDir = currentDir;
        optPlayArpeg();
        EEPROM.write(9, currentDir);
      }
      else if (note == 18) //pitch bend range toggle
      {
        int currentRange = EEPROM.read(10);
        currentRange = (currentRange + 1) % 4;
        switch (currentRange)
        {
          case 0:
            channels[0].setBendRange(1);
            break;
          case 1:
            channels[0].setBendRange(2);
            break;
          case 2:
            channels[0].setBendRange(6);
            break;
          case 3:
            channels[0].setBendRange(12);
            break;
        }
        optPlaySweep();
        EEPROM.write(10, currentRange);
      }
      else if (note == 19) //tonal noise toggle
      {
        int currentTonal = EEPROM.read(11);
        currentTonal = (currentTonal + 1) % 2;
        channels[0].setTonalNoise(currentTonal);
        for (int i = 0; i < 4; i++)
          if (channels[i].type == 2)
            optPlayTone(i, 60, 200);
        EEPROM.write(11, currentTonal);
      }
    }

    setArrayBit(note, volIn != 0);
    if (arpegMode)
    {
      arpegTick = 0;
      arpegUp = true;
      channels[soloChannel].note = 0;
      setNextArpegNote();
    }
    else  
      play(channel, note, volIn);
  }
}
void handleControlChange (byte channel, byte number, byte value){
  if (number == 1){ //modulation wheel message
    if (arpegMode){ //change arpeggiation speed
      float percent = (float)arpegTick / arpegPeriod;
      arpegPeriod = (value + 10) * 10;
      arpegTick = floor(percent * arpegPeriod);
    }
    else if (sweepMode) //change sweep speed
    {
      float percent = (float)channels[soloChannel].sweepTick / channels[soloChannel].getSweepPeriod();
      channels[soloChannel].setSweepPeriod((value + 10) * 10);
      channels[soloChannel].sweepTick = floor(percent * channels[soloChannel].getSweepPeriod());
    }
    else //in piano mode, adjust decay speed
    {
      if (value == 0)
        globalDecay = false;
      else
      {
        globalDecay = true;
        channels[0].setDecayPeriod((value + 10) * 10);
        for (int i = 0; i < 4; i++)
          channels[i].decayTick = 0;
      }
    }
  }
  else if (number == 7) //volume control messages
  {
    byte volIn = (byte)(value * 2);
    if (channel >= 2 && channel <= 5) //specific channel
    {
      if (channels[channel - 2].envelope != volIn)
      {
        channels[channel - 2].envelope = volIn;
        channels[channel - 2].changed = true;
      }
    }
    else
    {//all channels
      for (int i = 0; i < 4; i++)
      {
        if (channels[i].envelope != volIn)
        {
          channels[i].envelope = volIn;
          channels[i].changed = true;
        }
      }
    }
  }
}
void handlePitchBend (byte channel, int bend){
  for (int i = 0; i < 4; i++){
      channels[i].pitchBend = bend;
      channels[i].changed = true;
    }
}
/*
void handleAfterTouchPoly (byte channel, byte note, byte pressure){}
void handleProgramChange (byte channel, byte number){}
void handleAfterTouchChannel (byte channel, byte pressure){}
void handleSystemExclusive (byte *array, unsigned int size){}
void handleTimeCodeQuarterFrame (byte data){}
void handleSongPosition (unsigned int beats){}
void handleSongSelect (byte songnumber){}
void handleTuneRequest (void){}
void handleClock (void){}
void handleStart (void){}
void handleContinue (void){}
void handleStop (void){}
void handleActiveSensing (void){}
void handleSystemReset (void){}
*/
void setup(){
  //initialize NES
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleNoteOn(handleNoteOn);
  //MIDI.setHandleAfterTouchPoly(handleAfterTouchPoly);
  MIDI.setHandleControlChange(handleControlChange);
  //MIDI.setHandleProgramChange(handleProgramChange);
  //MIDI.setHandleAfterTouchChannel(handleAfterTouchChannel);
  MIDI.setHandlePitchBend(handlePitchBend);
  /*
  MIDI.setHandleSystemExclusive(handleSystemExclusive);
  MIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame);
  MIDI.setHandleSongPosition(handleSongPosition);
  MIDI.setHandleSongSelect(handleSongSelect);
  MIDI.setHandleTuneRequest(handleTuneRequest);
  MIDI.setHandleClock(handleClock);
  MIDI.setHandleStart(handleStart);
  MIDI.setHandleContinue(handleContinue);
  MIDI.setHandleStop(handleStop);
  MIDI.setHandleActiveSensing(handleActiveSensing);
  MIDI.setHandleSystemReset(handleSystemReset);
  */
  MIDI.begin();
  
  DDRB = 0x03;
  DDRC = 0x3F;
  DDRD = 0xF8;
  PORTB = 0x00;
  PORTC = 0x1F;
  PORTD = 0xA8;
  pinMode(2,INPUT); //Interrupt Pin
  digitalWrite(2,HIGH);
  pinMode(10, OUTPUT); //LED Pin
  //attachInterrupt(0, unlock, RISING);              //begin interrupt function on pin 2, calls writeData on rising edge
  
  toNES((byte*)initTable, 48);
  
  boolean jingle = false;
  
  //initialize options
  if (EEPROM.read(0) == 0xFF) //first boot, set options
  {
    jingle = true;
    EEPROM.write(0, 0x01); //settings on
    EEPROM.write(1, 0x00); //piano mode
    EEPROM.write(2, 0x01); //triangle is first channel
    EEPROM.write(3, 0x02); //square 1 is second channel
    EEPROM.write(4, 0x03); //square 2 is third channel
    EEPROM.write(5, 0x00); //no fourth channel
    EEPROM.write(6, 0x01); //second channel is solo
    EEPROM.write(7, 0x02); //square 1 has 50% duty cycle
    EEPROM.write(8, 0x01); //square 2 has 25% duty cycle
    EEPROM.write(9, 0x00); //arpeg direction is up
    EEPROM.write(10, 0x02); //pitch bend range is 6 semitones
    EEPROM.write(11, 0x00); //tonal noise is off
    EEPROM.write(12, 0x01); //option tones are on
  }
  //load options
  arpegMode = false;
  sweepMode = false;
  switch(EEPROM.read(1))
  {
    case 0x01:
      arpegMode = true;
      break;
    case 0x02:
      sweepMode = true;
      break;
  }
  numChannels = 0;
  boolean tri = false, sq1 = false, sq2 = false, noise = false;
  for (int i = 0; i < 4; i++) //set channel order
  {
    byte index;
    switch (EEPROM.read(i + 2))
    {
      case 0x01:
        index = 0x01;
        numChannels++;
        break;
      case 0x02:
        index = 0x02;
        numChannels++;
        break;
      case 0x03:
        index = 0x03;
        numChannels++;
        break;
      case 0x04:
        index = 0x04;
        numChannels++;
        break;
      default:
        if (!tri)
          index = 0x01;
        else if (!sq1)
          index = 0x02;
        else if (!sq2)
          index = 0x03;
        else if (!noise)
          index = 0x04;
      break;
    }
    switch (index)
    {
      case 0x01:
        channels[i].type = 0;
        channels[i].index = 0;
        tri = true;
        break;
      case 0x02:
        channels[i].type = 1;
        channels[i].index = 0;
        channels[i].dutyCycle = EEPROM.read(7); //set square 1 duty cycle
        sq1 = true;
        break;
      case 0x03:
        channels[i].type = 1;
        channels[i].index = 1;
        channels[i].dutyCycle = EEPROM.read(8); //set square 2 duty cycle
        sq2 = true;
        break;
      case 0x04:
        channels[i].type = 2;
        channels[i].index = 0;
        noise = true;
        break;
    }
  }
  arpegDir = EEPROM.read(9);
  switch (EEPROM.read(10))
  {
    case 0x00:
      channels[0].setBendRange(1);
      break;
    case 0x01:
      channels[0].setBendRange(2);
      break;
    case 0x02:
      channels[0].setBendRange(6);
      break;
    case 0x03:
      channels[0].setBendRange(12);
      break;
  }
  channels[0].setTonalNoise(EEPROM.read(11));
  optionTones = EEPROM.read(12);
  
  //initialize globals
  arpegTick = 0;
  for (int i = 0; i < 16; i++)
    notesDown[i] = 0x0000;
  /*
  //play jingle
  if (jingle)
  {
    //OPENING JINGLE!
    int tempo = 150, off = 12;
    //BEAT 1
    play(38+off, 0x08); //D2 on
    channels[0].setPacket();
    toNES(channels[0].packet, 6);
    delay(tempo);
    //BEAT 2
    play(50+off, 0x04); //D3 on
    play(59+off, 0x04); //B3 on
    channels[1].setPacket();
    channels[2].setPacket();
    toNES(channels[1].packet, 6);
    toNES(channels[2].packet, 6);
    delay(tempo);
    //BEAT 3
    playChannel(1, 47+off, 0x04); //B2 on
    playChannel(2, 55+off, 0x04); //G3 on
    channels[1].setPacket();
    channels[2].setPacket();
    toNES(channels[1].packet, 6);
    toNES(channels[2].packet, 6);
    delay(tempo);
    //BEAT 4
    playChannel(1, 43+off, 0x04); //G2 on
    playChannel(2, 52+off, 0x04); //E3 on
    channels[1].setPacket();
    channels[2].setPacket();
    toNES(channels[1].packet, 6);
    toNES(channels[2].packet, 6);
    delay(tempo);
    //BEAT 5
    playChannel(0, 42+off, 0x08); //F#2 on
    playChannel(1, 38+off, 0x04); //D2 on
    playChannel(2, 50+off, 0x04); //D3 on
    channels[0].setPacket();
    channels[1].setPacket();
    channels[2].setPacket();
    toNES(channels[0].packet, 6);
    toNES(channels[1].packet, 6);
    toNES(channels[2].packet, 6);
    delay(tempo * 2);
    //BEAT 6
    playChannel(1, 36+off, 0x04); //C2 on
    playChannel(2, 48+off, 0x04); //C3 on
    channels[1].setPacket();
    channels[2].setPacket();
    toNES(channels[1].packet, 6);
    toNES(channels[2].packet, 6);
    delay(tempo * 3);
    //BEAT 7
    playChannel(0, 43+off, 0x08); //G1 on
    playChannel(1, 35+off, 0x04); //B1 on
    playChannel(2, 47+off, 0x04); //B2 on
    channels[0].setPacket();
    channels[1].setPacket();
    channels[2].setPacket();
    toNES(channels[0].packet, 6);
    toNES(channels[1].packet, 6);
    toNES(channels[2].packet, 6);
    delay(tempo * 3);
    //BEAT 8
    for (int i = 0; i < 3; i++)
      channels[i].decaying = true;
    //END OPENING JINGLE~!
  }
  */
}
void loop(){
  //update
  digitalWrite(10, LOW);
  if (arpegMode && channels[soloChannel].note != 0)
  {
    arpegTick++;
    if (arpegTick == arpegPeriod)
    {
      setNextArpegNote();
      arpegTick = 0;
    }
  }
  
  //check for MIDI events
  MIDI.read();
  
  //update all channels
  for (int i = 0; i < 4; i++)
    channels[i].update();
  //send changed channels
  for (int i = 0; i < 4; i++)
    if (channels[i].changed)
    {
      channels[i].setPacket();
      toNES(channels[i].packet, 6);
    }
}
void setNextArpegNote(){ //Finds the next key pressed, in ascending order, to continue arpeggiation
  int index = channels[soloChannel].note;
  boolean found = false;
  if (arpegDir == 0)
  {
    for (int i = 1; i <= 128 && !found; i++)
      if (getArrayBit((index + i) % 128))
      {
        index = (index + i) % 128;
        found = true;
      }
  }
  else if (arpegDir == 1) //arpeg down
  {
    for (int i = 1; i <= 128 && !found; i++)
    {
      int newIndex = (index - i) % 128;
      if (newIndex < 0)
        newIndex += 128;
      if (getArrayBit(newIndex))
      {
        index = newIndex;
        found = true;
      }
    }
  }
  else //arpeg ping-pong
  {
    int lowIndex = index, highIndex = index;
    bool otherNote = false;
    for (int i = index + 1; i < 128 && !otherNote; i++) //find highIndex
      if (getArrayBit(i))
      {
        highIndex = i;
        otherNote = true;
        found = true;
      }
    otherNote = false;
    for (int i = index - 1; i >= 0 && !otherNote; i--) //find lowIndex
      if (getArrayBit(i))
      {
        lowIndex = i;
        otherNote = true;
        found = true;
      }
    if (getArrayBit(index))
      found = true;
    if (arpegUp)
    {
      if (highIndex != index)
        index = highIndex;
      else
      {
        index = lowIndex;
        arpegUp = false;
      }
    }
    else //arpegDown
    {
      if (lowIndex != index)
         index = lowIndex;
      else
      {
        index = highIndex;
        arpegUp = true;
      }
    }
  }
  if (found)
  {
    if (index != channels[soloChannel].note)
      playChannel(soloChannel, index, 0x08);
  }
  else
    playChannel(soloChannel, 0, 0);
}
boolean getArrayBit(int index){ //Helper method: finds bit value at given index of notesDown
  int i = index / 8;
  int shift = index % 8;
  return bitRead(notesDown[i], shift);
}
void setArrayBit(int index, boolean val){ //Helper method: sets bit value at given index of notesDown
  int i = index / 8;
  int shift = index % 8;
  bitWrite(notesDown[i], shift, val);
}
void toNES(byte buffer[], int len){ //Transfers packets to the NES's Audio Processing Unit (APU)
  for (int i = 0; i < len; i += 2) //send to NES
  {
    //DUBIOUS CODE
    if (buffer[i] == 0xFF && buffer[i+1] == 0xFF)
      i += 2;
    //END DUBIOUS CODE
    
    sendLock = true;
    //digitalWrite(10, HIGH);
    attachInterrupt(0, unlock, RISING);              //begin interrupt function on pin 2, calls writeData on rising edge
    while (sendLock);                                //wait until switch is thrown back
    detachInterrupt(0);                              //disable interrupt
    PORTD = buffer[i]<<3;                            //set PORTD register to 5 bits of address
    PORTB = buffer[i+1]>>6;                          //set PORTB register to high 2 data bits
    PORTC = buffer[i+1];                             //set PORTC register to low 6 data bits
    //digitalWrite(10,LOW);
  }
}
void unlock(){ //Triggered by rising edge during toNES().
  sendLock = false;
}
void play(int channel, int note, byte vol){ //Selects the appropriate channel to modify based on MIDI input.
  switch (channel)
  {
    case 2:
      playChannel(0, note, vol);
      break;
    case 3:
      playChannel(1, note, vol);
      break;
    case 4:
      playChannel(2, note, vol);
      break;
    case 5:
      playChannel(3, note, vol);
      break;
    default:
      if (vol == 0x00) //note off event
      {
        if (sweepMode)
        {
          if (note == channels[soloChannel].sweepTarget) //sweepTarget has been released
          { //return to sweepFrom
            if (getArrayBit(channels[soloChannel].note)) //sweepFrom is still being held
            {
              channels[soloChannel].sweepTarget = channels[soloChannel].note;
              channels[soloChannel].sweepFrom = channels[soloChannel].sweepNote;
              channels[soloChannel].sweepTo = channels[soloChannel].getPeriod(channels[soloChannel].note);
              channels[soloChannel].sweepTick = 0;
            }
            else //no relevant notes are being held, end sweep
            {
              channels[soloChannel].clear();
            }
          }
          else if (note == channels[soloChannel].note) //sweepFrom has been released
          {
            if (channels[soloChannel].sweepTick == 0) //no current sweep)
            {
              channels[soloChannel].clear();
            }
            else //current sweep
            {
              //CODE GOES HERE?
            }
          }
        }
        else
          for (int i = 0; i < numChannels; i++) //find the channel that is playing the note
            if (channels[i].note == note)
            {
              if (globalDecay)
                channels[i].decaying = true;
              else
              {
                channels[i].note = 0;
                channels[i].vol = 0;
                channels[i].changed = true;
              }
            }
      }
      else //note on event
      {
        if (sweepMode)
        {
          if (channels[soloChannel].note != 0) //begin sweep
          {
            channels[soloChannel].sweepTarget = note;
            channels[soloChannel].sweepFrom = channels[soloChannel].getPeriod(channels[soloChannel].note);
            channels[soloChannel].sweepTo = channels[soloChannel].getPeriod(note);
            if (channels[soloChannel].sweepTick > 0) //if there is a ongoing sweep
              channels[soloChannel].sweepFrom = channels[soloChannel].sweepNote;
            else
              channels[soloChannel].sweepFrom = channels[soloChannel].getPeriod(channels[soloChannel].note);
            channels[soloChannel].sweepTick = 0;
          }
          else
          {
            playChannel(soloChannel, note, vol);
          }
        }
        else
          for (int i = 0; i < numChannels; i++) //note on event, find an available non-noise channel
            if (channels[i].note == 0 || channels[i].decaying) //if the channel is available
            {
              playChannel(i, note, vol);
              return;
            }
      }
      break;
  }
}
void play(int note, byte vol){
  play(0, note, vol);
}
void playChannel(int channel, int note, byte vol){
  channels[channel].note = note;
  channels[channel].vol = vol;
  channels[channel].decaying = false;
  channels[channel].changed = true; //turn note on
}
void optPlayChannel(int type, int index, int note, int duration){ //option sound effects
  for (int i = 0; i < 4; i++)
    if (channels[i].type == type && channels[i].index == index)
      optPlayTone(i, note, duration);
}
void optPlayTone(int note){
  optPlayTone(soloChannel, note, 200);
}
void optPlayTone(int channel, int note, int duration){
  if (optionTones)
  {
    clearChannels();
    playChannel(channel, note, 0x08);
    channels[channel].setPacket();
    toNES(channels[channel].packet, 6);
    delay(duration);
    clearChannels();
  }
}
void optPlayChord(){
  if (optionTones)
  {
    clearChannels();
    playChannel(0, 72, 0x08);
    playChannel(1, 76, 0x04);
    playChannel(2, 79, 0x04);
    for (int i = 0; i < 3; i++)
    {
      channels[i].setPacket();
      toNES(channels[i].packet, 6);
    }
    delay(200);
    clearChannels();
  }
}
void optPlayArpeg(){
  if (optionTones)
  {
    clearChannels();
    if (arpegDir == 0) //arpeg up
      for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
          optPlayTone(soloChannel, 72 + 4 * i - (i / 2), 50);
    else if (arpegDir == 1) //arpeg down
      for (int j = 0; j < 3; j++)
        for (int i = 2; i >= 0; i--)
          optPlayTone(soloChannel, 72 + 4 * i - (i / 2), 50);
    else //arpeg ping-pong
      for (int i = 0; i < 9; i++)
      {
        int note = i % 4;
        if (note == 3)
          note = 1;
        optPlayTone(soloChannel, 72 + 4 * note - (note / 2), 50);
      }
  }
}
void optPlaySweep(){
  if (optionTones)
  {
    clearChannels();
    playChannel(soloChannel, 72, 0x08);
    channels[soloChannel].sweepTarget = 72 + channels[soloChannel].getBendRange();
    channels[soloChannel].sweepFrom = channels[soloChannel].getPeriod(channels[soloChannel].note);
    channels[soloChannel].sweepTo = channels[soloChannel].getPeriod(channels[soloChannel].sweepTarget);
    channels[soloChannel].sweepTick = 0;
    for (int i = 0; i < 200; i++)
    {
      channels[soloChannel].update();
      channels[soloChannel].setPacket();
      toNES(channels[soloChannel].packet, 6);
      delay(1);
    }
    clearChannels();
  }
}
void clearChannels(){
  for (int i = 0; i < 4; i++)
  {
    playChannel(i, 0, 0);
    channels[i].setPacket();
    toNES(channels[i].packet, 6);
  }
}
