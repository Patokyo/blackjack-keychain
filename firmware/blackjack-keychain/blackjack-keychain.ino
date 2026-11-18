/*
  BLACKJACK STRATEGY TRAINER

  Hardware: ATmega328P-A in the 32 pin TQFP/MLF package, 5 V from
  the USB-C socket, internal 8 MHz RC oscillator, no crystal.

  This is the keychain build. The game itself - the rules, the
  timing, the betting, the grading and the EEPROM layout - is the
  same one firmware/nano-blackjack runs, line for line. What is
  different here is the board, which has a pin for all seven LEDs,
  and the two extra digits of wiring that come with it.

  No serial. Everything the game knows is on the four digit
  display and the seven LEDs, and every input is a button.

  Pin map, taken from blackjack-keychain.kicad_sch and the board it
  produced (pad is the TQFP pad, the second column the port bit, the
  third the Arduino name):

    pad 32  PD2  D2    correct LED
    pad 31  PD1  D1    wrong LED
    pad 30  PD0  D0    SW6, the mode switch, +5 V or GND
    pad  1  PD3  D3    lose LED
    pad  2  PD4  D4    digit 4 driver, Q4
    pad  9  PD5  D5    segment b
    pad 10  PD6  D6    segment g
    pad 11  PD7  D7    segment c
    pad 12  PB0  D8    win LED
    pad 13  PB1  D9    segment dp
    pad 14  PB2  D10   segment f
    pad 15  PB3  D11   segment d   (also ISP MOSI)
    pad 16  PB4  D12   segment a   (also ISP MISO)
    pad 17  PB5  D13   segment e   (also ISP SCK)
    pad 23  PC0  A0    soft LED
    pad 24  PC1  A1    digit 3 driver, Q3
    pad 25  PC2  A2    digit 1 driver, Q1
    pad 26  PC3  A3    pair LED
    pad 27  PC4  A4    hit / stand / double ladder
    pad 28  PC5  A5    split / bank ladder
    pad 29  PC6  RESET, 10k pull-up on the board
    pad  7  PB6  ---   push LED
    pad  8  PB7  ---   digit 2 driver, Q2

  Digits run left to right as 1 to 4, which is what the CC56-12EWA
  calls its common cathodes: CC1 is pad 12 of the display and CC4
  is pad 6, so Q1 on the leftmost digit and Q4 on the rightmost.

  PB6 and PB7 have no Arduino number on a 328P, because a standard
  board spends them on the crystal. Here the chip runs from its
  internal oscillator, so they are free, but they can only be
  reached through PORTB:

    DDRB  |= _BV(6);  PORTB |= _BV(6);    push LED on
    PORTB &= ~_BV(6);                     push LED off

  Never pass 6 or 7 to digitalWrite(). Those numbers are D6 and D7,
  segments g and c, and a "push LED" on 6 would light a segment.

  Clock and fuses. Build for F_CPU = 8000000L: MiniCore's ATmega328
  at "8 MHz internal". CKDIV8 must be unprogrammed, low fuse 0xE2.
  A chip straight from the factory divides the internal oscillator
  down to 1 MHz, which runs the whole game eight times too slowly.

  Burn the chip with no bootloader, so reset goes straight into the
  sketch. Program through the ISP header on J2.

  PLAY MODE - SW6 to +5 V

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
  display shows PLAY, then HIGH and then the best bank so far,
  before the first hand is dealt.

  Hold the bank button at any time, in any part of the game, to
  take the display over and show the bankroll. Keep holding past
  a second and it starts flashing HIGH over the best bank, over
  and over, until the button is let go.

  Before every hand the display shows BET, then the bet itself.
  The bet is the player's to set right there: HIT puts another $5
  on it, STAND takes $5 off, DOUBLE doubles it and SPLIT halves
  it, as often as wanted until it reads right. It never goes below
  $5, never above $100, and never above what the bank holds: a
  press that would take it past any of those limits is refused,
  and answers with the same two quick blinks as any other action
  the game will not take. Tapping the bank button confirms it and
  the hand is dealt; holding it past that second is only a look at
  the bank, so a hand is never dealt by accident while the bankroll
  is being read. The bet chosen carries over to the next hand, so
  it only has to be set again to change it.

  A hand costs what the bet says, $10 to begin with, but a short
  bank is not the end of the run: with less than $5 left the whole
  balance goes on the next hand. Only a completely empty bank stops
  the game.

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

  TRAIN MODE - SW6 to GND

  The same hands with no bankroll, so the bet is not asked for and
  every hand is the same $10. Nothing costs anything, so a double
  or a split is always offered when the cards allow one.

  Every decision is graded. Correct lights the correct LED; wrong
  lights the wrong LED and puts the action the book wanted on the
  display for half a second before the hand carries on, as HIT,
  STND, DOUB or SPLT. The LEDs stay lit until the next decision
  replaces them.

  The bank button shows the running accuracy instead of a bankroll,
  and the accuracy blinks at the end of each hand in place of the
  bankroll: two digits, a point, two decimals, so 87.50 is 87.5
  percent. A perfect run has no room for two decimals and reads
  100.

  Nothing about accuracy is stored between sessions; it starts
  again from zero each time the switch comes back to train.

  The switch is read all through a hand, not only between hands.
  Moving it drops the hand that is in play, void, with no money
  paid out or taken for it, and the display answers with PLAY or
  TRN as the switch lands.

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
  - $100 to start, and the bet on every hand is the player's,
    from $5 up to $100, set before the cards come out. A bank
    holding less than the bet allows caps it, so a hand never
    stakes more than the bank can lose
  - Train mode has no bank, so there every hand is the same $10
  - A split hand and a double each cost another bet the size of
    the one already on the hand
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
  H = Hit     (SW1, 1k   on A4)   adds $5 to the bet
  S = Stand   (SW2, 3.3k on A4)   takes $5 off the bet
  D = Double  (SW3, 6.8k on A4)   doubles the bet
  P = Split   (SW4, 2.2k on A5)   halves the bet
  B = Bank    (SW5, 6.8k on A5)   confirms the bet, and holds the
                                  bankroll, or the accuracy in
                                  train mode

  The five buttons are two resistor ladders, not five separate
  pins. Each one switches its pin to +5 V through its own resistor
  against a 10k pull down, so A4 reads three buttons and A5 reads
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
// bet is carried exactly rather than rounded away. A long is 32 bits
// here, which is what $9999.00 needs: a 16 bit int stops at $327.67
// and 999900 cents would wrap to 16860, ending the run early at
// $168.60.
const long STARTING_BANKROLL = 10000;   // $100.00

// The bet is the player's to set before every hand. This is what a
// run starts on, the least it can be, the most it can be, and what
// one press moves it by
const long BET = 1000;      // $10.00, the bet a run starts on
const long MIN_BET = 500;   // $5.00, the smallest bet allowed
const long MAX_BET = 10000; // $100.00, the largest bet allowed
const long BET_STEP = 500;  // $5.00, one press of hit or stand

// $9999.00: the most four digits can show, and where the egg is
const long MAX_BANK = 999900;

// The one thing kept across a power cut: a four byte high score,
// little endian, at the bottom of the EEPROM. The bankroll is not
// stored, so every power up starts a fresh run at 100.
const int HIGHSCORE_ADDRESS = 0;   // 0 to 3

// The drill's memory: one byte per hand of the strategy chart. A byte
// counts right answers in a row, so three in a row masters a hand and
// a miss starts it again. Three hundred bytes sit above the high score.
const int MASTERY_TAG_ADDRESS = 4;   // 1 byte, the table's format
const int MASTERY_ADDRESS = 5;       // 300 bytes, one per chart hand
const byte MASTERY_VERSION = 1;
const byte MASTERY_MAX = 3;          // right answers in a row to master
const byte DRILL_LEVELS = 5;         // difficulty bands in the chart
const byte REVIEW_EVERY = 6;         // one hand in six looks further back

long bankroll = STARTING_BANKROLL;
long highscore = 0;

// The bet set for the next hand, kept from hand to hand until it is
// changed again. Not stored, so a run starts on $10.
long betAmount = BET;

// Train mode score, one count per decision. Not stored anywhere
// and reset every time a train session starts.
int correctDecisions = 0;
int totalDecisions = 0;

// Latched the moment the mode switch moves, wherever the game is
bool trainingMode = false;
bool modeKnown = false;

// What the switch said when it was last sampled, so a move is seen
// as an edge
bool switchWasPlay = true;

// Set the instant the switch moves, and cleared by loop(), which is
// what tears down the hand that was in play
bool modeChangePending = false;

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
  long wager;      // money riding on the hand, 0 for the dealer
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


// The bet that goes on the next hand, kept inside what the bank can
// cover and under the ceiling. Nothing under the minimum can be bet
// while the bank holds that much, and nothing above the bank at all.
// A bank too small for the minimum goes on the table whole: $3
// staked is a better end to a run than $3 sat out. The ceiling keeps
// one hand from swallowing a fat bank in a single round.
long chosenBet() {

  if (bankroll <= 0) {
    return 0;
  }

  if (bankroll < MIN_BET) {
    return bankroll;
  }

  if (betAmount < MIN_BET) {
    return MIN_BET;
  }

  if (betAmount > bankroll) {
    return bankroll;
  }

  if (betAmount > MAX_BET) {
    return MAX_BET;
  }

  return betAmount;
}


// One button's worth of change, held to the same limits: a double
// that would pass the bank stops at the bank, and a half that would
// fall under the minimum stops at the minimum. Returns false when
// the press asked for more than the bank can cover, more than the
// ceiling allows or less than the minimum, which the caller answers
// with the usual two blinks so no end of the range is ever silent.
bool changeBet(char button) {

  // An all in bet has nothing to move: there is no room above it and
  // the minimum is already out of reach, so every press is refused
  if (bankroll < MIN_BET) {
    betAmount = chosenBet();
    return false;
  }

  long bet = chosenBet();

  if (button == 'H') {
    bet += BET_STEP;
  } else if (button == 'S') {
    bet -= BET_STEP;
  } else if (button == 'D') {
    bet *= 2;
  } else if (button == 'P') {
    bet /= 2;
  }

  // The ceiling and the bank are two separate reasons to stop, and
  // either one refuses the press. A bank over $100 refuses on the
  // ceiling first, which is what keeps a single hand to $100.
  bool overBank = bet > bankroll;
  bool overMax = bet > MAX_BET;
  bool underMin = bet < MIN_BET;

  if (overBank) {
    bet = bankroll;
  }

  if (overMax) {
    bet = MAX_BET;
  }

  if (underMin) {
    bet = MIN_BET;
  }

  betAmount = bet;

  return !overBank && !overMax && !underMin;
}


// What one bet costs this round, which is the bet the player has set.
// Train mode has no bank, so there every hand is the same $10.
long betSize() {

  if (trainingMode) {
    return BET;
  }

  return chosenBet();
}


// Money not already riding on the table this round. A double or a
// split needs another bet out of this, which is what stops the
// bankroll from running below 0.
long moneyLeft(const Hand hands[], int handCount) {

  long committed = 0;

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

    // These fall through to the available-action check below, so a
    // hit that leaves 9, 10 or 11 is graded HIT when doubling is
    // no longer on the table      if (total == 11) {

        // An ace up is the one exception: this device never peeks at
        // the hole card, and an S17 dealer stands on 17, so 11 against
        // an ace is a hit rather than a double
        action = (up == 11) ? HIT : DOUBLE;

      } else if (total == 10) {

      action = (up <= 9) ? DOUBLE : HIT;

    } else if (total == 9) {

      action = (up >= 3 && up <= 6) ? DOUBLE : HIT;

    } else {

      return HIT;
    }
  }

  // A double is only available on the first two cards
  if (action == DOUBLE && !canDouble(hand)) {
    return HIT;
  }

  return action;
}


// --------------------------------------------------
// THE DRILL
// The chart is 300 hands: ten pair rows, eight soft rows and the
// twelve hard totals that still have a decision in them, each
// against the ten dealer up cards. A byte of EEPROM per hand keeps
// the counting, so nothing here needs a table in RAM.
// --------------------------------------------------

// The cell space, laid out the way the chart prints it: pairs first,
// then soft hands, then hard totals, each row running through the ten
// up cards from 2 to ace
const int CELL_COUNT = 300;
const int CELL_SOFT = 100;
const int CELL_HARD = 180;

const byte UP_CARDS[10] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 1 };

// The hand the drill is asking about this round, and whether its
// first answer has already been counted
byte drillCell = 0;
bool drillGraded = false;

// How many hands of the chart are mastered, counted at boot and kept
// up to date as answers come in
int masteredCells = 0;


// A cell is a pair (0), a soft hand (1) or a hard total (2)
byte cellKind(byte index) {

  if (index < CELL_SOFT) return 0;
  if (index < CELL_HARD) return 1;

  return 2;
}


byte cellUp(byte index) {

  return UP_CARDS[index % 10];
}


// The value of the pair, 1 for a pair of aces
byte cellPair(byte index) {

  return (byte)(index / 10) + 1;
}


// The low card of a soft hand, so 2 is A,2 and 9 is A,9
byte cellSoftLow(byte index) {

  return (byte)((index - CELL_SOFT) / 10) + 2;
}


byte cellHardTotal(byte index) {

  return (byte)((index - CELL_HARD) / 10) + 5;
}


// The hands that catch people out: the soft 18s that stand, the 16s
// that do not, 15 against a ten, 11 against an ace and a pair of 9s
// against a 7. They make up the last band on their own.
bool cellIsTricky(byte index) {

  byte up = cellUp(index);
  byte kind = cellKind(index);

  if (kind == 1 && cellSoftLow(index) == 7) {
    return up == 2 || up == 1 || up >= 7;
  }

  if (kind == 2) {

    byte total = cellHardTotal(index);

    if (total == 16) return up == 1 || up >= 9;
    if (total == 15) return up == 10;
    if (total == 11) return up == 1;
  }

  if (kind == 0 && cellPair(index) == 9) {
    return up == 7;
  }

  return false;
}


// Which band a hand belongs to. The bands run from the hands that
// come up most to the ones that need the most thought.
byte cellLevel(byte index) {

  if (cellIsTricky(index)) return 5;

  byte kind = cellKind(index);

  if (kind == 0) return 4;   // pairs
  if (kind == 1) return 3;   // soft hands

  byte total = cellHardTotal(index);

  if (total <= 8) return 1;  // nothing to get wrong
  if (total <= 11) return 2; // the doubles

  // 12 to 16: a bust card in front of the dealer or not
  byte up = cellUp(index);

  return (up == 1 || up >= 7) ? 2 : 1;
}


// What is stored against a hand. A byte nothing has written to yet
// reads back as 255, which counts as unseen like a zero does.
byte masteryOf(byte index) {

  byte value = EEPROM.read(MASTERY_ADDRESS + index);

  return (value > MASTERY_MAX) ? 0 : value;
}


void writeMastery(byte index, byte value) {

  if (masteryOf(index) == value) {
    return;
  }

  EEPROM.update(MASTERY_ADDRESS + index, value);
}


// The first band with anything left to master, or 0 when the whole
// chart is mastered
byte firstUnmasteredLevel() {

  for (byte level = 1; level <= DRILL_LEVELS; level++) {

    for (int i = 0; i < CELL_COUNT; i++) {

      byte cell = (byte)i;

      if (cellLevel(cell) == level && masteryOf(cell) < MASTERY_MAX) {
        return level;
      }
    }
  }

  return 0;
}


// How much a hand still needs work: four for one never answered, down
// to one for a hand a single right answer from mastered
byte cellWeight(byte index) {

  byte mastery = masteryOf(index);

  if (mastery >= MASTERY_MAX) {
    return 0;
  }

  return (byte)(MASTERY_MAX - mastery + 1);
}


// Pick the hand to deal: from the first band not yet mastered,
// weighted towards the hands answered wrong most often, with the odd
// hand from the bands below so the easy ones stay learned
byte chooseDrillCell() {

  byte level = firstUnmasteredLevel();

  // Everything is mastered: the drill carries on evenly
  if (level == 0) {
    return (byte)random(CELL_COUNT);
  }

  bool review = (level > 1) && (random(REVIEW_EVERY) == 0);
  int total = 0;

  for (int i = 0; i < CELL_COUNT; i++) {

    byte cell = (byte)i;

    if (review) {

      if (cellLevel(cell) < level && masteryOf(cell) >= MASTERY_MAX) {
        total++;
      }

    } else if (cellLevel(cell) == level) {

      total += cellWeight(cell);
    }
  }

  if (total == 0) {
    return (byte)random(CELL_COUNT);
  }

  int pick = random(total);

  for (int i = 0; i < CELL_COUNT; i++) {

    byte cell = (byte)i;

    if (review) {

      if (cellLevel(cell) < level && masteryOf(cell) >= MASTERY_MAX && pick-- == 0) {
        return cell;
      }

    } else if (cellLevel(cell) == level) {

      int weight = cellWeight(cell);

      if (pick < weight) {
        return cell;
      }

      pick -= weight;
    }
  }

  return (byte)random(CELL_COUNT);
}


// The two cards that make the chosen hand, and the dealer's up card.
// Hard totals are built from two different values, so that no pair
// stands in for a total the chart prints on a row of its own.
void cellCards(byte index, byte &first, byte &second, byte &up) {

  byte kind = cellKind(index);

  if (kind == 0) {

    first = cellPair(index);
    second = first;

  } else if (kind == 1) {

    first = 1;
    second = cellSoftLow(index);

  } else {

    byte total = cellHardTotal(index);

    // Every combination of two different values that adds up, with
    // one of them kept as the cards go by
    byte low = 2;
    byte high = total - 2;
    byte count = 0;

    for (byte a = 2; a <= 9; a++) {

      byte b = total - a;

      if (b < 2 || b > 10 || b == a) {
        continue;
      }

      if (random(++count) == 0) {
        low = a;
        high = b;
      }
    }

    first = low;
    second = high;
  }

  up = cellUp(index);
}


// One answer, stored against the hand it was asked about. Returns the
// band number when that answer was the last one the band needed, so
// the caller can say so between hands, and 0 otherwise.
byte recordDrill(bool correct) {

  byte before = masteryOf(drillCell);
  byte after;

  if (correct) {

    after = (before < MASTERY_MAX) ? before + 1 : before;

  } else {

    // Three in a row, so a miss starts the count again. A hand that
    // was already mastered only drops one step, which keeps an odd
    // slip from wiping out a whole band.
    after = (before >= MASTERY_MAX) ? 1 : 0;
  }

  if (after == before) {
    return 0;
  }

  writeMastery(drillCell, after);

  if (before < MASTERY_MAX && after >= MASTERY_MAX) {
    masteredCells++;
  } else if (before >= MASTERY_MAX && after < MASTERY_MAX) {
    masteredCells--;
  }

  // Did that finish the band?
  byte level = cellLevel(drillCell);

  for (int i = 0; i < CELL_COUNT; i++) {

    byte cell = (byte)i;

    if (cellLevel(cell) == level && masteryOf(cell) < MASTERY_MAX) {
      return 0;
    }
  }

  return level;
}


// --------------------------------------------------
// PIN MAP
// One line per pad of the ATmega328P, as wired on the board. The
// board is fixed, so these are constants rather than settings.
// --------------------------------------------------

// Mode switch, +5 V for play, GND for train. The pull up covers the
// break in the switch and holds play on a board with none fitted.
const int MODE_PIN = 0;        // PD0 pad 30

// Button ladders
const int BTN_MAIN_PIN = A4;   // PC4 pad 27: hit, stand, double
const int BTN_SIDE_PIN = A5;   // PC5 pad 28: split, bank

// LEDs, all active high through 330R
const int LED_PAIR    = A3;    // PC3 pad 26
const int LED_SOFT    = A0;    // PC0 pad 23
const int LED_WIN     = 8;     // PB0 pad 12
const int LED_LOSE    = 3;     // PD3 pad 1
const int LED_CORRECT = 2;     // PD2 pad 32
const int LED_WRONG   = 1;     // PD1 pad 31
const byte LED_PUSH_BIT = 6;   // PB6 pad 7, PORTB only

// PB6 and PB7 have no Arduino number, so the push LED and digit 2
// are written through PORTB rather than digitalWrite()
void writePortBPin(byte bit, bool on) {

  if (on) {
    PORTB |= _BV(bit);
  } else {
    PORTB &= ~_BV(bit);
  }
}

// Every indicator goes through one of these, so a call site does
// not have to know which pin, or which port, it landed on
void setCorrectLed(bool on) { digitalWrite(LED_CORRECT, on); }
void setWrongLed(bool on) { digitalWrite(LED_WRONG, on); }
void setPairLed(bool on) { digitalWrite(LED_PAIR, on); }
void setSoftLed(bool on) { digitalWrite(LED_SOFT, on); }
void setPushLed(bool on) { writePortBPin(LED_PUSH_BIT, on); }

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
const byte DIGIT_2_BIT = 7;    // Q2, PORTB only
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
const byte LEVEL_6K8  = 1;   // double on A4, bank on A5
const byte LEVEL_3K3  = 2;   // stand on A4
const byte LEVEL_2K2  = 3;   // split on A5
const byte LEVEL_1K   = 4;   // hit on A4


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
const byte LETTER_A = 0x77;   // a b c e f g, a closed A
const byte LETTER_B = 0x7C;   // c d e f g
const byte LETTER_D = 0x5E;   // b c d e g
const byte LETTER_E = 0x79;   // a d e f g
const byte LETTER_G = 0x3D;   // a c d e f, a C that closes
const byte LETTER_H = 0x76;   // b c e f g
const byte LETTER_I = 0x06;   // b c
const byte LETTER_L = 0x38;   // d e f
const byte LETTER_N = 0x54;   // c e g
const byte LETTER_O = 0x3F;   // a b c d e f
const byte LETTER_P = 0x73;   // a b e f g
const byte LETTER_R = 0x50;   // e g
const byte LETTER_S = 0x6D;   // a c d f g
const byte LETTER_T = 0x78;   // d e f g
const byte LETTER_U = 0x3E;   // b c d e f
const byte LETTER_Y = 0x6E;   // b c d f g

// The label shown when the high score is on the display
const byte HIGH_WORD[4] = { LETTER_H, LETTER_I, LETTER_G, LETTER_H };

// The label that goes in front of the share of the chart mastered.
// M has no seven segment shape, so this reads RATE
const byte RATE_WORD[4] = { LETTER_R, LETTER_A, LETTER_T, LETTER_E };

// What the game wants shown, one segment pattern per digit
byte displayBuffer[4] = { SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK };

// What the bank button shows, rebuilt whenever it is sampled
byte holdDigits[4] = { SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK };

// The best bank so far, for the half of a long hold that shows it
byte highScoreDigits[4] = { SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK };

byte refreshIndex = 0;
byte refreshTicks = 0;

// The bank button as sampled with the display refresh. bankShown is
// the debounced state, and a change has to hold for three samples
// before it counts, which is what rides out contact bounce.
bool bankShown = false;
byte bankSettle = 0;
unsigned long bankHeldSince = 0;

// How long a press has to last before it is a hold rather than a tap.
// The high score comes up at this point, and at the bet prompt a press
// shorter than this confirms the bet instead of only showing the bank.
const unsigned long BANK_HOLD_MS = 1000;


void writeSegments(byte pattern) {

  for (byte i = 0; i < 7; i++) {
    digitalWrite(SEG_PIN[i], (pattern >> i) & 1);
  }

  // The point is on its own pin, not part of the seven
  digitalWrite(SEG_DP, (pattern & SEGMENT_DP) ? HIGH : LOW);
}


void digitDrive(byte index, bool on) {

  // Digit 2 is the only one without an Arduino number
  if (index == 1) {
    writePortBPin(DIGIT_2_BIT, on);
    return;
  }

  if (index == 0) digitalWrite(DIGIT_1, on);
  if (index == 2) digitalWrite(DIGIT_3, on);
  if (index == 3) digitalWrite(DIGIT_4, on);
}


// Writes value into width digits at offset, right justified with
// blank leading digits. Only the requested digits are touched.
void numberToDigits(byte *out, byte offset, byte width, long value) {

  for (byte i = 0; i < width; i++) {
    out[offset + i] = SEGMENT_BLANK;
  }

  if (value < 0) value = 0;

  byte i = width;

  while (i > 0) {

    i--;

    out[offset + i] = FONT[(byte)(value % 10)];
    value /= 10;

    if (value == 0) break;
  }
}


// A share as a percentage with two decimals, 00.00 to 99.99. A
// flawless run would need five digits plus the point, so 100 is
// shown as 100. with the last digit blank.
void percentageToDigits(byte *out, long part, long whole) {

  int hundredths = 0;

  if (whole > 0) {
    hundredths = (part * 10000) / whole;
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


// How many of this train session's decisions were right
void accuracyToDigits(byte *out) {

  percentageToDigits(out, correctDecisions, totalDecisions);
}


// How much of the chart is mastered, in the same shape
void masteryToDigits(byte *out) {

  percentageToDigits(out, masteredCells, CELL_COUNT);
}


// True once the bank button has been down for a full second, which is
// where both modes move on to what the long hold shows
bool holdPastFirstSecond() {

  return (millis() - bankHeldSince) >= BANK_HOLD_MS;
}


// Train mode: past that second the accuracy and the mastery take
// turns, 200 ms each, with RATE in front of the second one
bool holdWantsMasteryWord() {

  if (!trainingMode || !holdPastFirstSecond()) {
    return false;
  }

  return (((millis() - bankHeldSince - BANK_HOLD_MS) / 200) % 2) == 0;
}


// What holding the bank button shows: the money in play mode, the
// accuracy and then the mastery in train mode
void fillHoldView(byte *out) {

  if (trainingMode) {

    if (holdPastFirstSecond() && !holdWantsMasteryWord()) {
      masteryToDigits(out);
    } else {
      accuracyToDigits(out);
    }

    return;
  }

  // Cents in, dollars out: the half dollar is kept, not shown
  numberToDigits(out, 0, 4, bankroll / 100);
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

  if (held < BANK_HOLD_MS) {
    return false;
  }

  return (((held - BANK_HOLD_MS) / 200) % 2) == 0;
}


// The other half of that flash: the best bank itself, in the dollars
// the bankroll is shown in. Train mode has no high score, so there the
// hold stays on the accuracy.
bool holdWantsHighScore() {

  if (trainingMode) {
    return false;
  }

  unsigned long held = millis() - bankHeldSince;

  if (held < BANK_HOLD_MS) {
    return false;
  }

  return (((held - BANK_HOLD_MS) / 200) % 2) != 0;
}


// The mode switch is sampled with every display refresh, which runs
// through pause() in all of the waits, so a move is caught wherever
// the game happens to be. It is sampled on the same slow tick as the
// bank button, so it costs well under a millisecond.
void pollModeSwitch() {

  bool switchPlay = switchSaysPlay();

  if (switchPlay == switchWasPlay) {
    return;
  }

  switchWasPlay = switchPlay;

  // The mode changes right here and now. Dropping the hand in play
  // is left to the checkpoints in playRound(), which see the flag on
  // their way round and return instead of dealing on.
  trainingMode = !switchPlay;
  modeChangePending = true;
}


void refreshDisplay() {

  // Sample the bank button and the mode switch now and then:
  // holding the button takes the display over from whatever the game
  // was showing, and the switch is what changes mode.
  if ((refreshTicks++ & 7) == 0) {

    pollModeSwitch();

    bool down = (readLadder(BTN_SIDE_PIN) == LEVEL_6K8);

    if (down == bankShown) {

      bankSettle = 0;

    } else if (++bankSettle >= 3) {

      bankSettle = 0;
      bankShown = down;

      // Start the hold clock on the edge, so the first second is
      // the bankroll and only then does it move on
      if (bankShown) {
        bankHeldSince = millis();
      }
    }

    if (bankShown) {

      fillHoldView(holdDigits);

      // Cents in, dollars out, as with the bankroll above
      numberToDigits(highScoreDigits, 0, 4, highscore / 100);
    }
  }

  // Blank the digit being left before changing the segments, so
  // no half lit digit shows through
  digitDrive(refreshIndex, false);

  refreshIndex = (refreshIndex + 1) & 3;

  if (bankShown) {

    if (holdWantsHighWord()) {

      writeSegments(HIGH_WORD[refreshIndex]);

    } else if (holdWantsHighScore()) {

      writeSegments(highScoreDigits[refreshIndex]);

    } else if (holdWantsMasteryWord()) {

      writeSegments(RATE_WORD[refreshIndex]);

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
  DDRB |= _BV(DIGIT_2_BIT);
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


// The word for the mode the switch has just been put in
void showModeWord(bool play) {

  clearDisplay();

  if (play) {

    displayBuffer[0] = LETTER_P;
    displayBuffer[1] = LETTER_L;
    displayBuffer[2] = LETTER_A;
    displayBuffer[3] = LETTER_Y;

  } else {

    displayBuffer[0] = LETTER_T;
    displayBuffer[1] = LETTER_R;
    displayBuffer[2] = LETTER_N;
  }

  pause(700);
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


// A band finished: L and the number of the band now being worked on
void showLevelUp(byte level) {

  clearDisplay();

  displayBuffer[0] = LETTER_L;
  displayBuffer[1] = FONT[level % 10];

  pause(700);
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
  DDRB |= _BV(LED_PUSH_BIT);

  digitalWrite(LED_PAIR, LOW);
  digitalWrite(LED_SOFT, LOW);
  digitalWrite(LED_WIN, LOW);
  digitalWrite(LED_LOSE, LOW);
  setPushLed(LOW);
}


// A bankroll of 9999 is the most four digits can show, so getting
// there sets every LED going on its own coin toss at its own random
// interval. The game never picks up again: only a reset ends it.
void easterEgg() {

  // The light show never ends. Only a reset leaves it, and a reset
  // starts a fresh run at 100 because nothing of this one is stored

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

    setPushLed(random(2));

    pause(random(20, 120));
  }
}


void setOutcomeLed(Outcome outcome) {

  digitalWrite(LED_WIN, outcome == OUTCOME_WIN);
  setPushLed(outcome == OUTCOME_PUSH);
  digitalWrite(LED_LOSE, outcome == OUTCOME_LOSE);
}


// --------------------------------------------------
// PLAYER INPUT
// --------------------------------------------------

// The levels themselves are with the pin map, because the display
// reads the bank level too
void initialiseInputs() {

  // Plain inputs, the pull downs are on the board
  pinMode(BTN_MAIN_PIN, INPUT);
  pinMode(BTN_SIDE_PIN, INPUT);

  // The mode switch drives this pin to a rail either way, and the
  // pull up tidies the moment between throws
  pinMode(MODE_PIN, INPUT_PULLUP);
}


// True when the mode switch is at the play end. A build with no
// switch fitted has the pull up and so reads play always.
bool switchSaysPlay() {

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
// any contact bounce on the way up. A mode change gives up on the
// button rather than waiting for it to be let go.
void waitForRelease(int pin) {

  while (readLadder(pin) != LEVEL_NONE) {

    if (modeChangePending) {
      return;
    }

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
// Returns false when the mode switch moved instead of a button being
// pressed, which drops the hand rather than asking for a decision on
// it.
bool waitForAction(Action &chosen) {

  while (true) {

    if (modeChangePending) {
      return false;
    }

    char button = readButton();

    if (button == 'H') { chosen = HIT;    return true; }
    if (button == 'S') { chosen = STAND;  return true; }
    if (button == 'D') { chosen = DOUBLE; return true; }
    if (button == 'P') { chosen = SPLIT;  return true; }
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

  // The press and its release both have to finish first, and a mode
  // change is a way out that leaves the bank where it is
  while (!anyButtonDown()) {

    if (modeChangePending) {
      return;
    }

    pause(20);
  }

  while (anyButtonDown()) {

    if (modeChangePending) {
      return;
    }

    pause(20);
  }

  bankroll = STARTING_BANKROLL;
  betAmount = BET;

  setOutcomeLed(OUTCOME_NONE);
}


// Before the cards come out the bet is the player's to set. BET names
// what the number that follows is, then the four action buttons move
// the bet and a tap of the bank button deals the hand. Returns false
// when the mode switch moved instead, which drops the round before a
// card is dealt.
bool askForBet() {

  clearDisplay();

  displayBuffer[0] = LETTER_B;
  displayBuffer[1] = LETTER_E;
  displayBuffer[2] = LETTER_T;

  pause(700);

  while (true) {

    if (modeChangePending) {
      return false;
    }

    // The amount stays up while the buttons are worked
    numberToDigits(displayBuffer, 0, 4, betSize() / 100);

    // The press is timed from here, so a tap and a hold can be told
    // apart once it is let go
    unsigned long pressedAt = millis();

    byte main = settledLevel(BTN_MAIN_PIN);

    if (main != LEVEL_NONE) {

      char button = 0;

      if (main == LEVEL_1K)  button = 'H';
      if (main == LEVEL_3K3) button = 'S';
      if (main == LEVEL_6K8) button = 'D';

      waitForRelease(BTN_MAIN_PIN);

      if (modeChangePending) {
        return false;
      }

      // A press past either end blinks twice, as any refused
      // action does
      if (!changeBet(button)) {
        rejectAction();
      }

      continue;
    }

    byte side = settledLevel(BTN_SIDE_PIN);

    if (side == LEVEL_2K2) {

      waitForRelease(BTN_SIDE_PIN);

      if (modeChangePending) {
        return false;
      }

      // Half of $5 is under the minimum, which is refused the
      // same way
      if (!changeBet('P')) {
        rejectAction();
      }

      continue;
    }

    if (side == LEVEL_6K8) {

      // Down until it is let go, with the display showing the
      // bankroll and then the high score, as any other hold does
      waitForRelease(BTN_SIDE_PIN);

      if (modeChangePending) {
        return false;
      }

      // A hold was a look at the bank, not a confirmation
      if (millis() - pressedAt < BANK_HOLD_MS) {
        return true;
      }

      continue;
    }

    pause(10);
  }
}


void playRound() {

  // A switch moved since the last round: loop() shows the new mode
  // rather than this one dealing a hand
  if (modeChangePending) {
    return;
  }

  if (shoeIsLow()) {
    initialiseShoe();
  }

  // Switching back from train mode with an empty bank
  if (!trainingMode && bankroll <= 0) {

    waitForRestart();

    if (modeChangePending) {
      return;
    }
  }

  // New round, no result showing yet
  setOutcomeLed(OUTCOME_NONE);
  setCorrectLed(LOW);
  setWrongLed(LOW);

  // The bet is set here, in play mode only: train mode has no bank
  // to bet and no reason to ask
  if (!trainingMode && !askForBet()) {
    return;
  }

  Hand hands[MAX_HANDS];
  int handCount = 1;

  clearHand(hands[0]);

  byte dealerUp;
  byte dealerHole;
  byte first;
  byte second;

  if (trainingMode) {

    // The drill deals the hand it wants to ask about. The chart cell
    // decides both player cards and the dealer's up card; only the
    // hole card and the hits come off the shoe, so no search of the
    // shoe is needed and a hand the chart has but a shoe rarely
    // deals can still come up
    drillCell = chooseDrillCell();
    cellCards(drillCell, first, second, dealerUp);

    dealerHole = drawCard();

  } else {

    dealerUp = drawCard();
    dealerHole = drawCard();

    first = drawCard();
    second = drawCard();
  }

  addCard(hands[0], first);
  addCard(hands[0], second);

  hands[0].wager = betSize();

  // Only the visible card is known until the hole card is turned
  Hand dealer;
  clearHand(dealer);
  addCard(dealer, dealerUp);

  // The band this hand has just finished, if it finished one, and
  // whether the drill's own hand has been counted yet
  byte finishedLevel = 0;

  drillGraded = false;

  int current = 0;

  while (current < handCount) {

    // The hand was dropped between hands of a split, so it stops
    // here rather than playing the remaining ones out
    if (modeChangePending) {
      return;
    }

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

      Action chosen;

      if (!waitForAction(chosen)) {
        return;
      }

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

        // The hand the drill dealt is the one being learned, so only
        // its first answer counts against the chart. It is stored
        // right here rather than at the end of the hand, so a power
        // cut or a flick of the switch cannot lose it; the rest of a
        // split's decisions still count for the accuracy.
        if (!drillGraded) {

          drillGraded = true;
          finishedLevel = recordDrill(correctChoice);
        }

        if (!correctChoice) {

          showActionWord(correct);
          pause(500);
        }

        if (finishedLevel && finishedLevel < DRILL_LEVELS) {

          showLevelUp((byte)(finishedLevel + 1));
        }

        finishedLevel = 0;
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

  // A hand dropped inside that pause never reaches the dealer
  if (modeChangePending) {
    return;
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

      if (modeChangePending) {
        return;
      }

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

  // A dropped hand is void: it is not settled, so no money moves and
  // nothing is paid out or taken for it
  if (modeChangePending) {
    return;
  }

  byte dealerTotal = handTotal(dealer);
  bool dealerNatural = handIsNatural(dealer);

  for (int i = 0; i < handCount; i++) {

    byte playerTotal = handTotal(hands[i]);
    bool playerNatural = handIsNatural(hands[i]);
    long wager = hands[i].wager;

    Outcome outcome = OUTCOME_PUSH;
    long change = 0;

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

    // A better bankroll than ever before is kept for good. It is the
    // only thing about a run that a power cut leaves behind
    if (bankroll > highscore) {

      highscore = bankroll;
      EEPROM.put(HIGHSCORE_ADDRESS, highscore);
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

  // What the switch reads now, so the first sample during a pause
  // has something to compare against
  switchWasPlay = switchSaysPlay();

  // The high score is the only thing left by an earlier run. A blank
  // EEPROM reads back as -1.
  highscore = 0;
  EEPROM.get(HIGHSCORE_ADDRESS, highscore);

  if (highscore < 0) {
    highscore = 0;
  }

  // Every power up starts a fresh run at 100
  bankroll = STARTING_BANKROLL;

  // The drill table has a format byte of its own. A table left by an
  // older build, or a blank EEPROM, is cleared once: every hand then
  // starts unseen, which is what a zero byte already means.
  if (EEPROM.read(MASTERY_TAG_ADDRESS) != MASTERY_VERSION) {

    for (int i = 0; i < CELL_COUNT; i++) {
      EEPROM.update(MASTERY_ADDRESS + i, 0);
    }

    EEPROM.update(MASTERY_TAG_ADDRESS, MASTERY_VERSION);
  }

  // Counted once here and kept up to date as answers come in, so the
  // display never has to run through 300 bytes to show a percentage
  masteredCells = 0;

  for (int i = 0; i < CELL_COUNT; i++) {

    if (masteryOf((byte)i) >= MASTERY_MAX) {
      masteredCells++;
    }
  }

  // Seed the random generator. A6 and A7 are the two ADC inputs that
  // exist only on the 32 pin part, and the board leaves both of them
  // floating, which makes them the best noise source on the board. On
  // a crystal-less chip the time from reset to here barely varies, so
  // micros() on its own would deal the same shoe every power up.
  unsigned long seed = micros();

  for (byte i = 0; i < 8; i++) {
    seed = seed * 31 + analogRead(A6);
    seed = seed * 31 + analogRead(A7);
    delay(2);
  }

  randomSeed(seed);

  initialiseShoe();
}


// --------------------------------------------------
// MAIN LOOP
// --------------------------------------------------

void loop() {

  bool modeChanged = false;
  bool enteredPlay = false;

  if (!modeKnown) {

    // The switch decides from the first hand on. A build with no
    // switch fitted is pulled up, so it comes up in play.
    modeKnown = true;
    modeChanged = true;
    enteredPlay = switchSaysPlay();
    switchWasPlay = enteredPlay;
    trainingMode = !enteredPlay;

  } else if (modeChangePending) {

    // The switch moved mid-hand: pollModeSwitch() has changed the
    // mode already, and playRound() has dropped the hand
    modeChangePending = false;
    modeChanged = true;
    enteredPlay = !trainingMode;
  }

  if (modeChanged) {

    setOutcomeLed(OUTCOME_NONE);
    setSoftLed(false);
    setPairLed(false);
    setCorrectLed(LOW);
    setWrongLed(LOW);

    if (trainingMode) {

      // Every train session starts from nothing
      correctDecisions = 0;
      totalDecisions = 0;
    }

    // PLAY or TRN, before anything else the new mode has to say
    showModeWord(enteredPlay);
  }

  if (enteredPlay) {

    // HIGH, then the best bank, before the first hand
    showHighScore();

  } else if (modeChanged) {

    // Into train mode, where the accuracy blinking follows the TRN
    // word: the correct and wrong LEDs are not fitted
    flashSummary();
  }

  playRound();
}
