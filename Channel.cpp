#include "Channel.h"

//constructor
Channel::Channel(int type, int index)
{
  this->type = type;
  this->index = index;
  note = 0;
  vol = 0;
  envelope = 0xFF;
  lastHigh = 0xFF;
  dutyCycle = 0;
  changed = false;
  decaying = false;
  decayTick = 0;
  sweepNote = 0;
  sweepTarget = 0;
  sweepFrom = 0;
  sweepTo = 0;
  sweepTick = 0;
  pitchBend = 64;
}

//Called once every main loop to adjust various attributes of the channel.
void Channel::update()
{
  if (sweepTo != 0) //sweep
  {
    float percent = (float)sweepTick / sweepPeriod;
    sweepNote = sweepFrom + floor(percent * (sweepTo - sweepFrom));
    changed = true;
    
    sweepTick++;
    if (sweepTick == sweepPeriod) //sweep is complete
    {
      note = sweepTarget; //lock target 
      sweepNote = 0;
      sweepTarget = 0;
      sweepFrom = 0;
      sweepTo = 0;
      sweepTick = 0;
    }
  }
  
  if (decaying) //decrease decaying volume
  {
    if (vol > 0x00)
    {
      decayTick++;
      if (decayTick == decayPeriod)
      {
        vol--;
        decayTick = 0;
        changed = true;
      }
    }
    if (vol == 0x00) //note has fully decayed, silence channel completely
    {
      note = 0;
      decaying = false;
    }
  }
}

//Constructs a packet of addresses and data to be sent to the APU.
void Channel::setPacket()
{
  //clear packet
  for (int i = 0; i < 6; i++)
    packet[i] = 0xFF;

  int period = getPeriod(note);
  byte finalVol = floor((envelope / (float)255) * vol);
  if (sweepNote != 0) //replace with interpolated period if a sweep is occurring
    period = sweepNote;

  if (type == 0) //triangle wave
  {
    packet[0] = 0x08;
    packet[1] = 0x81;
    packet[2] = 0x0A;
    packet[3] = period;
    packet[4] = 0x0B;
    packet[5] = period>>8;
    if (finalVol == 0) //if channel muted
      packet[1] = 0x80; //mute it
  }
  else if (type == 1) //pulse wave
  {
    packet[0] = 0x03;
    packet[1] = period>>8;
    packet[2] = 0x02;
    packet[3] = period;
    packet[4] = 0x00;
    packet[5] = (dutyCycle<<6)|finalVol|0x30;
    
    if (index == 1) //second pulse wave
    {
      packet[0] = 0x07;
      packet[2] = 0x06;
      packet[4] = 0x04;
    }
    
    if (!dirtyPulse && period>>8 == lastHigh) //high freq byte has not changed, do not resend
    {
      packet[0] = 0xFF;
      packet[1] = 0xFF;
    }
    lastHigh = period>>8;
  }
  else if (type == 2) //noise wave
  {
    packet[0] = 0x0F;
    if (finalVol > 0) //if note on
      packet[1] = 0xFF;
    else
      packet[1] = 0x00;
    packet[2] = 0x0C;
    packet[3] = finalVol | 0x30;
    packet[4] = 0x0E;
    packet[5] = 0x0F - (note & 0x0F);
    packet[5] |= tonalNoise << 7;
  }
  else if (type == 3) //DMC
  {
    packet[0] = 0x11;
    packet[1] = period;
  }
  
  changed = false;
}

int Channel::getPeriod(int note)
{
  int index = note;
  if (type == 0)
    index = min(127, note + 12);
  if (pitchBend == 64)
    return noteTable[index];
  
  //calculate bent period
  int var = pitchBend - 64;
  int index2;
  if (var < 0)
    index2 = max(0, index - bendRange);
  else
    index2 = min(127, index + bendRange);
  var = abs(var);
  float percent = (float)var / 64;
  return noteTable[index] + floor(percent * (noteTable[index2] - noteTable[index]));
}

void Channel::clear()
{
  note = 0;
  vol = 0;
  sweepTick = 0;
  changed = true;
}

//static variable accessors/mutators
boolean Channel::getDirtyPulse()
{
  return dirtyPulse;
}
void Channel::setDirtyPulse(boolean val)
{
  dirtyPulse = val;
}
boolean Channel::getTonalNoise()
{
  return tonalNoise;
}
void Channel::setTonalNoise(boolean val)
{
  tonalNoise = val;
}
int Channel::getDecayPeriod()
{
  return decayPeriod;
}
void Channel::setDecayPeriod(int val)
{
  decayPeriod = val;
}
int Channel::getSweepPeriod()
{
  return sweepPeriod;
}
void Channel::setSweepPeriod(int val)
{
  sweepPeriod = val;
}
int Channel::getBendRange()
{
  return bendRange;
}
void Channel::setBendRange(int val)
{
  bendRange = val;
}
