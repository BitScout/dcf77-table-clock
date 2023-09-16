#include <Adafruit_NeoPixel.h>

// Bootup sequence:
// ---------------------------
// 1. Power delivery to DCF77 board switched on
// 2. 3 second delay to let the module tune itself
// 3. The onboard LED starts blinking each second (100 or 200 ms each)
// 4. After at least 60 seconds, the NeoPixel strip should start lighting up, one LED per second

// DCF77 board -> Arduino Nano
// ---------------------------
// VDD  -> D7 (For a hopefully cleaner power supply, and to simplify the wiring)
// GND  -> GND
// Pin3 -> Nothing
// Pin4 -> A7

// NeoPixel Strip -> Arduino Nano
// ---------------------------
// VDD  -> 5V
// GND  -> GND
// Data -> D6

#define DCF77_POWER_PIN 7
#define DCF77_DATA_PIN A7
#define BITCOUNT 58

#define LEDSPIN  6
#define LEDCOUNT 60
#define LED_OFFSET 59

bool debug = false;

int signal = 0;
int previousSignal = 0;

int signalTogglePoint = 512;
long flankUpMillis = 0;
long flankDownMillis = 0;
long markMillis = 0;
int  duration;
int  bitCounter = 0;
byte bitbuffer[BITCOUNT];
bool bit = true;

int minute  = 0;
int hour    = 0;
int day     = 0;
int weekday = 0;
int month   = 0;
int year    = 0;

byte minuteParity = 0;
byte hourParity   = 0;
byte dateParity   = 0;

bool minuteParityOkay = 1;
bool hourParityOkay   = 1;
bool dateParityOkay   = 1;

int bitValues[8] = {1, 2, 4, 8, 10, 20, 40, 80};

bool minuteSignalAquired = false;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDCOUNT, LEDSPIN, NEO_GRB + NEO_KHZ800);

uint32_t offBit  = strip.Color(0, 0, 0);
uint32_t oneBit  = strip.Color(0, 255, 0);
uint32_t zeroBit = strip.Color(0, 0, 63);
uint32_t errorBit = strip.Color(255, 0, 0);
uint32_t successBit = strip.Color(150, 150, 150);
uint32_t bootWaitingBit = strip.Color(0, 255, 0);
uint32_t bootDoneBit = strip.Color(255, 255, 255);
uint32_t bipBit = strip.Color(255, 255, 255);

void setup(void) {
  Serial.begin(9600);
  pinMode(DCF77_POWER_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DCF77_DATA_PIN, INPUT);
  digitalWrite(DCF77_POWER_PIN, HIGH);
  
  strip.begin();
  clearStrip();

  digitalWrite(LED_BUILTIN, LOW);
  strip.setPixelColor(LED_OFFSET, bootWaitingBit);
  strip.show();
  delay(3000); // Give DCF77 module 60 seconds to tune itself
  digitalWrite(LED_BUILTIN, HIGH);
  strip.setPixelColor(LED_OFFSET, bootDoneBit);
  strip.show();
}

void loop(void) {
  signal = analogRead(DCF77_DATA_PIN);
  Serial.print(signal); Serial.print("\t");
  Serial.print(1000); Serial.print("\t"); // Equivalent to 100 ms
  Serial.print(2000); Serial.print("\t"); // Equivalent to 200 ms
  Serial.println(duration); // Use serial plotter to watch this output
  
  if(signal > signalTogglePoint and previousSignal < signalTogglePoint) {
    if(millis() > (flankUpMillis + 800)) {
      flankUpMillis = millis();
      duration = (flankUpMillis - flankDownMillis);
      digitalWrite(LED_BUILTIN, HIGH);
      strip.setPixelColor(LED_OFFSET, bipBit);
      strip.show();
      
      if(duration > 1700 and duration < 2100) {
        minuteSignalAquired = true;
        mark();
      }
    }
  } else if(signal < signalTogglePoint and previousSignal > signalTogglePoint) {
    flankDownMillis = millis();
    duration = (flankDownMillis - flankUpMillis);

    digitalWrite(LED_BUILTIN, LOW);
    strip.setPixelColor(LED_OFFSET, offBit);
    strip.show();

    bit = (duration > 140 and duration < 250);
    
    processBit(bit);
    bitCounter++;
  }

  if(minuteSignalAquired && ((millis() - flankUpMillis) > 10000)) {
    digitalWrite(DCF77_POWER_PIN, LOW);
    clearStrip();
    delay(3000); // Give DCF77 module 3 seconds to lose power, then power it up again
    digitalWrite(DCF77_POWER_PIN, HIGH);
    delay(3000);

    minuteSignalAquired = false;
    flankUpMillis = millis();
  }
  
  previousSignal = signal;
}

void processBit(bool bit) {

  if(markMillis == 0) {
    return;
  }
  
  if(bit) {
    strip.setPixelColor(LED_OFFSET-bitCounter, oneBit);
  } else {
    strip.setPixelColor(LED_OFFSET-bitCounter, zeroBit);
  }

  if(bit) {
    bitbuffer[bitCounter] = 2;
  } else {
    bitbuffer[bitCounter] = 1;
  }

  if(bitCounter >= 21 and bitCounter <= 27) {
    if(bit) minuteParity++;
    minute += bit * bitValues[bitCounter - 21];
  } else if(bitCounter >= 29 and bitCounter <= 34) {
    if(bit) hourParity++;
    hour += bit * bitValues[bitCounter - 29];
  } else if(bitCounter >= 36 and bitCounter <= 57) {
    if(bit) dateParity++;

    if(bitCounter >= 36 and bitCounter <= 41) {
      day += bit * bitValues[bitCounter - 36];
    } else if(bitCounter >= 42 and bitCounter <= 44) {
      weekday += bit * bitValues[bitCounter - 42];
    } else if(bitCounter >= 45 and bitCounter <= 49) {
      month += bit * bitValues[bitCounter - 45];
    } else if(bitCounter >= 50 and bitCounter <= 57) {
      year += bit * bitValues[bitCounter - 50];
    }
  }

  switch (bitCounter) {
    case 28:
      if(bit) minuteParity++;
      minuteParityOkay = (minuteParity % 2 == 0);
      if (!minuteParityOkay) strip.setPixelColor(LED_OFFSET-bitCounter, errorBit);
      if (!minuteParityOkay and debug) Serial.println("Minute parity FAIL");
      break;
    case 35:
      if(bit) hourParity++;
      hourParityOkay = (hourParity % 2 == 0);
      if (!hourParityOkay) strip.setPixelColor(LED_OFFSET-bitCounter, errorBit);
      if (!hourParityOkay and debug) Serial.println("Hour parity FAIL");
      break;
    case 58:
      if(bit) dateParity++;
      dateParityOkay = (dateParity % 2 == 0);
      if (!dateParityOkay) strip.setPixelColor(LED_OFFSET-bitCounter, errorBit);
      if (!dateParityOkay and debug) Serial.println("Date parity FAIL");

      if (minuteParityOkay and hourParityOkay and dateParityOkay) strip.setPixelColor(LED_OFFSET-bitCounter - 1, successBit);
      
      break;
  }
  
  strip.show();

  if(debug) {
    Serial.print(bitCounter);
    Serial.print("\t");
    for(int i = 0; i < BITCOUNT; i++) {
      Serial.print(bitbuffer[i]);
    }
    Serial.print("\t");
    Serial.print(hour);
    Serial.print(":");
    Serial.print(minute);
    Serial.print("\t");
    Serial.print(weekday);
    Serial.print(" ");
    Serial.print(year + 2000);
    Serial.print("-");
    Serial.print(month);
    Serial.print("-");
    Serial.print(day);
    Serial.println("");
  }
}

void mark() {
  markMillis = millis();

  if(debug) {
    // Print the (probably) correct date and time
    if (markMillis > 0 and bitCounter > 56 and minuteParityOkay and hourParityOkay and dateParityOkay) {
      Serial.print(hour);
      Serial.print(":");
      Serial.print(minute);
      Serial.print("/");
      Serial.print(weekday);
      Serial.print("/");
      Serial.print(year + 2000);
      Serial.print("-");
      Serial.print(month);
      Serial.print("-");
      Serial.print(day);
      Serial.println("");
    }
  
    Serial.println("MARK");
    Serial.println("");
    Serial.println("Bit#\tBitstream\t\t\t\t\t\t\tTime\tDOW + Date");
  }

  for(int i = 0; i < BITCOUNT; i++) {
    bitbuffer[i] = 0;
  }

  clearStrip();
  
  bitCounter = 0;
  
  minute  = 0;
  hour    = 0;
  day     = 0;
  weekday = 0;
  month   = 0;
  year    = 0;
  
  minuteParity = 0;
  hourParity   = 0;
  dateParity   = 0;
}

void clearStrip() {
  for(int i = 0; i < LEDCOUNT; i++) {
    strip.setPixelColor(i, offBit);
  }
  strip.show();
}
