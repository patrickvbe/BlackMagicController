
/*

 */
#define UseATEM
//#define _DEBUG
//#define USE_WATCHDOG

#include <Ethernet.h>

#ifdef USE_WATCHDOG
  #include <avr/wdt.h> 
#endif

#ifdef UseATEM
  #include <ATEMext.h>
#else
  #include "DummyATEM.h"
#endif

#ifdef _DEBUG
  #define DebugPrint(arg) Serial.print(arg)
  #define DebugPrintln(arg) Serial.println(arg)
#else
  #define DebugPrint(arg)
  #define DebugPrintln(arg)
#endif

const byte keycols[] = {30, 28, 26, 24, 22, 23};
const byte colcount = sizeof(keycols);

// Currently implemented as one-shot mode.
class AntiJitterKeyboard
{
  public:
    const unsigned long jitterdelay = 50;
  
    AntiJitterKeyboard(const byte pin) : m_pin(pin)
    {
      m_lastchange = millis();
      m_lastvalue = 0;
    }
  
    byte Read()
    {
      // Read all columns for the row.
      pinMode(m_pin, OUTPUT);
      digitalWrite(m_pin, LOW);
      byte value = 0;
      for(byte colidx=0; colidx < colcount; colidx++)
      {
        value <<= 1;
        value |= (digitalRead(keycols[colidx]) == LOW ? 1 : 0);
      }
      pinMode(m_pin, INPUT_PULLUP);
  
      // anti-jitter
      if ( value != m_lastvalue )
      {
        m_lastchange = millis();
        m_lastvalue = value;
      }
      else
      {
        if ( m_lastvalue != m_currentvalue && (millis() - m_lastchange) > jitterdelay )
        {
          m_currentvalue = m_lastvalue;
          return m_currentvalue;
        }
      }
      return 0; // Nothing changed.
    }

  private:
    const byte    m_pin;
    byte          m_lastvalue;
    byte          m_currentvalue;
    unsigned long m_lastchange;
};

// Network configuration
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 241);

// The leds are controlled by four shift register IC's. Each IC controls one row.
const int ledcount = 4;
byte leds[ledcount];
byte previousleds[ledcount];
const int latchPin = 49;   // 74HC595 pin: ST_CP
const int clockPin = 48;   //              SH_CP
const int dataPin  = 46;   //              DS
bool wasintransition = false;

// This logic controls blinking leds
const unsigned long blinkspeed = 125;
unsigned long nextblink = millis();
bool blinkstatus = false;

// This controls the buttons and leds for the 'Preview' row.
AntiJitterKeyboard previewkb(25);
const int     ledindexpreview = 2;

// This controls the buttons and leds for the 'Program' row.
AntiJitterKeyboard programkb(27);
const int     ledindexprogram = 1;

// This controls the buttons and leds for the 'Aux' row.
AntiJitterKeyboard auxkb(29);
const int     ledindexaux = 0;

// This controls the buttons and leds for the 'Commands' row.
AntiJitterKeyboard commandkb(31);
const int     ledindexcommand = 3;

const int sliderport = A6;
int       lastslidervalue = 0;
bool      reverseslider = analogRead(sliderport) < 500;

#define SOURCE_PC1    1
#define SOURCE_PC2    2
#define SOURCE_LEFT   5
#define SOURCE_RIGHT  6
#define SOURCE_BLACK  0
#define SOURCE_PGM   10010

ATEMext AtemSwitcher;

const unsigned long REBOOT_NOT_CONNECTED_INTERVAL = 30000;
unsigned long reboottime = 0;
bool isconnected = true;

void setup()
{
  #ifdef USE_WATCHDOG
  wdt_disable();
  #endif
  
  // Pins controlling the shift registers
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin,  OUTPUT);

  // Keyboard pins
  for ( byte pin = 22; pin <= 31; pin++)
  {
    pinMode(pin,  INPUT_PULLUP);
  }

  // Cycle all leds to test them.
  for ( byte ledvalue = 2; ledvalue != B10000000; ledvalue = ledvalue << 1)
  {
    digitalWrite(latchPin, LOW);
    for ( int i = 0; i < 2; i++ )
    {
      shiftOut(dataPin, clockPin, LSBFIRST, ledvalue >> 1);
    }
    for ( int i = 0; i < 2; i++ )
    {
      shiftOut(dataPin, clockPin, MSBFIRST, ledvalue);
    }
    digitalWrite(latchPin, HIGH);
    delay(400);
  }

  // Clear all leds
  digitalWrite(latchPin, LOW);
  for ( int i = 0; i < ledcount; i++ ) shiftOut(dataPin, clockPin, LSBFIRST, 1);
  digitalWrite(latchPin, HIGH);

//  delay(30000);

  randomSeed(analogRead(5));  // For random port selection

#ifdef _DEBUG
  Serial.begin(115200);
#endif

  // Network connection
  DebugPrintln("Start network");
  Ethernet.begin(mac, ip);
  DebugPrintln(Ethernet.localIP());

  // Connect to an ATEM switcher on this address and using this local port:
  // The port number is chosen randomly among high numbers.
  DebugPrintln("Start switcher");
  AtemSwitcher.begin(IPAddress(192, 168, 1, 240));  // <= SETUP (the IP address of the ATEM switcher)
#ifdef _DEBUG
  AtemSwitcher.serialOutput(2);
#endif
  AtemSwitcher.connect();
}

void loop()
{
  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  // General time-base
  unsigned long cmillis = millis();

#ifdef USE_WATCHDOG
  // Simple watchdog like thingy. If not connected for a certain time, reboot.
  if ( AtemSwitcher.isConnected() != isconnected )
  {
    isconnected = !isconnected;
    DebugPrint("Connectie veranderd: ");
    DebugPrintln(isconnected);
    if ( !isconnected ) reboottime = cmillis + REBOOT_NOT_CONNECTED_INTERVAL;
  }
  else if ( !isconnected && cmillis > reboottime )
  {
    DebugPrintln("Reset!");
    // Set the watchdog. We don't reset it, so it will trigger.
    wdt_enable(WDTO_250MS);
    wdt_reset();
    reboottime = cmillis + REBOOT_NOT_CONNECTED_INTERVAL;
  }
#endif

  // Control blinking items.
  if ( cmillis >= nextblink )
  {
    blinkstatus = !blinkstatus;
    nextblink = cmillis + blinkspeed;
  }

  bool intransition = AtemSwitcher.getTransitionPosition(0) != 0;
  if ( wasintransition != intransition )
  {
    if ( wasintransition )
    {
      AtemSwitcher.setTransitionNextTransition(0, 1);
    }
    wasintransition = intransition;
  }

  // led status updates
  bool updateleds = false;
  byte statusleds = 0;
  bool keyonair = AtemSwitcher.getKeyerOnAirEnabled(0, 0);
  bool changekey = AtemSwitcher.getTransitionNextTransition(0) == 3;
  
  // ------------------ Process the Preview row ------------------

  // Set the status for the leds.
  uint16_t source = AtemSwitcher.getPreviewInputVideoSource(0);
  if ( source == SOURCE_PC1   ) statusleds |= 0x02;
  if ( source == SOURCE_PC2   ) statusleds |= 0x04;
  if ( source == SOURCE_LEFT  ) statusleds |= 0x08;
  if ( source == SOURCE_RIGHT ) statusleds |= 0x10;
  if ( blinkstatus && intransition ) statusleds = 0; 
  if ( changekey ^ keyonair ) statusleds |= 0x40;
  if ( leds[ledindexpreview] != statusleds )
  {
    leds[ledindexpreview] = statusleds;
    updateleds = true;
  }

  // Process actions on button presses
  byte buttons = previewkb.Read();
  if ( buttons )
  {
    if ( buttons & 0x01 )  AtemSwitcher.setPreviewInputVideoSource(0, SOURCE_PC1);
    if ( buttons & 0x02 )  AtemSwitcher.setPreviewInputVideoSource(0, SOURCE_PC2);
    if ( buttons & 0x04 )  AtemSwitcher.setPreviewInputVideoSource(0, SOURCE_LEFT);
    if ( buttons & 0x08 )  AtemSwitcher.setPreviewInputVideoSource(0, SOURCE_RIGHT);
    if ( buttons & 0x10 )  ; // Not used
    if ( buttons & 0x20 )  AtemSwitcher.setTransitionNextTransition(0, changekey ? 1 : 3);
  }

  // ------------------ Process the Program row ------------------

  // Set the status for the leds.
  statusleds = 0;
  source = AtemSwitcher.getProgramInputVideoSource(0);
  if ( source == SOURCE_PC1   ) statusleds |= 0x80;
  if ( source == SOURCE_PC2   ) statusleds |= 0x40;
  if ( source == SOURCE_LEFT  ) statusleds |= 0x20;
  if ( source == SOURCE_RIGHT ) statusleds |= 0x10;
  // Not used                      0x08
  if ( keyonair ) statusleds |= 0x04;
  if ( leds[ledindexprogram] != statusleds )
  {
    leds[ledindexprogram] = statusleds;
    updateleds = true;
  }

  // Process actions on button presses
  buttons = programkb.Read();
  if ( buttons )
  {
    if ( buttons & 0x01 )  AtemSwitcher.setProgramInputVideoSource(0, SOURCE_PC1);
    if ( buttons & 0x02 )  AtemSwitcher.setProgramInputVideoSource(0, SOURCE_PC2);
    if ( buttons & 0x04 )  AtemSwitcher.setProgramInputVideoSource(0, SOURCE_LEFT);
    if ( buttons & 0x08 )  AtemSwitcher.setProgramInputVideoSource(0, SOURCE_RIGHT);
    if ( buttons & 0x10 )  ; // Not used
    if ( buttons & 0x20 )
    {
      AtemSwitcher.setKeyerOnAirEnabled(0, 0, !keyonair);
      AtemSwitcher.setTransitionNextTransition(0, changekey ? 1 : 3);
    }
  }
  
  // ------------------ Process the Aux row ------------------

  // Set the status for the leds.
  statusleds = 0;
  source = AtemSwitcher.getAuxSourceInput(0);
//  DebugPrint("Source: ");
//  DebugPrintln(source);
  if ( source == SOURCE_PC1   ) statusleds |= 0x80;
  if ( source == SOURCE_PC2   ) statusleds |= 0x40;
  if ( source == SOURCE_PGM   ) statusleds |= 0x20;   // Program
  if ( source == SOURCE_BLACK ) statusleds |= 0x10;   // Black
  if ( leds[ledindexaux] != statusleds )
  {
    leds[ledindexaux] = statusleds;
    updateleds = true;
  }

  // Process actions on button presses
  buttons = auxkb.Read();
  if ( buttons )
  {
    if ( buttons & 0x01 )  AtemSwitcher.setAuxSourceInput(0, SOURCE_PC1);
    if ( buttons & 0x02 )  AtemSwitcher.setAuxSourceInput(0, SOURCE_PC2);
    if ( buttons & 0x04 )  AtemSwitcher.setAuxSourceInput(0, SOURCE_PGM);
    if ( buttons & 0x08 )  AtemSwitcher.setAuxSourceInput(0, SOURCE_BLACK);
  }
  
  // ------------------ Process the Command row ------------------

  // Set the status for the leds.
  statusleds = 0;
  if ( AtemSwitcher.getTransitionPosition(0) != 0 )         statusleds |=                   0x02;
  if ( AtemSwitcher.getFadeToBlackStateInTransition(0) )    statusleds |=                   0x04;
  else if ( AtemSwitcher.getFadeToBlackStateFullyBlack(0) ) statusleds |= blinkstatus ? 0 : 0x04;
  // Not used                                                          0x04;
  // Not used                                                          0x08;
  if ( leds[ledindexcommand] != statusleds )
  {
    leds[ledindexcommand] = statusleds;
    updateleds = true;
  }

  // Process actions on button presses
  buttons = commandkb.Read();
  if ( buttons )
  {
    DebugPrintln("Command button: ");
    DebugPrintln(buttons);
    if ( buttons & 0x01 )  AtemSwitcher.performAutoME(0);
    if ( buttons & 0x02 )  AtemSwitcher.performFadeToBlackME(0);
    if ( buttons & 0x04 ) {
      #ifdef UseATEM
      AtemSwitcher.setKeyerType(0, 0, 2); // 2 = pattern ?
			AtemSwitcher.setKeyerFillSource(0, 0, 1);
			AtemSwitcher.setKeyPatternPattern(0, 0, 1);
      AtemSwitcher.setKeyPatternSize(0, 0, 2000);
			AtemSwitcher.setKeyPatternSize(0, 0, 3000);
			AtemSwitcher.setKeyPatternSoftness(0, 0, 4500);
      AtemSwitcher.setTransitionNextTransition(0, keyonair ? 1 : 3);
      #endif
    }
    if ( buttons & 0x08 ) ; // Not used
  }

  const int marge = 10;
  
  // ------------------ Process the slider ------------------
  int slidervalue = analogRead(sliderport);
  // Create some solid jitter-free begin and end points
  if ( abs(lastslidervalue - slidervalue) >= marge )
  {
    if ( slidervalue <=         marge  ) slidervalue = 0;
    if ( slidervalue >= (1021 - marge) ) slidervalue = 1023;
    if ( slidervalue != lastslidervalue )
    {
      lastslidervalue = slidervalue;
      DebugPrint(slidervalue);
      DebugPrint(" -> ");
      // Now map to transition position.
      if ( reverseslider )  slidervalue = map(slidervalue, 0, 1023, 0, 10000);
      else                  slidervalue = map(slidervalue, 0, 1023, 10000, 0);
      DebugPrintln(slidervalue);
      AtemSwitcher.setTransitionPosition(0, slidervalue);
      if ( slidervalue == 10000) reverseslider = !reverseslider;
    }
  }
  
  // ------------------ Write the leds to the shift registes, if changed. ------------------
  if ( updateleds )
  {
    digitalWrite(latchPin, LOW);
    for ( int i = 0; i < ledcount; i++ ) shiftOut(dataPin, clockPin, MSBFIRST, leds[i]);
    digitalWrite(latchPin, HIGH);
  }
}
