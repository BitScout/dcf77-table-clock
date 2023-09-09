#include <Adafruit_NeoPixel.h>

// Bootup sequence:
// ---------------------------
// 1. Power delivery to DCF77 board switched on
// 2. 30 second delay to let the module tune itself
// 3. 7 seconds of finding the distinction between high and low
// 4. The onboard LED starts blinking each second (100 or 200 ms each)
// 5. After at least 60 seconds, the NeoPixel strip should start lighting up, one LED per second

// DCF77 board -> Arduino Nano
// ---------------------------
// VDD  -> D7 (For a cleaner power supply)
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
#define SAMPLEZIZE 100
#define BITCOUNT 58

#define LEDSPIN  6
#define LEDCOUNT 144
#define LED_OFFSET 143

bool debug = true;

int signal = 0;
int previousSignal = 0;
int signalTogglePoint = 500;

int signalSamples[SAMPLEZIZE];

long flankUpMillis = 0;
long flankDownMillis = 0;
long markMillis = 0;
int  duration;
int  bitCounter = 0;
byte bitbuffer[BITCOUNT];

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

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDCOUNT, LEDSPIN, NEO_GRB + NEO_KHZ800);

uint32_t oneBit  = strip.Color(0, 255, 0);
uint32_t zeroBit = strip.Color(0, 0, 255);
uint32_t errorBit = strip.Color(255, 0, 0);
uint32_t successBit = strip.Color(150, 150, 150);

void setup(void) {
  Serial.begin(9600);
  pinMode(DCF77_POWER_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DCF77_DATA_PIN, INPUT);
  digitalWrite(DCF77_POWER_PIN, HIGH);
  
  strip.begin();
  clearStrip();

  digitalWrite(LED_BUILTIN, LOW);
  delay(3000); // Give DCF77 module 30 seconds to tune itself
  digitalWrite(LED_BUILTIN, HIGH);
  
  int low;
  int high;
  
  digitalWrite(LED_BUILTIN, HIGH);
  for (int i = 0; i < SAMPLEZIZE; i = i + 1) {
    signal = analogRead(DCF77_DATA_PIN);
    signalSamples[i] = signal;
    delay(70);
  }
  bubbleSort();
  digitalWrite(LED_BUILTIN, LOW);

  low = signalSamples[(int)(SAMPLEZIZE * 0.03)];
  high = signalSamples[(int)(SAMPLEZIZE * 0.97)];
  signalTogglePoint = low + (high - low) / 4;
  previousSignal = low;
  
  if(debug) {
    Serial.print("Detected levels: ");
    Serial.print(low);
    Serial.print(" / ");
    Serial.print(signalTogglePoint);
    Serial.print(" / ");
    Serial.println(high);
    Serial.println("Waiting for minute mark ...");
  }
}

void loop(void) {
  signal = analogRead(DCF77_DATA_PIN);
  //Serial.println(signal);
  
  if(signal > signalTogglePoint and previousSignal < signalTogglePoint) {
    flankUpMillis = millis();
    duration = (flankUpMillis - flankDownMillis);
    digitalWrite(LED_BUILTIN, HIGH);
    
    if(duration > 1500 and duration < 2500) {
      mark();
    }
  } else if(signal < signalTogglePoint and previousSignal > signalTogglePoint) {
    flankDownMillis = millis();
    duration = (flankDownMillis - flankUpMillis);
    digitalWrite(LED_BUILTIN, LOW);

    bool bit = (duration > 150 and duration < 250);
    
    processBit(bit);
    bitCounter++;
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
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
}

void bubbleSort() {
  int out, in, swapper;
  for(out=0 ; out < SAMPLEZIZE; out++) {  // outer loop
    for(in=out; in<(SAMPLEZIZE-1); in++)  {  // inner loop
      if( signalSamples[in] > signalSamples[in+1] ) {   // out of order?
        // swap them:
        swapper = signalSamples[in];
        signalSamples [in] = signalSamples[in+1];
        signalSamples[in+1] = swapper;
      }
    }
  }
}
