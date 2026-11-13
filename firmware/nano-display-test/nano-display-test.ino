// --------------------------------------------------
// NANO DISPLAY TEST
//
// Breadboard check for the keychain's display, the CC56-12EWA
// four digit common cathode part, running on a plain Arduino
// Nano instead of the keychain board.
//
// At power up it checks the wiring in order: every digit on its
// own, every segment on its own, the digit order, and the four
// decimal point positions. Each step is named on the serial
// monitor so what lights up can be compared with what should
// light up. When the check finishes the display counts 0000 to
// 9999, which shows every character the game ever draws.
//
// Press reset to run the check again. Nothing else is needed:
// no buttons, no library, and the display is the only thing
// wired to the Nano.
//
// Wiring, display pin -> 330R -> Nano, for the eight segments:
//
//   a    pin 11   D12        CC1  pin 12   Q1 collector
//   b    pin  7   D5         CC2  pin  9   Q2 collector
//   c    pin  4   D7         CC3  pin  8   Q3 collector
//   d    pin  2   D11        CC4  pin  6   Q4 collector
//   e    pin  1   D13
//   f    pin 10   D10        Each base through 1k:
//   g    pin  5   D6           Q1 A2   Q2 D8   Q3 A1   Q4 D4
//   dp   pin  3   D9
//
// All four emitters to the Nano's ground. Digit 2 is D8 here
// because the keychain board drives it from PB7, which a Nano
// does not bring out.
// --------------------------------------------------


const int SEG_A  = 12;
const int SEG_B  = 5;
const int SEG_C  = 7;
const int SEG_D  = 11;
const int SEG_E  = 13;
const int SEG_F  = 10;
const int SEG_G  = 6;
const int SEG_DP = 9;

// Index order matches the pattern bits: bit 0 is a through bit 6
// is g, and bit 7 is the decimal point
const byte SEG_PINS[8] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G, SEG_DP };
const byte SEG_DISPLAY_PIN[8] = { 11, 7, 4, 2, 1, 10, 5, 3 };
const char *SEG_NAME[8] = { "a", "b", "c", "d", "e", "f", "g", "dp" };
const char *SEG_NANO[8] = { "D12", "D5", "D7", "D11", "D13", "D10", "D6", "D9" };

// Digit 1 is the leftmost, the one at display pins 11 and 12
const byte DIGIT_PINS[4] = { A2, 8, A1, 4 };
const byte DIGIT_CC_PIN[4] = { 12, 9, 8, 6 };
const char *DIGIT_NANO[4] = { "A2", "D8", "A1", "D4" };

const byte SEGMENT_BLANK = 0x00;
const byte SEGMENT_DP = 0x80;

const byte FONT[10] = {
  0x3F,  // 0  abcdef
  0x06,  // 1  bc
  0x5B,  // 2  abdeg
  0x4F,  // 3  abcdg
  0x66,  // 4  bcfg
  0x6D,  // 5  acdfg
  0x7D,  // 6  acdefg
  0x07,  // 7  abc
  0x7F,  // 8  abcdefg
  0x6F   // 9  abcdfg
};

// What each digit should be showing, one pattern per digit
byte displayBuffer[4] = { SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK };

byte scanIndex = 0;


void writeSegments(byte pattern) {

  for (byte i = 0; i < 7; i++) {
    digitalWrite(SEG_PINS[i], (pattern >> i) & 1);
  }

  digitalWrite(SEG_DP, (pattern & SEGMENT_DP) ? HIGH : LOW);
}


// One digit of one frame. The digit being left is blanked before
// the segments change, so nothing half lit shows through.
void scanOnce() {

  digitalWrite(DIGIT_PINS[scanIndex], LOW);

  scanIndex = (scanIndex + 1) & 3;

  writeSegments(displayBuffer[scanIndex]);

  digitalWrite(DIGIT_PINS[scanIndex], HIGH);

  delayMicroseconds(900);
}


// Keeps the display going for a while. Every wait goes through
// here, so nothing ever sits on one digit and burns it.
void hold(unsigned long ms) {

  unsigned long start = millis();

  while (millis() - start < ms) {
    scanOnce();
  }
}


void fill(byte pattern) {

  for (byte i = 0; i < 4; i++) {
    displayBuffer[i] = pattern;
  }
}


void blankAndSettle() {

  fill(SEGMENT_BLANK);
  hold(300);
}


void digitTest() {

  Serial.println(F(""));
  Serial.println(F("--- digits: each on its own, in its own place ---"));

  for (byte d = 0; d < 4; d++) {

    fill(SEGMENT_BLANK);
    displayBuffer[d] = FONT[d + 1];

    Serial.print(F("digit "));
    Serial.print(d + 1);
    Serial.print(F("  CC pin "));
    Serial.print(DIGIT_CC_PIN[d]);
    Serial.print(F("  Q"));
    Serial.print(d + 1);
    Serial.print(F(" base 1k from "));
    Serial.print(DIGIT_NANO[d]);
    Serial.print(F("  -> a '"));
    Serial.print(d + 1);
    Serial.println(F("' here and nowhere else"));

    hold(700);
  }

  blankAndSettle();
}


void segmentTest() {

  Serial.println(F(""));
  Serial.println(F("--- segments: each on its own, on all four digits ---"));

  for (byte s = 0; s < 8; s++) {

    fill((byte)(1 << s));

    Serial.print(F("segment "));
    Serial.print(SEG_NAME[s]);
    Serial.print(F("  display pin "));
    Serial.print(SEG_DISPLAY_PIN[s]);
    Serial.print(F("  330R to "));
    Serial.print(SEG_NANO[s]);

    if (s == 7) {
      Serial.println(F("  -> all four decimal points"));
    } else {
      Serial.println(F("  -> that one bar, on all four digits"));
    }

    hold(600);
  }

  blankAndSettle();
}


void digitOrderTest() {

  Serial.println(F(""));
  Serial.println(F("--- order: the display should read 1234, left to right ---"));

  for (byte i = 0; i < 4; i++) {
    displayBuffer[i] = FONT[i + 1];
  }

  hold(2000);

  blankAndSettle();
}


void pointTest() {

  Serial.println(F(""));
  Serial.println(F("--- decimal point: one position at a time ---"));

  for (byte d = 0; d < 4; d++) {

    fill(SEGMENT_BLANK);

    // Something to sit beside, so the point is easy to judge
    displayBuffer[d] = FONT[8] | SEGMENT_DP;

    Serial.print(F("digit "));
    Serial.print(d + 1);
    Serial.print(F(" ('"));
    Serial.print(d + 1);
    Serial.println(F("') should have the point lit"));

    hold(500);
  }

  blankAndSettle();
}


void allOnTest() {

  Serial.println(F(""));
  Serial.println(F("--- everything at once: 8.8.8.8. ---"));

  fill(FONT[8] | SEGMENT_DP);
  hold(2500);

  blankAndSettle();

  Serial.println(F(""));
  Serial.println(F("--- nothing lit, display dark ---"));
  hold(800);
}


void setup() {

  Serial.begin(9600);

  for (byte i = 0; i < 8; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
  }

  for (byte d = 0; d < 4; d++) {
    pinMode(DIGIT_PINS[d], OUTPUT);
    digitalWrite(DIGIT_PINS[d], LOW);
  }

  // Give the serial monitor time to open
  hold(1200);

  Serial.println(F(""));
  Serial.println(F("Nano display test, CC56-12EWA on a breadboard"));
  Serial.println(F("All four emitters to ground, or nothing lights."));

  digitTest();
  segmentTest();
  digitOrderTest();
  pointTest();
  allOnTest();

  Serial.println(F(""));
  Serial.println(F("Check done. Counting 0000 to 9999."));
  Serial.println(F("Press reset to run the check again."));
}


void loop() {

  unsigned long start = millis();

  int value = (int)((start / 1000) % 10000);

  displayBuffer[0] = FONT[value / 1000];
  displayBuffer[1] = FONT[(value / 100) % 10];
  displayBuffer[2] = FONT[(value / 10) % 10];
  displayBuffer[3] = FONT[value % 10];

  while (millis() - start < 1000) {
    scanOnce();
  }
}
