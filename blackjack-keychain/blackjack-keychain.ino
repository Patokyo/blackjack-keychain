/*
  BLACKJACK STRATEGY TRAINER
  Stage 2: Full hands over serial

  Hardware: ATmega328P-A in the 32 pin TQFP/MLF package, 5 V from
  the USB-C socket, internal 8 MHz RC oscillator, no crystal.

  Pin map, taken straight from blackjack-keychain.net (pad is the
  TQFP pad, the second column the port bit, the third the Arduino
  name):

    pad 32  PD2  D2    correct LED
    pad 31  PD1  D1    wrong LED
    pad 30  PD0  D0    SW6, an SPDT tied to +5 V and GND
    pad  1  PD3  D3    lose LED
    pad  2  PD4  D4    push LED
    pad  9  PD5  D5    digit 3 driver, Q3
    pad 10  PD6  D6    segment b
    pad 11  PD7  D7    segment g
    pad 12  PB0  D8    segment c
    pad 13  PB1  D9    segment dp
    pad 14  PB2  D10   segment f
    pad 15  PB3  D11   segment d   (also ISP MOSI)
    pad 16  PB4  D12   segment a   (also ISP MISO)
    pad 17  PB5  D13   segment e   (also ISP SCK)
    pad 23  PC0  A0    digit 2 driver, Q2
    pad 24  PC1  A1    soft LED
    pad 25  PC2  A2    digit 4 driver, Q4
    pad 26  PC3  A3    pair LED
    pad 27  PC4  A4    hit / stand / double ladder
    pad 28  PC5  A5    split / bank ladder
    pad 29  PC6  RESET
    pad  7  PB6  ---   win LED
    pad  8  PB7  ---   digit 1 driver, Q1

  PB6 and PB7 have no Arduino number on a 328P because a standard
  board spends them on the crystal. Here the chip runs from its
  internal oscillator, so they are free, but they can only be
  reached through PORTB:

    DDRB  |= _BV(6);  PORTB |= _BV(6);    win LED on
    PORTB &= ~_BV(6);                     win LED off

  Never pass 6 or 7 to digitalWrite(). Those numbers are D6 and D7,
  segments b and g, and a "win LED" on 6 would light a segment.

  Two things about this netlist are worth knowing before flashing:

  - Serial transmit is pad 31, which is also the wrong LED. Prints
    still go out and still read correctly, but that LED sits lit
    between bytes and flickers while data goes past.

  - Serial receive is pad 30, held at +5 V or GND by SW6, so the
    serial port cannot be typed into on this board. The buttons
    are the only input.

  Clock and fuses. Build for F_CPU = 8000000L: MiniCore's ATmega328
  at "8 MHz internal", or a Pro Mini 3.3 V/8 MHz. CKDIV8 must be
  unprogrammed, low fuse 0xE2. A chip straight from the factory
  divides the internal oscillator down to 1 MHz, which plays the
  game eight times too slowly and makes serial output unreadable.

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
  - Wins pay even money, pushes return the bet
  - The game ends when $10 can no longer be covered

  Display:
  "Player: 16"        total only
  "Player: 12 Soft"   an ace is being counted as 11
  "Player: 12 Pair"   the first two cards are the same
  "Hand 2: 19"        when the hand was split

  Controls, from the buttons, or from the serial port if SW6 is
  left off the board:
  H = Hit     (SW1, 1k   on A4)
  S = Stand   (SW2, 3.3k on A4)
  D = Double  (SW3, 6.8k on A4)
  P = Split   (SW4, 2.2k on A5)
  B = Bank    (SW5, 6.8k on A5)   wired, no game action yet

  The five buttons are two resistor ladders, not five separate
  pins. Each one switches its pin to +5 V through its own resistor
  against a 10k pull down, so A4 reads three buttons and A5 reads
  two by the voltage it lands on. The divider and the ADC reference
  both come from +5 V, so the readings are ratiometric and do not
  care how the supply sags.

  Press one button at a time on a ladder. Two at once put their
  resistors in parallel and read as a third, meaningless level.

  The LEDs and the four digit display are wired and mapped below
  but nothing drives them yet, so the serial log is still the only
  output there is.

  Every decision is marked CORRECT / INCORRECT against basic
  strategy, the whole hand is played out, the dealer draws to
  17, and each hand is then marked a win, loss or push.
*/

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

// Score tracking (one question per decision)
int correctDecisions = 0;
int totalDecisions = 0;

// Betting
const int STARTING_BANKROLL = 100;
const int BET = 10;

int bankroll = STARTING_BANKROLL;
bool gameOver = false;

// Player actions
enum Action {
  HIT,
  STAND,
  DOUBLE,
  SPLIT
};

// A hand is kept as "every ace counts as 1" plus how many aces
// it holds, so no list of cards has to be stored.
struct Hand {
  byte hard;       // total with aces counted as 1
  byte aces;       // number of aces in the hand
  byte count;      // number of cards in the hand
  byte first;      // value of the first card
  byte wager;      // money riding on the hand, 0 for the dealer
  byte pairValue;  // matching value if the hand is a pair, else 0
  bool aceSplit;   // came from splitting aces: one card only
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

  return hand.count == 2 && !hand.aceSplit && handTotal(hand) == 21;
}


bool canDouble(const Hand &hand) {

  return hand.count == 2;
}


bool canSplit(const Hand &hand, int handCount) {

  return hand.pairValue != 0 && handCount < MAX_HANDS && !hand.aceSplit;
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
  byte up = (dealerUp == 1) ? 11 : dealerUp;

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
// DISPLAY
// --------------------------------------------------

void printCardName(byte card) {

  if (card == 1) {
    Serial.print('A');
  } else {
    Serial.print(card);
  }
}


void printHandLabel(int index, int handCount) {

  if (handCount > 1) {

    Serial.print(F("Hand "));
    Serial.print(index + 1);
    Serial.print(F(": "));

  } else {

    Serial.print(F("Player: "));
  }
}


void printHand(const Hand &hand, int index, int handCount) {

  printHandLabel(index, handCount);

  Serial.print(handTotal(hand));

  if (hand.pairValue != 0) {
    Serial.print(F(" Pair"));
  }

  if (handIsSoft(hand)) {
    Serial.print(F(" Soft"));
  }

  Serial.println();
}


void printDraw(byte card, int index, int handCount) {

  if (handCount > 1) {

    Serial.print(F("Hand "));
    Serial.print(index + 1);
    Serial.print(F(" draws: "));

  } else {

    Serial.print(F("Card: "));
  }

  printCardName(card);
  Serial.println();
}


void printDealerUpcard(byte dealerUp) {

  Serial.print(F("Dealer: "));
  Serial.println(dealerUp == 1 ? 11 : dealerUp);
}


void printActionName(Action action) {

  switch (action) {

    case HIT:    Serial.println(F("HIT"));    break;
    case STAND:  Serial.println(F("STAND"));  break;
    case DOUBLE: Serial.println(F("DOUBLE")); break;
    case SPLIT:  Serial.println(F("SPLIT"));  break;
  }
}


// --------------------------------------------------
// PIN MAP
// One line per net in blackjack-keychain.net. The board is
// fixed, so these are constants rather than settings.
// Everything below the ladders is mapped but not driven yet.
// --------------------------------------------------

// Button ladders
const int BTN_MAIN_PIN = A4;   // PC4 pad 27: hit, stand, double
const int BTN_SIDE_PIN = A5;   // PC5 pad 28: split, bank

// LEDs, all active high through 330R
const int LED_PAIR    = A3;    // PC3 pad 26
const int LED_SOFT    = A1;    // PC1 pad 24
const int LED_LOSE    = 3;     // PD3 pad 1
const int LED_PUSH    = 4;     // PD4 pad 2
const int LED_CORRECT = 2;     // PD2 pad 32
const int LED_WRONG   = 1;     // PD1 pad 31, the TX pin
const byte LED_WIN_BIT = 6;    // PB6 pad 7, PORTB only

// Four digit display U2, CC56-12EWA, common cathode, with one NPN
// low side switch per digit. Segments and digit drives are all
// active high.
const int SEG_A  = 12;   // PB4 pad 16
const int SEG_B  = 6;    // PD6 pad 10
const int SEG_C  = 8;    // PB0 pad 12
const int SEG_D  = 11;   // PB3 pad 15
const int SEG_E  = 13;   // PB5 pad 17
const int SEG_F  = 10;   // PB2 pad 14
const int SEG_G  = 7;    // PD7 pad 11
const int SEG_DP = 9;    // PB1 pad 13

const int DIGIT_2 = A0;  // PC0 pad 23, Q2
const int DIGIT_3 = 5;   // PD5 pad 9,  Q3
const int DIGIT_4 = A2;  // PC2 pad 25, Q4
const byte DIGIT_1_BIT = 7;    // PB7 pad 8, Q1, PORTB only


// --------------------------------------------------
// PLAYER INPUT
// --------------------------------------------------

// Each button pulls its ladder pin up through its own resistor
// against a 10k pull down, so the ADC sees 10k/(10k+R) of full
// scale:
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

void initialiseInputs() {

  // Plain analog inputs, the pull downs are on the board
  pinMode(BTN_MAIN_PIN, INPUT);
  pinMode(BTN_SIDE_PIN, INPUT);
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

    delay(3);

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
    delay(5);
  }

  delay(20);
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


char getPlayerInput() {

  // Asked once per decision, so a newline still arriving from the serial
  // monitor cannot print the prompt a second time
  Serial.print(F("Action (H/S/D/P): "));

  // A button still held from the last decision must not answer this one
  while (anyButtonDown()) {
    delay(5);
  }

  while (true) {

    char button = readButton();

    if (button != 0) {

      // The bank button is on the board but has no action yet
      if (button == 'B') {

        Serial.println(F("Bank button has no game action yet."));
        Serial.print(F("Action (H/S/D/P): "));
        continue;
      }

      // Echo the press so the log reads the same as typed input
      Serial.println(button);
      return button;
    }

    if (Serial.available() == 0) {
      continue;
    }

    char input = Serial.read();

    // Ignore line endings and spaces silently
    if (input == '\n' || input == '\r' || input == ' ') {
      continue;
    }

    // Clear anything typed ahead
    while (Serial.available()) {
      Serial.read();
    }

    if (input >= 'a' && input <= 'z') {
      input = input - 'a' + 'A';
    }

    if (input == 'H' || input == 'S' ||
        input == 'D' || input == 'P') {

      return input;
    }

    Serial.println(F("Invalid input. Use H, S, D or P."));
    Serial.print(F("Action (H/S/D/P): "));
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
  newHand.wager = BET;

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

void playRound() {

  if (shoeIsLow()) {

    Serial.println();
    Serial.println(F("--- Shuffling a fresh shoe ---"));
    initialiseShoe();
  }

  Hand hands[MAX_HANDS];
  int handCount = 1;

  clearHand(hands[0]);

  byte dealerUp = drawCard();
  byte dealerHole = drawCard();

  addCard(hands[0], drawCard());
  addCard(hands[0], drawCard());

  hands[0].wager = BET;

  // Dealer's visible card only, the hole card is added at the end
  Hand dealer;
  clearHand(dealer);
  addCard(dealer, dealerUp);

  Serial.println();
  Serial.println(F("=============================="));

  Serial.print(F("Bankroll: $"));
  Serial.print(bankroll);
  Serial.print(F("  Bet: $"));
  Serial.println(BET);

  int current = 0;

  while (current < handCount) {

    // A freshly split hand holds one card and needs a second
    if (hands[current].count == 1) {

      byte card = drawCard();
      addCard(hands[current], card);

      printDraw(card, current, handCount);

      if (hands[current].aceSplit) {

        // Split aces are played with that single card
        current++;
        continue;
      }
    }

    // A natural blackjack has nothing to decide
    if (handIsNatural(hands[current])) {

      printHand(hands[current], current, handCount);
      printDealerUpcard(dealerUp);
      Serial.println();
      Serial.println(F("Blackjack!"));

      current++;
      continue;
    }

    bool restartHand = false;

    while (true) {

      printHand(hands[current], current, handCount);
      printDealerUpcard(dealerUp);
      Serial.println();

      char input = getPlayerInput();

      Action chosen;

      switch (input) {

        case 'H': chosen = HIT;    break;
        case 'S': chosen = STAND;  break;
        case 'D': chosen = DOUBLE; break;
        default:  chosen = SPLIT;  break;
      }

      // Reject actions this hand cannot make, without grading them
      if (chosen == DOUBLE && !canDouble(hands[current])) {

        Serial.println(F("Cannot double after hitting. Choose again."));
        continue;
      }

      if (chosen == SPLIT && !canSplit(hands[current], handCount)) {

        Serial.println(F("Cannot split this hand. Choose again."));
        continue;
      }

      Action correct = bestAction(hands[current], dealerUp, handCount);

      totalDecisions++;

      if (chosen == correct) {

        correctDecisions++;
        Serial.println(F("CORRECT"));

      } else {

        Serial.println(F("INCORRECT"));
        Serial.print(F("Correct action: "));
        printActionName(correct);
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
        hands[current].wager += BET;
      }

      // Hit or double both draw one card
      byte card = drawCard();
      addCard(hands[current], card);

      printDraw(card, current, handCount);

      if (handIsBust(hands[current])) {

        Serial.println(F("Bust!"));
        break;
      }

      if (handTotal(hands[current]) == 21) {

        Serial.println(F("21, standing."));
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

  Serial.println();

  addCard(dealer, dealerHole);

  Serial.print(F("Dealer hole card: "));
  printCardName(dealerHole);
  Serial.print(F(" (dealer total "));
  Serial.print(handTotal(dealer));
  Serial.println(F(")"));

  if (handIsNatural(dealer)) {
    Serial.println(F("Dealer has blackjack."));
  }

  // Dealer stands on all 17s, including soft 17
  while (handTotal(dealer) < 17) {

    byte card = drawCard();
    addCard(dealer, card);

    Serial.print(F("Dealer draws: "));
    printCardName(card);
    Serial.print(F(" (dealer total "));
    Serial.print(handTotal(dealer));
    Serial.println(F(")"));
  }

  // --------------------------------------------------
  // RESULT
  // --------------------------------------------------

  byte dealerTotal = handTotal(dealer);
  bool dealerNatural = handIsNatural(dealer);

  Serial.println();

  for (int i = 0; i < handCount; i++) {

    byte playerTotal = handTotal(hands[i]);
    bool playerNatural = handIsNatural(hands[i]);
    byte wager = hands[i].wager;

    if (handCount > 1) {

      Serial.print(F("Hand "));
      Serial.print(i + 1);
      Serial.print(F(": "));
    }

    if (playerTotal > 21) {

      Serial.println(F("Player busts. Dealer wins."));
      bankroll -= wager;

    } else if (playerNatural && dealerNatural) {

      Serial.println(F("Both blackjack. Push."));

    } else if (dealerNatural) {

      // A natural beats any 21 made with three or more cards
      Serial.println(F("Dealer blackjack. Dealer wins."));
      bankroll -= wager;

    } else if (playerNatural) {

      // A natural beats a dealer 21 made with three or more cards
      Serial.println(F("Player blackjack. Player wins."));
      bankroll += wager;

    } else if (dealerTotal > 21) {

      Serial.println(F("Dealer busts. Player wins."));
      bankroll += wager;

    } else if (playerTotal > dealerTotal) {

      Serial.println(F("Player wins."));
      bankroll += wager;

    } else if (playerTotal < dealerTotal) {

      Serial.println(F("Dealer wins."));
      bankroll -= wager;

    } else {

      Serial.println(F("Push."));
    }
  }

  Serial.println();

  Serial.print(F("Bankroll: $"));
  Serial.println(bankroll);
  Serial.println();

  Serial.print(F("Decisions: "));
  Serial.print(correctDecisions);
  Serial.print('/');
  Serial.print(totalDecisions);

  // A blackjack round asks nothing, so guard the percentage
  if (totalDecisions > 0) {

    Serial.print(F(" ("));
    Serial.print((correctDecisions * 100) / totalDecisions);
    Serial.print(F("%)"));
  }

  Serial.println();

  Serial.print(F("Cards remaining: "));
  Serial.println(SHOE_SIZE - shoePosition);

  // The bankroll can no longer cover a bet
  if (bankroll < BET) {

    Serial.println();
    Serial.println(F("Out of money. Game over."));
    gameOver = true;
  }

  Serial.println(F("=============================="));
}


// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup() {

  Serial.begin(9600);

  // Seed the random generator. A6 and A7 are the two ADC inputs that
  // exist only on the 32 pin part, and the netlist leaves both of them
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

  initialiseInputs();

  Serial.println();
  Serial.println(F("BLACKJACK STRATEGY TRAINER"));
  Serial.println(F("6 deck shoe | S17 | DAS"));
  Serial.println(F("Buttons: hit/stand/double on A4, split/bank on A5"));

  Serial.print(F("Starting bankroll: $"));
  Serial.print(STARTING_BANKROLL);
  Serial.print(F("  Bet: $"));
  Serial.println(BET);

  Serial.println(F("Starting shoe..."));

  initialiseShoe();
}


// --------------------------------------------------
// MAIN LOOP
// --------------------------------------------------

void loop() {

  // Out of money: the game is finished
  if (gameOver) {
    return;
  }

  playRound();

  delay(1000);
}
