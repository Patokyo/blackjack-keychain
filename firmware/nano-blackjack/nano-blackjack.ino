/*
  BLACKJACK STRATEGY TRAINER

  Hardware: Arduino Nano, ATmega328P at 16 MHz, 5 V from USB, with
  the keychain board's parts wired up on a breadboard.

  This is the keychain game (firmware/blackjack-keychain) running on
  a Nano. The rules, the timing, the display behaviour and the
  EEPROM layout are all the same. What differs is the pin map, and
  two LEDs that no longer have a pin to sit on.

  No serial. Everything the game knows is on the four digit
  display and the five LEDs, and every input is a button.

  Pin map. A Nano brings out 22 pins, but D0 and D1 belong to its
  USB serial chip and A6 and A7 are analogue inputs with no port
  bit at all, so 20 pins are really available. The keychain board
  asks for 22: twelve for the display, seven LEDs, the two button
  ladders and the mode switch. Two of those have to go.

  What goes is the correct LED and the wrong LED. The pair, soft,
  win, push and lose LEDs all stay. Train mode still grades every
  decision, it just cannot light a lamp for it: setCorrectLed()
  and setWrongLed() below are empty for that reason, and they are
  the only things to fill in if extra outputs ever turn up. A
  wrong answer still says what the book wanted on the display, and
  the accuracy is still there behind the bank button.

  What moves is the two button ladders, from A4 and A5 to A6 and
  A7. A ladder is only ever read, and analogRead() works on A6 and
  A7 even though nothing else can be done with those two pins, so
  that frees A4 and A5 to be ordinary outputs for the last LEDs.

    D12  segment a          A2  digit 1, Q1
    D5   segment b          D8  digit 2, Q2
    D7   segment c          A1  digit 3, Q3
    D11  segment d          D4  digit 4, Q4
    D13  segment e
    D10  segment f          D2  pair LED
    D6   segment g          D3  soft LED
    D9   segment dp         A0  win LED
                            A3  lose LED
    A4   push LED
    A5   mode switch, +5 V for play, GND for train
    A6   hit / stand / double ladder
    A7   split / bank ladder

  That is every usable pin spoken for: fifteen outputs (twelve for
  the display, five LEDs), A5 as the mode switch, and A6 and A7 as
  the two analogue inputs. Nothing is left over.

  D0 and D1 are deliberately left alone. D1 is driven by the USB
  serial chip, so an output there would fight it, and nothing in
  the sketch wants the port anyway. It stays free for flashing.

  Digits run left to right as 1 to 4, which is what the CC56-12EWA
  calls its common cathodes: CC1 is pad 12 of the display and CC4
  is pad 6, so Q1 on the leftmost digit and Q4 on the rightmost.

  Build for "Arduino Nano" (ATmega328P, 16 MHz) with the bootloader
  the board came with. Nothing here depends on the clock: every
  wait is in milliseconds, so the game runs at the speed it does on
  the keychain.

  Breadboard wiring:
  - segments a to dp, each through 330R, to D12 D5 D7 D11 D13 D10
    D6 D9 in that order
  - display commons CC1 to CC4 to the collectors of four NPNs, the
    base of each through 1k to A2 D8 A1 D4, all emitters to ground
  - each LED from its pin through 330R to ground
  - ladders: 10k from A6 and A7 to ground, +5 V through 1k, 3.3k
    and 6.8k to A6, through 2.2k and 6.8k to A7
  - mode switch, SPDT, +5 V and ground to A5

  PLAY MODE - mode switch to +5 V

  Digits 1 and 2 show the player's total for the hand in play.
  Digits 3 and 4 show the dealer's up card, then the dealer's
  running total once the hole card is turned over.

  The soft LED is lit while an ace is being counted as 11, and the
  pair LED while the first two cards match.

  Play mode does not grade anything. The correct and wrong LEDs
  stay dark and no advice is given: that is what train mode is for.

  Once the player stands the dealer turns its hole card over and
  draws a card every half second, so the total climbs where the
  player can watch it.

  Win, push or lose lights at the end of the hand, and the bankroll
  then blinks on all four digits for half a second before the next
  hand is dealt.

  Coming into play mode, at power up or back from train mode, the
  display shows HIGH and then the best bank so far before the
  first hand is dealt.

  Hold the bank button at any time, in any part of the game, to
  take the display over and show the bankroll. Keep holding past
  a second and it starts flashing HIGH over the best bank, over
  and over, until the button is let go.

  A hand costs $10, but a shorter bank is not the end of the run:
  with less than $10 left the whole balance goes on the next hand,
  so $5 can be bet back up to $10. Only a completely empty bank
  stops the game.

  The bankroll itself is kept in EEPROM after every hand, so
  switching the keychain off carries the run over to the next
  power up. A run that ended wiped out, or on the 9999 jackpot,
  comes back at $100 when it is next powered on, both of which
  are treated as a finished run. A bankroll better than the best
  one so far is written too.

  An empty bank lights the lose LED and stops the game. Press any
  button and it starts again from $100.

  Four digits cannot show a bank bigger than 9999, and reaching
  that is worth a party: every LED starts flashing at random and
  nothing else happens until the chip is reset.

  TRAIN MODE - mode switch to GND

  The same hands with no bankroll. Nothing costs anything, so a
  double or a split is always offered when the cards allow one.

  Every decision is graded. A wrong one puts the action the book
  wanted on the display for half a second before the hand carries
  on, as HIT, STND, DOUB or SPLT. There is no correct or wrong LED
  on this build, so a right answer passes silently: the absence of
  a word, and the accuracy behind the bank button, are the only
  signs of it.

  The bank button shows the running accuracy instead of a bankroll,
  and the accuracy blinks at the end of each hand in place of the
  bankroll: two digits, a point, two decimals, so 87.50 is 87.5
  percent. A perfect run has no room for two decimals and reads
  100.

  Nothing about accuracy is stored between sessions; it starts
  again from zero each time the switch comes back to GND.

  Rules:
  - 6 deck shoe, reshuffled between hands when it runs low
  - Dealer stands on soft 17
  - Double on any first two cards
  - Double after split allowed
  - Up to 4 hands by splitting (no resplitting aces;
    split aces receive one card only)
  - A two card 21 is a natural blackjack and is not asked to act
  - A natural beats any 21 made with three or more cards
  - Two naturals push; two ordinary 21s push as normal
  - Surrender disabled

  Betting:
  - $100 to start, a fixed $10 on every hand
  - A split hand costs another $10, a double costs another $10
  - Something is only offered when there is money left to cover
    it, so the bankroll can never run below 0
  - Wins pay even money, a natural pays 3:2, pushes return the
    bet
  - Money is counted in cents internally, so the half dollar a
    3:2 natural pays on an odd bet (a $5 natural pays $7.50) is
    kept and played with. The display has four digits and drops
    it, so $12.50 reads as 12 until a hand brings the bank back
    to a whole number of dollars

  Controls:
  H = Hit     (SW1, 1k   on A6)
  S = Stand   (SW2, 3.3k on A6)
  D = Double  (SW3, 6.8k on A6)
  P = Split   (SW4, 2.2k on A7)
  B = Bank    (SW5, 6.8k on A7)   holds the bankroll, or the
                                 accuracy in train mode

  The five buttons are two resistor ladders, not five separate
  pins. Each one switches its pin to +5 V through its own resistor
  against a 10k pull down, so A6 reads three buttons and A7 reads
  two by the voltage it lands on. The divider and the ADC reference
  both come from +5 V, so the readings are ratiometric and do not
  care how the supply sags.

  Press one button at a time on a ladder. Two at once put their
  resistors in parallel and read as a third, meaningless level.

  A decision the hand cannot make, such as a double after a hit or
  a split the bankroll cannot cover, blinks the display and is not
  graded.
*/

#include <EEPROM.h>

const int DECK_COUNT = 6;
const int SHOE_SIZE = 312;

// Start a fresh shoe when fewer than this many cards are left.
// A single round cannot use anywhere near this many cards, so a
// reshuffle never happens mid-round.
const int RESHUFFLE_AT = 40;

// Maximum number of hands one round can be split into
const int MAX_HANDS = 4;

// Card values:
// 1 = Ace
// 2-9 = Number cards
// 10 = 10, Jack, Queen or King

byte shoe[SHOE_SIZE];
int shoePosition = 0;

// Betting. Money is counted in cents, so a 3:2 natural on an odd
// bet is carried exactly rather than rounded away.
const int STARTING_BANKROLL = 10000;   // $100.00
const int BET = 1000;                  // $10.00

// $9999.00: the most four digits can show, and where the egg is
const int MAX_BANK = 999900;

// Two byte high score, little endian, at the bottom of the EEPROM,
// then the bankroll, its complement and a format tag right after it
const int HIGHSCORE_ADDRESS = 0;
const int BANKROLL_ADDRESS = 2;

// Bumped whenever the stored meaning changes. This one counts cents
// and pays 3:2, so a balance left by an older build is ignored.
const byte BANKROLL_VERSION = 2;

int bankroll = STARTING_BANKROLL;
int highscore = 0;

// What is already in EEPROM, so a hand that changed nothing costs
// no write at all
int savedBankroll = STARTING_BANKROLL;

// Train mode score, one count per decision. Not stored anywhere
// and reset every time a train session starts.
int correctDecisions = 0;
int totalDecisions = 0;

// Latched from the mode switch at the start of each hand
bool trainingMode = false;
bool modeKnown = false;

// Player actions
enum Action {
  HIT,
  STAND,
  DOUBLE,
  SPLIT
};

// End of hand result
enum Outcome {
  OUTCOME_NONE,
  OUTCOME_WIN,
  OUTCOME_PUSH,
  OUTCOME_LOSE
};

// A hand is kept as "every ace counts as 1" plus how many aces
// it holds, so no list of cards has to be stored.
struct Hand {
  byte hard;       // total with aces counted as 1
  byte aces;       // number of aces in the hand
  byte count;      // number of cards in the hand
  byte first;      // value of the first card
  int wager;       // money riding on the hand, 0 for the dealer
  byte pairValue;  // matching value if the hand is a pair, else 0
  bool aceSplit;   // came from splitting aces: one card only
  bool fromSplit;  // came off a split, so 21 is not a natural
};


// --------------------------------------------------
// CARD SHOE
// --------------------------------------------------

void initialiseShoe() {

  int index = 0;

  for (int deck = 0; deck < DECK_COUNT; deck++) {

    // Four cards of each value from Ace to 9
    for (byte value = 1; value <= 9; value++) {

      for (int i = 0; i < 4; i++) {
        shoe[index++] = value;
      }
    }

    // Sixteen 10-value cards per deck
    for (int i = 0; i < 16; i++) {
      shoe[index++] = 10;
    }
  }

  // Fisher-Yates shuffle
  for (int i = SHOE_SIZE - 1; i > 0; i--) {

    int j = random(i + 1);

    byte temp = shoe[i];
    shoe[i] = shoe[j];
    shoe[j] = temp;
  }

  shoePosition = 0;
}


bool shoeIsLow() {

  return (SHOE_SIZE - shoePosition) < RESHUFFLE_AT;
}


// Draw a card from the shoe, removing it permanently
byte drawCard() {

  if (shoePosition >= SHOE_SIZE) {

    initialiseShoe();
  }

  return shoe[shoePosition++];
}


// --------------------------------------------------
// HAND CALCULATIONS
// --------------------------------------------------

void clearHand(Hand &hand) {

  hand.hard = 0;
  hand.aces = 0;
  hand.count = 0;
  hand.first = 0;
  hand.wager = 0;
  hand.pairValue = 0;
  hand.aceSplit = false;
  hand.fromSplit = false;
}


void addCard(Hand &hand, byte card) {

  if (hand.count == 0) {
    hand.first = card;
  }

  hand.hard += card;

  if (card == 1) {
    hand.aces++;
  }

  hand.count++;

  if (hand.count == 2) {

    // A pair only exists on the first two cards
    hand.pairValue = (card == hand.first) ? card : 0;

  } else if (hand.count > 2) {

    hand.pairValue = 0;
  }
}


// Best total: count one ace as 11 where that does not bust
byte handTotal(const Hand &hand) {

  byte total = hand.hard;

  if (hand.aces > 0 && total + 10 <= 21) {
    total += 10;
  }

  return total;
}


// True when an ace is currently being played as 11
bool handIsSoft(const Hand &hand) {

  return hand.aces > 0 && hand.hard + 10 <= 21;
}


bool handIsBust(const Hand &hand) {

  return handTotal(hand) > 21;
}


// A natural is an ace plus a ten value card in the first two cards.
// 21 on three or more cards, or 21 after a split, is not a natural.
bool handIsNatural(const Hand &hand) {

  return hand.count == 2 && !hand.fromSplit && handTotal(hand) == 21;
}


// An ace at the top of the dealer's hand reads as 11
byte cardValue(byte card) {

  return (card == 1) ? 11 : card;
}


bool canDouble(const Hand &hand) {

  return hand.count == 2;
}


bool canSplit(const Hand &hand, int handCount) {

  return hand.pairValue != 0 && handCount < MAX_HANDS && !hand.aceSplit;
}


// What one bet costs this round. The bank can fall below a full
// bet, and then the whole of it goes on the table: $5 staked is
// better than $5 sat out. Train mode has no bank, so it always
// plays full bets.
int betSize() {

  if (trainingMode) {
    return BET;
  }

  return (bankroll < BET) ? bankroll : BET;
}


// Money not already riding on the table this round. A double or a
// split needs another bet out of this, which is what stops the
// bankroll from running below 0.
int moneyLeft(const Hand hands[], int handCount) {

  int committed = 0;

  for (int i = 0; i < handCount; i++) {
    committed += hands[i].wager;
  }

  return bankroll - committed;
}


// --------------------------------------------------
// BASIC STRATEGY
// Six decks, S17, DAS, no surrender
// --------------------------------------------------

bool shouldSplit(byte pair, byte up) {

  if (pair == 1 || pair == 8) return true;   // always split aces and 8s
  if (pair == 5 || pair == 10) return false; // never split 5s or 10s

  if (pair == 4) return (up == 5 || up == 6);

  if (pair == 6) return (up >= 2 && up <= 6);

  if (pair == 9) {
    return (up >= 2 && up <= 6) || (up >= 8 && up <= 9);
  }

  // 2s, 3s and 7s split against 2 through 7
  return (up >= 2 && up <= 7);
}


Action bestAction(const Hand &hand, byte dealerUp, int handCount) {

  // An ace up is worth 11
  byte up = cardValue(dealerUp);

  if (canSplit(hand, handCount) && shouldSplit(hand.pairValue, up)) {
    return SPLIT;
  }

  byte total = handTotal(hand);
  Action action;

  if (handIsSoft(hand)) {

    // SOFT HANDS (ace counted as 11)
    if (total >= 19) return STAND;

    if (total == 18) {

      if (up >= 3 && up <= 6) action = DOUBLE;
      else if (up == 2 || up == 7 || up == 8) return STAND;
      else return HIT;

    } else if (total == 17) {

      action = (up >= 3 && up <= 6) ? DOUBLE : HIT;

    } else if (total == 15 || total == 16) {

      action = (up >= 4 && up <= 6) ? DOUBLE : HIT;

    } else if (total == 13 || total == 14) {

      action = (up == 5 || up == 6) ? DOUBLE : HIT;

    } else {

      return HIT;
    }

  } else {

    // HARD HANDS
    if (total >= 17) return STAND;

    if (total >= 13) return (up >= 2 && up <= 6) ? STAND : HIT;

    if (total == 12) return (up >= 4 && up <= 6) ? STAND : HIT;

    if (total == 11) return DOUBLE;

    if (total == 10) return (up <= 9) ? DOUBLE : HIT;

    if (total == 9) return (up >= 3 && up <= 6) ? DOUBLE : HIT;

    return HIT;
  }

  // A double is only available on the first two cards
  if (action == DOUBLE && !canDouble(hand)) {
    return HIT;
  }

  return action;
}


// --------------------------------------------------
// PIN MAP
// One line per pin in use, as wired on the breadboard. The wiring
// is fixed once it is built, so these are constants rather than
// settings.
// --------------------------------------------------

// Mode switch, +5 V for play, GND for train. A plain digital input
// on a pin that also happens to be an analogue one.
const int MODE_PIN = A5;

// Button ladders. Read with analogRead(), which is all A6 and A7
// can do: they have no digital port bit behind them.
const int BTN_MAIN_PIN = A6;   // hit, stand, double
const int BTN_SIDE_PIN = A7;   // split, bank

// LEDs, all active high through 330R to ground
const int LED_PAIR  = 2;
const int LED_SOFT  = 3;
const int LED_WIN   = A0;
const int LED_LOSE  = A3;
const int LED_PUSH  = A4;

// The grading LEDs are the pair the Nano has no pin for, so these
// two are empty. Every call site still runs, which is what they
// would light again if extra outputs ever turned up.
void setCorrectLed(bool) {}
void setWrongLed(bool) {}

// The other two indicators drive their pin directly
void setPairLed(bool on) { digitalWrite(LED_PAIR, on); }
void setSoftLed(bool on) { digitalWrite(LED_SOFT, on); }

// Four digit display U2, CC56-12EWA, common cathode, with one NPN
// low side switch per digit. Segments and digit drives are all
// active high. Segments a to g are shared by all four digits, and
// the single decimal point is shared with them.
const int SEG_A  = 12;
const int SEG_B  = 5;
const int SEG_C  = 7;
const int SEG_D  = 11;
const int SEG_E  = 13;
const int SEG_F  = 10;
const int SEG_G  = 6;
const int SEG_DP = 9;

// One low side switch per digit, Q1 leftmost through Q4 rightmost
const int DIGIT_1 = A2;  // Q1
const int DIGIT_2 = 8;   // Q2
const int DIGIT_3 = A1;  // Q3
const int DIGIT_4 = 4;   // Q4

// Segment order for the font table below
const byte SEG_PIN[7] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G };

// Button ladder levels. Each button pulls its ladder pin up through
// its own resistor against a 10k pull down, so the ADC sees
// 10k/(10k+R) of full scale:
//
//   hit    1k    -> 930     split  2.2k -> 839
//   stand  3.3k  -> 769     bank   6.8k -> 609
//   double 6.8k  -> 609     nothing     -> 0
//
// Both the divider and the ADC reference come from +5 V, so these
// counts hold however the supply sags.
const byte LEVEL_NONE = 0;
const byte LEVEL_6K8  = 1;   // double on A6, bank on A7
const byte LEVEL_3K3  = 2;   // stand on A6
const byte LEVEL_2K2  = 3;   // split on A7
const byte LEVEL_1K   = 4;   // hit on A6


// --------------------------------------------------
// DISPLAY
// The four digits are multiplexed: one digit is lit at a time and
// the four are cycled fast enough to look steady. Every wait in
// the sketch goes through pause(), which keeps the cycling going,
// so holding a button or watching the dealer draw never blanks or
// flickers the display.
// --------------------------------------------------

// Segment patterns: bit 0 is a through bit 6 is g, and bit 7 is
// the decimal point, which has a pin of its own
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

// Enough letters for the four action words. A seven segment cell
// only ever draws a rough letter, so these are the usual shapes.
const byte LETTER_B = 0x7C;   // c d e f g
const byte LETTER_D = 0x5E;   // b c d e g
const byte LETTER_G = 0x3D;   // a c d e f, a C that closes
const byte LETTER_H = 0x76;   // b c e f g
const byte LETTER_I = 0x06;   // b c
const byte LETTER_L = 0x38;   // d e f
const byte LETTER_N = 0x54;   // c e g
const byte LETTER_O = 0x3F;   // a b c d e f
const byte LETTER_P = 0x73;   // a b e f g
const byte LETTER_S = 0x6D;   // a c d f g
const byte LETTER_T = 0x78;   // d e f g
const byte LETTER_U = 0x3E;   // b c d e f

// The label shown when the high score is on the display
const byte HIGH_WORD[4] = { LETTER_H, LETTER_I, LETTER_G, LETTER_H };

// What the game wants shown, one segment pattern per digit
byte displayBuffer[4] = { SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK };

// What the bank button shows, rebuilt whenever it is sampled
byte holdDigits[4] = { SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK };

byte refreshIndex = 0;
byte refreshTicks = 0;
bool bankShown = false;
unsigned long bankHeldSince = 0;


void writeSegments(byte pattern) {

  for (byte i = 0; i < 7; i++) {
    digitalWrite(SEG_PIN[i], (pattern >> i) & 1);
  }

  // The point is on its own pin, not part of the seven
  digitalWrite(SEG_DP, (pattern & SEGMENT_DP) ? HIGH : LOW);
}


void digitDrive(byte index, bool on) {

  if (index == 0) digitalWrite(DIGIT_1, on);
  if (index == 1) digitalWrite(DIGIT_2, on);
  if (index == 2) digitalWrite(DIGIT_3, on);
  if (index == 3) digitalWrite(DIGIT_4, on);
}


// Writes value into width digits at offset, right justified with
// blank leading digits. Only the requested digits are touched.
void numberToDigits(byte *out, byte offset, byte width, int value) {

  for (byte i = 0; i < width; i++) {
    out[offset + i] = SEGMENT_BLANK;
  }

  if (value < 0) value = 0;

  byte i = width;

  while (i > 0) {

    i--;

    out[offset + i] = FONT[value % 10];
    value /= 10;

    if (value == 0) break;
  }
}


// Accuracy as a percentage with two decimals, 00.00 to 99.99.
// A flawless run would need five digits plus the point, so 100
// is shown as 100. with the last digit blank.
void accuracyToDigits(byte *out) {

  int hundredths = 0;

  if (totalDecisions > 0) {
    hundredths = ((long)correctDecisions * 10000) / totalDecisions;
  }

  if (hundredths >= 10000) {

    out[0] = FONT[1];
    out[1] = FONT[0];
    out[2] = FONT[0] | SEGMENT_DP;
    out[3] = SEGMENT_BLANK;

    return;
  }

  out[0] = FONT[hundredths / 1000];
  out[1] = FONT[(hundredths / 100) % 10] | SEGMENT_DP;
  out[2] = FONT[(hundredths / 10) % 10];
  out[3] = FONT[hundredths % 10];
}


// What holding the bank button shows: the money in play mode, the
// accuracy in train mode
void fillHoldView(byte *out) {

  if (trainingMode) {
    accuracyToDigits(out);
  } else {
    // Cents in, dollars out: the half dollar is kept, not shown
    numberToDigits(out, 0, 4, bankroll / 100);
  }
}


// A long hold on the bank button moves on from the bankroll to the
// best bank: HIGH for 200 ms, the score for 200 ms, round again, for
// as long as the button is down. Train mode has no high score, so
// there the hold stays on the accuracy.
bool holdWantsHighWord() {

  if (trainingMode) {
    return false;
  }

  unsigned long held = millis() - bankHeldSince;

  if (held < 1000) {
    return false;
  }

  return (((held - 1000) / 200) % 2) == 0;
}


void refreshDisplay() {

  // Sample the bank button now and then. Holding it takes the
  // display over from whatever the game was showing.
  if ((refreshTicks++ & 7) == 0) {

    bool down = (readLadder(BTN_SIDE_PIN) == LEVEL_6K8);

    // Start the hold clock on the edge, so the first second is the
    // bankroll and only then does it move on
    if (down && !bankShown) {
      bankHeldSince = millis();
    }

    bankShown = down;

    if (bankShown) {
      fillHoldView(holdDigits);
    }
  }

  // Blank the digit being left before changing the segments, so
  // no half lit digit shows through
  digitDrive(refreshIndex, false);

  refreshIndex = (refreshIndex + 1) & 3;

  if (bankShown) {

    if (holdWantsHighWord()) {
      writeSegments(HIGH_WORD[refreshIndex]);
    } else {
      writeSegments(holdDigits[refreshIndex]);
    }

  } else {

    writeSegments(displayBuffer[refreshIndex]);
  }

  digitDrive(refreshIndex, true);
}


// Every wait in the sketch goes through here
void pause(unsigned int ms) {

  unsigned long start = millis();

  do {

    refreshDisplay();
    delay(1);

  } while (millis() - start < ms);
}


void initialiseDisplay() {

  for (byte i = 0; i < 7; i++) {
    pinMode(SEG_PIN[i], OUTPUT);
  }

  pinMode(SEG_DP, OUTPUT);

  pinMode(DIGIT_1, OUTPUT);
  pinMode(DIGIT_2, OUTPUT);
  pinMode(DIGIT_3, OUTPUT);
  pinMode(DIGIT_4, OUTPUT);

  writeSegments(SEGMENT_BLANK);

  for (byte i = 0; i < 4; i++) {
    digitDrive(i, false);
  }
}


// Player's total on the first two digits, the dealer's value on
// the last two
void showTable(const Hand &hand, int dealerValue) {

  numberToDigits(displayBuffer, 0, 2, handTotal(hand));
  numberToDigits(displayBuffer, 2, 2, dealerValue);
}


void clearDisplay() {

  for (byte i = 0; i < 4; i++) {
    displayBuffer[i] = SEGMENT_BLANK;
  }
}


// Blinks the bankroll, or the accuracy in train mode, for half a
// second
void flashSummary() {

  for (byte i = 0; i < 2; i++) {

    fillHoldView(displayBuffer);
    pause(125);

    clearDisplay();
    pause(125);
  }
}


// On the way into play mode: the label, then the best bank so far
void showHighScore() {

  clearDisplay();

  for (byte i = 0; i < 4; i++) {
    displayBuffer[i] = HIGH_WORD[i];
  }

  pause(200);

  numberToDigits(displayBuffer, 0, 4, highscore / 100);

  pause(200);
}


// The word for the action the book wanted, left justified
void showActionWord(Action action) {

  clearDisplay();

  if (action == HIT) {

    displayBuffer[0] = LETTER_H;
    displayBuffer[1] = LETTER_I;
    displayBuffer[2] = LETTER_T;

  } else if (action == STAND) {

    displayBuffer[0] = LETTER_S;
    displayBuffer[1] = LETTER_T;
    displayBuffer[2] = LETTER_N;
    displayBuffer[3] = LETTER_D;

  } else if (action == DOUBLE) {

    displayBuffer[0] = LETTER_D;
    displayBuffer[1] = LETTER_O;
    displayBuffer[2] = LETTER_U;
    displayBuffer[3] = LETTER_B;

  } else {

    displayBuffer[0] = LETTER_S;
    displayBuffer[1] = LETTER_P;
    displayBuffer[2] = LETTER_L;
    displayBuffer[3] = LETTER_T;
  }
}


// Two quick blinks: this action was not allowed, nothing changes
void rejectAction() {

  byte saved[4];

  for (byte i = 0; i < 4; i++) {
    saved[i] = displayBuffer[i];
  }

  for (byte i = 0; i < 2; i++) {

    clearDisplay();
    pause(80);

    for (byte d = 0; d < 4; d++) {
      displayBuffer[d] = saved[d];
    }

    pause(80);
  }
}


// --------------------------------------------------
// LEDS
// --------------------------------------------------

void initialiseLeds() {

  pinMode(LED_PAIR, OUTPUT);
  pinMode(LED_SOFT, OUTPUT);
  pinMode(LED_WIN, OUTPUT);
  pinMode(LED_LOSE, OUTPUT);
  pinMode(LED_PUSH, OUTPUT);

  digitalWrite(LED_PAIR, LOW);
  digitalWrite(LED_SOFT, LOW);
  digitalWrite(LED_WIN, LOW);
  digitalWrite(LED_LOSE, LOW);
  digitalWrite(LED_PUSH, LOW);
}


// A bankroll of 9999 is the most four digits can show, so getting
// there sets every LED going on its own coin toss at its own random
// interval. The game never picks up again: only a reset ends it.
void easterEgg() {

  // The light show never ends, so the run is banked as finished
  // here: a reset then comes back playable at 100 instead of
  // dropping straight into the egg all over again
  bankroll = STARTING_BANKROLL;
  saveBankroll();

  // Nothing above 9999 could be displayed anyway
  clearDisplay();
  numberToDigits(displayBuffer, 0, 4, 9999);

  while (true) {

    setPairLed(random(2));
    setSoftLed(random(2));
    digitalWrite(LED_WIN, random(2));
    digitalWrite(LED_LOSE, random(2));
    setCorrectLed(random(2));
    setWrongLed(random(2));

    digitalWrite(LED_PUSH, random(2));

    pause(random(20, 120));
  }
}


void setOutcomeLed(Outcome outcome) {

  digitalWrite(LED_WIN, outcome == OUTCOME_WIN);
  digitalWrite(LED_PUSH, outcome == OUTCOME_PUSH);
  digitalWrite(LED_LOSE, outcome == OUTCOME_LOSE);
}


// --------------------------------------------------
// PLAYER INPUT
// --------------------------------------------------

// The levels themselves are with the pin map, because the display
// reads the bank level too
void initialiseInputs() {

  // Plain inputs, the pull downs are on the breadboard
  pinMode(BTN_MAIN_PIN, INPUT);
  pinMode(BTN_SIDE_PIN, INPUT);

  // The mode switch always drives its pin to a rail. It is a
  // digital input, even though A5 can also be read as an analogue
  // one.
  pinMode(MODE_PIN, INPUT);
}


bool inPlayMode() {

  return digitalRead(MODE_PIN) == HIGH;
}


byte ladderLevel(int reading) {

  // Band edges sit halfway between the levels listed above
  if (reading < 300) return LEVEL_NONE;
  if (reading < 690) return LEVEL_6K8;
  if (reading < 800) return LEVEL_3K3;
  if (reading < 880) return LEVEL_2K2;

  return LEVEL_1K;
}


byte readLadder(int pin) {

  return ladderLevel(analogRead(pin));
}


// Sample until the level has held steady, so contact bounce on the
// way down does not read as a different button
byte settledLevel(int pin) {

  byte last = readLadder(pin);
  byte same = 0;

  while (same < 4) {

    pause(3);

    byte now = readLadder(pin);

    if (now == last) {
      same++;
    } else {
      last = now;
      same = 0;
    }
  }

  return last;
}


// Hold until the button is let go again, which also rides out
// any contact bounce on the way up
void waitForRelease(int pin) {

  while (readLadder(pin) != LEVEL_NONE) {
    pause(5);
  }

  pause(20);
}


bool anyButtonDown() {

  return readLadder(BTN_MAIN_PIN) != LEVEL_NONE ||
         readLadder(BTN_SIDE_PIN) != LEVEL_NONE;
}


// Returns the button being pressed, or 0 when none is
char readButton() {

  byte main = settledLevel(BTN_MAIN_PIN);

  if (main != LEVEL_NONE) {

    waitForRelease(BTN_MAIN_PIN);

    if (main == LEVEL_1K)  return 'H';
    if (main == LEVEL_3K3) return 'S';
    if (main == LEVEL_6K8) return 'D';

    return 0;
  }

  byte side = settledLevel(BTN_SIDE_PIN);

  if (side != LEVEL_NONE) {

    waitForRelease(BTN_SIDE_PIN);

    if (side == LEVEL_2K2) return 'P';
    if (side == LEVEL_6K8) return 'B';

    return 0;
  }

  return 0;
}


// Waits for one of the four action buttons. The bank button is not
// an action: holding it only changes what the display shows, and
// refreshDisplay already handles that, so it is swallowed here.
Action waitForAction() {

  while (true) {

    char button = readButton();

    if (button == 'H') return HIT;
    if (button == 'S') return STAND;
    if (button == 'D') return DOUBLE;
    if (button == 'P') return SPLIT;
  }
}


// --------------------------------------------------
// SPLITTING
// --------------------------------------------------

void splitHand(Hand hands[], int index, int &handCount) {

  byte pair = hands[index].pairValue;

  // The second card of the pair becomes its own one card hand
  Hand newHand;
  clearHand(newHand);
  addCard(newHand, pair);
  newHand.wager = betSize();

  // Take that card back off the original hand
  hands[index].hard -= pair;
  hands[index].count = 1;
  hands[index].pairValue = 0;
  hands[index].first = pair;

  // One ace went to the new hand
  hands[index].aces = (pair == 1) ? 1 : 0;

  // Make room for the new hand
  for (int i = handCount; i > index + 1; i--) {
    hands[i] = hands[i - 1];
  }

  hands[index + 1] = newHand;

  // Neither half of a split can make a natural
  hands[index].fromSplit = true;
  hands[index + 1].fromSplit = true;

  if (pair == 1) {

    // Split aces get one card each and nothing more
    hands[index].aceSplit = true;
    hands[index + 1].aceSplit = true;
  }

  handCount++;
}


// --------------------------------------------------
// EEPROM
// --------------------------------------------------

// The bankroll is stored with its complement behind a format tag. A
// write cut short by a power loss can only damage one byte, so a
// damaged save reads back as a mismatch instead of a wrong balance.
void saveBankroll() {

  EEPROM.put(BANKROLL_ADDRESS, BANKROLL_VERSION);
  EEPROM.put(BANKROLL_ADDRESS + 1, bankroll);
  EEPROM.put(BANKROLL_ADDRESS + 3, (int)~bankroll);

  savedBankroll = bankroll;
}


// -1 when nothing trustworthy is stored, which is what a blank chip,
// a half written pair and an older build's balance all look like
int loadBankroll() {

  byte version = 0;
  int value = 0;
  int checkValue = 0;

  EEPROM.get(BANKROLL_ADDRESS, version);
  EEPROM.get(BANKROLL_ADDRESS + 1, value);
  EEPROM.get(BANKROLL_ADDRESS + 3, checkValue);

  if (version != BANKROLL_VERSION) {
    return -1;
  }

  if (checkValue != (int)~value) {
    return -1;
  }

  return value;
}


// --------------------------------------------------
// PLAY ONE ROUND
// --------------------------------------------------

// Out of money: the lose LED stays on and nothing happens until a
// button is pressed, which starts a fresh bank
void waitForRestart() {

  clearDisplay();
  numberToDigits(displayBuffer, 0, 4, 0);

  setOutcomeLed(OUTCOME_LOSE);
  setSoftLed(false);
  setPairLed(false);
  setCorrectLed(LOW);
  setWrongLed(LOW);

  // The press and its release both have to finish first
  while (!anyButtonDown()) {
    pause(20);
  }

  while (anyButtonDown()) {
    pause(20);
  }

  bankroll = STARTING_BANKROLL;
  saveBankroll();

  setOutcomeLed(OUTCOME_NONE);
}


void playRound() {

  if (shoeIsLow()) {
    initialiseShoe();
  }

  // Switching back from train mode with an empty bank
  if (!trainingMode && bankroll <= 0) {
    waitForRestart();
  }

  // New round, no result showing yet
  setOutcomeLed(OUTCOME_NONE);
  setCorrectLed(LOW);
  setWrongLed(LOW);

  Hand hands[MAX_HANDS];
  int handCount = 1;

  clearHand(hands[0]);

  byte dealerUp = drawCard();
  byte dealerHole = drawCard();

  addCard(hands[0], drawCard());
  addCard(hands[0], drawCard());

  hands[0].wager = betSize();

  // Only the visible card is known until the hole card is turned
  Hand dealer;
  clearHand(dealer);
  addCard(dealer, dealerUp);

  int current = 0;

  while (current < handCount) {

    // A freshly split hand holds one card and needs a second
    if (hands[current].count == 1) {

      addCard(hands[current], drawCard());

      showTable(hands[current], cardValue(dealerUp));
      setSoftLed(handIsSoft(hands[current]));
      setPairLed(hands[current].pairValue != 0);
      pause(500);

      // Split aces are played with that single card
      if (hands[current].aceSplit) {
        current++;
        continue;
      }
    }

    // A natural blackjack has nothing to decide
    if (handIsNatural(hands[current])) {

      showTable(hands[current], cardValue(dealerUp));
      setSoftLed(handIsSoft(hands[current]));
      setPairLed(false);
      pause(700);

      current++;
      continue;
    }

    bool restartHand = false;

    while (true) {

      showTable(hands[current], cardValue(dealerUp));
      setSoftLed(handIsSoft(hands[current]));
      setPairLed(hands[current].pairValue != 0);

      Action chosen = waitForAction();

      // Reject actions this hand cannot make, without grading them.
      // Train mode has no bankroll, so there only the cards can say
      // no; in play mode the money has to be there as well.
      if (chosen == DOUBLE &&
          (!canDouble(hands[current]) ||
           (!trainingMode && moneyLeft(hands, handCount) < betSize()))) {

        rejectAction();
        continue;
      }

      if (chosen == SPLIT &&
          (!canSplit(hands[current], handCount) ||
           (!trainingMode && moneyLeft(hands, handCount) < betSize()))) {

        rejectAction();
        continue;
      }

      // Play mode takes the decision at face value and says nothing
      // about it. Train mode grades every decision and shows the
      // action the book wanted when the answer is wrong. The correct
      // and wrong LEDs are not fitted on this build, so a right
      // answer is graded silently.
      if (trainingMode) {

        Action correct = bestAction(hands[current], dealerUp, handCount);
        bool correctChoice = (chosen == correct);

        totalDecisions++;

        if (correctChoice) {
          correctDecisions++;
        }

        setCorrectLed(correctChoice);
        setWrongLed(!correctChoice);

        if (!correctChoice) {

          showActionWord(correct);
          pause(500);
        }
      }

      if (chosen == STAND) {
        break;
      }

      if (chosen == SPLIT) {

        splitHand(hands, current, handCount);
        restartHand = true;
        break;
      }

      // Doubling puts another bet on the hand
      if (chosen == DOUBLE) {
        hands[current].wager += betSize();
      }

      // Hit or double both draw one card
      addCard(hands[current], drawCard());

      showTable(hands[current], cardValue(dealerUp));
      setSoftLed(handIsSoft(hands[current]));
      setPairLed(hands[current].pairValue != 0);
      pause(350);

      if (handIsBust(hands[current])) {
        pause(400);
        break;
      }

      if (handTotal(hands[current]) == 21) {
        pause(400);
        break;
      }

      if (chosen == DOUBLE) {
        break;
      }
    }

    if (!restartHand) {
      current++;
    }
  }

  // --------------------------------------------------
  // DEALER'S TURN
  // --------------------------------------------------

  // Standing ends the hand the instant the decision is made, so a
  // train session holds the grade up long enough to be seen before
  // the dealer starts
  if (trainingMode) {
    pause(500);
  }

  setCorrectLed(LOW);
  setWrongLed(LOW);
  setSoftLed(false);
  setPairLed(false);

  // Nothing to beat if every hand has already busted. The first
  // two digits keep following the last hand that is still alive.
  bool dealerPlays = false;
  int displayHand = 0;

  for (int i = 0; i < handCount; i++) {

    if (!handIsBust(hands[i])) {

      dealerPlays = true;
      displayHand = i;
    }
  }

  if (dealerPlays) {

    // Turn the hole card over and let the player see it
    addCard(dealer, dealerHole);
    showTable(hands[displayHand], handTotal(dealer));
    pause(600);

    // Dealer stands on all 17s, including soft 17. One card every
    // half second so the total climbs where it can be watched.
    while (handTotal(dealer) < 17) {

      addCard(dealer, drawCard());

      showTable(hands[displayHand], handTotal(dealer));
      pause(500);
    }

  } else {

    addCard(dealer, dealerHole);
  }

  // --------------------------------------------------
  // RESULT
  // --------------------------------------------------

  byte dealerTotal = handTotal(dealer);
  bool dealerNatural = handIsNatural(dealer);

  for (int i = 0; i < handCount; i++) {

    byte playerTotal = handTotal(hands[i]);
    bool playerNatural = handIsNatural(hands[i]);
    int wager = hands[i].wager;

    Outcome outcome = OUTCOME_PUSH;
    int change = 0;

    if (playerTotal > 21) {

      outcome = OUTCOME_LOSE;
      change = -wager;

    } else if (playerNatural && dealerNatural) {

      outcome = OUTCOME_PUSH;

    } else if (dealerNatural) {

      // A natural beats any 21 made with three or more cards
      outcome = OUTCOME_LOSE;
      change = -wager;

    } else if (playerNatural) {

      // A natural beats a dealer 21 made with three or more cards,
      // and pays 3:2. Rounding is down to the cent, which only ever
      // costs half a cent on an odd wager.
      outcome = OUTCOME_WIN;
      change = wager + wager / 2;

    } else if (dealerTotal > 21) {

      outcome = OUTCOME_WIN;
      change = wager;

    } else if (playerTotal > dealerTotal) {

      outcome = OUTCOME_WIN;
      change = wager;

    } else if (playerTotal < dealerTotal) {

      outcome = OUTCOME_LOSE;
      change = -wager;

    } else {

      outcome = OUTCOME_PUSH;
    }

    // Train mode plays for accuracy, not money
    if (!trainingMode) {
      bankroll += change;
    }

    showTable(hands[i], dealerTotal);
    setOutcomeLed(outcome);
    pause(900);
  }

  if (!trainingMode) {

    if (bankroll < 0) {
      bankroll = 0;
    }

    // Four digits is all the display has, and 9999 is where the egg
    // lives, so the bank stops there
    if (bankroll > MAX_BANK) {
      bankroll = MAX_BANK;
    }

    // A better bankroll than ever before is kept for good
    if (bankroll > highscore) {

      highscore = bankroll;
      EEPROM.put(HIGHSCORE_ADDRESS, highscore);
    }

    // Carry the balance over to the next session, unless a push
    // left it exactly where it was
    if (bankroll != savedBankroll) {
      saveBankroll();
    }
  }

  // Easter egg: the bank cannot be beaten, and it never comes back
  if (!trainingMode && bankroll >= MAX_BANK) {
    easterEgg();
  }

  // --------------------------------------------------
  // NEXT HAND, OR OUT OF MONEY
  // --------------------------------------------------

  // Show what is left in the bank, or the accuracy, blinking
  flashSummary();

  if (!trainingMode && bankroll == 0) {
    waitForRestart();
  }
}


// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup() {

  initialiseDisplay();
  initialiseLeds();
  initialiseInputs();

  int storedBank = loadBankroll();

  highscore = 0;
  EEPROM.get(HIGHSCORE_ADDRESS, highscore);

  // A blank chip, a damaged save or a balance from an older build
  // means whatever high score is left behind is in the old units
  // too, so it goes with it
  if (storedBank < 0) {

    highscore = 0;
    EEPROM.put(HIGHSCORE_ADDRESS, highscore);
  }

  // A blank EEPROM reads back as -1
  if (highscore < 0) {
    highscore = 0;
  }

  // Pick up the run where it was left. A run that finished wiped out
  // or on the 9999 jackpot starts over at 100.
  if (storedBank < 0 || storedBank == 0 || storedBank >= MAX_BANK) {

    bankroll = STARTING_BANKROLL;
    saveBankroll();

  } else {

    bankroll = storedBank;
    savedBankroll = bankroll;
  }

  // Seed the random generator. Every pin is in use here, so there
  // is no floating input to sample noise from: the clock is the
  // source. The Nano runs from a crystal and spends a moment in its
  // bootloader first, so the microseconds counted by the time setup
  // runs differ from one power up to the next. A ladder button held
  // down at power up stirs in a little more, since its pin is then
  // driven rather than pulled to ground.
  unsigned long seed = micros();

  for (byte i = 0; i < 8; i++) {
    seed = seed * 31 + analogRead(BTN_MAIN_PIN);
    seed = seed * 31 + analogRead(BTN_SIDE_PIN);
    delay(2);
  }

  randomSeed(seed);

  initialiseShoe();
}


// --------------------------------------------------
// MAIN LOOP
// --------------------------------------------------

void loop() {

  // The switch is read between hands, not in the middle of one
  bool wantTraining = !inPlayMode();
  bool enteredPlay = false;

  if (!modeKnown || wantTraining != trainingMode) {

    modeKnown = true;
    enteredPlay = !wantTraining;
    trainingMode = wantTraining;

    setOutcomeLed(OUTCOME_NONE);
    setCorrectLed(LOW);
    setWrongLed(LOW);

    if (trainingMode) {

      // Every train session starts from nothing
      correctDecisions = 0;
      totalDecisions = 0;
    }
  }

  // HIGH, then the best bank, before the first hand
  if (enteredPlay) {
    showHighScore();
  }

  playRound();
}
