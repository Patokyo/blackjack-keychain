/*
  BLACKJACK STRATEGY TRAINER - rules, shoe and money

  A port of firmware/blackjack-keychain/blackjack-keychain.ino. The shoe,
  the hand maths, the split and double restrictions, the basic strategy
  table and the betting all follow the sketch line for line, so this plays
  exactly the same game:

    - 6 deck shoe, reshuffled between hands when it runs low
    - dealer stands on soft 17
    - double on any first two cards, double after split allowed
    - up to 4 hands by splitting (no resplitting aces, split aces get
      one card only)
    - a two card 21 is a natural, and not after a split
    - a natural beats any 21 made with three or more cards
    - two naturals, or two ordinary 21s, push
    - $100 to start, and the bet on every hand is the player's, from $5
      up to $100, set before the cards come out. A bank holding less than
      the bet allows caps it, so a hand never stakes more than the bank
      can lose
    - a split hand and a double each cost another bet the size of the one
      already on the hand
    - money is counted in cents, a natural pays 3:2, and the half dollar
      it can pay on a short bet is kept (the display drops it)
    - a bank under $10 goes on the next hand whole, and only an empty
      bank stops the game
    - the bank stops at $9999, which is the jackpot
    - a run starts at $100 every load, and only the best bank is kept
      between sessions

  Everything here is state without a clock: ui.js runs the round, because
  the sketch's pauses and its display refresh are the same thing.
*/

(function (root, factory) {

  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.BlackjackEngine = factory();
  }

})(typeof globalThis !== 'undefined' ? globalThis : this, function () {

  'use strict';

  const DECK_COUNT = 6;
  const SHOE_SIZE = DECK_COUNT * 52;      // 312
  const RESHUFFLE_AT = 40;
  const MAX_HANDS = 4;

  // Money is counted in cents, so a 3:2 natural on an odd bet is carried
  // exactly rather than rounded away
  const STARTING_BANKROLL = 10000;        // $100.00

  // The bet is the player's to set before every hand: what a run starts
  // on, the least it can be, the most it can be, and what one press moves
  // it by
  const BET = 1000;                       // $10.00, the bet a run starts on
  const MIN_BET = 500;                    // $5.00, the smallest bet allowed
  const MAX_BET = 10000;                  // $100.00, the largest bet allowed
  const BET_STEP = 500;                   // $5.00, one press of hit or stand

  const MAX_BANK = 999900;                // $9999.00, the most four digits show

  // The one thing kept between sessions: the best bank
  const HIGH_KEY = 'blackjack-keychain.highscore';

  // The drill's memory: one byte per hand of the chart, counting right
  // answers in a row, and the key it is kept under. The chart has 300
  // hands, which is exactly the size of the sketch's EEPROM table.
  const CELL_COUNT = 300;
  const CELL_SOFT = 100;
  const CELL_HARD = 180;
  const NO_CELL = 255;   // a hand the chart has no row for
  const MASTERY_MAX = 3;
  const REVIEW_EVERY = 6;
  const MASTERY_KEY = 'blackjack-keychain.mastery';
  const UP_CARDS = [2, 3, 4, 5, 6, 7, 8, 9, 10, 1];


  // --------------------------------------------------
  // HAND CALCULATIONS
  // A hand is kept as "every ace counts as 1" plus how many aces it
  // holds, so no list of cards has to be stored.
  // --------------------------------------------------

  function clearHand(hand) {

    hand.hard = 0;          // total with aces counted as 1
    hand.aces = 0;          // number of aces in the hand
    hand.count = 0;         // number of cards in the hand
    hand.first = 0;         // value of the first card
    hand.wager = 0;         // money riding on the hand, 0 for the dealer
    hand.pairValue = 0;     // matching value if the hand is a pair, else 0
    hand.aceSplit = false;  // came from splitting aces: one card only
    hand.fromSplit = false; // came off a split, so 21 is not a natural

    return hand;
  }


  function emptyHand() {

    return clearHand({});
  }


  function addCard(hand, card) {

    if (hand.count === 0) {
      hand.first = card;
    }

    hand.hard += card;

    if (card === 1) {
      hand.aces++;
    }

    hand.count++;

    if (hand.count === 2) {

      // A pair only exists on the first two cards
      hand.pairValue = (card === hand.first) ? card : 0;

    } else if (hand.count > 2) {

      hand.pairValue = 0;
    }
  }


  // Best total: count one ace as 11 where that does not bust
  function handTotal(hand) {

    let total = hand.hard;

    if (hand.aces > 0 && total + 10 <= 21) {
      total += 10;
    }

    return total;
  }


  // True when an ace is currently being played as 11
  function handIsSoft(hand) {

    return hand.aces > 0 && hand.hard + 10 <= 21;
  }


  function handIsBust(hand) {

    return handTotal(hand) > 21;
  }


  // A natural is an ace plus a ten value card in the first two cards.
  // 21 on three or more cards, or 21 after a split, is not a natural.
  function handIsNatural(hand) {

    return hand.count === 2 && !hand.fromSplit && handTotal(hand) === 21;
  }


  // An ace at the top of the dealer's hand reads as 11
  function cardValue(card) {

    return (card === 1) ? 11 : card;
  }


  function canDouble(hand) {

    return hand.count === 2;
  }


  function canSplit(hand, handCount) {

    return hand.pairValue !== 0 && handCount < MAX_HANDS && !hand.aceSplit;
  }


  // --------------------------------------------------
  // BASIC STRATEGY
  // Six decks, S17, DAS, no surrender
  // --------------------------------------------------

  function shouldSplit(pair, up) {

    if (pair === 1 || pair === 8) return true;    // always split aces and 8s
    if (pair === 5 || pair === 10) return false;  // never split 5s or 10s

    if (pair === 4) return (up === 5 || up === 6);

    if (pair === 6) return (up >= 2 && up <= 6);

    if (pair === 9) {
      return (up >= 2 && up <= 6) || (up >= 8 && up <= 9);
    }

    // 2s, 3s and 7s split against 2 through 7
    return (up >= 2 && up <= 7);
  }


  function bestAction(hand, dealerUp, handCount) {

    // An ace up is worth 11
    const up = cardValue(dealerUp);

    if (canSplit(hand, handCount) && shouldSplit(hand.pairValue, up)) {
      return 'P';
    }

    const total = handTotal(hand);
    let action;

    if (handIsSoft(hand)) {

      // SOFT HANDS (ace counted as 11)
      if (total >= 19) return 'S';

      if (total === 18) {

        if (up >= 3 && up <= 6) action = 'D';
        else if (up === 2 || up === 7 || up === 8) return 'S';
        else return 'H';

      } else if (total === 17) {

        action = (up >= 3 && up <= 6) ? 'D' : 'H';

      } else if (total === 15 || total === 16) {

        action = (up >= 4 && up <= 6) ? 'D' : 'H';

      } else if (total === 13 || total === 14) {

        action = (up === 5 || up === 6) ? 'D' : 'H';

      } else {

        return 'H';
      }

    } else {

      // HARD HANDS
      if (total >= 17) return 'S';

      if (total >= 13) return (up >= 2 && up <= 6) ? 'S' : 'H';

      if (total === 12) return (up >= 4 && up <= 6) ? 'S' : 'H';

      // These fall through to the available-action check below, so a
      // hit that leaves 9, 10 or 11 is graded HIT when doubling is
      // no longer on the table
      if (total === 11) {

        // An ace up is the one exception: this device never peeks at the
        // hole card, and an S17 dealer stands on 17, so 11 against an ace
        // is a hit rather than a double
        action = (up === 11) ? 'H' : 'D';

      } else if (total === 10) {

        action = (up <= 9) ? 'D' : 'H';

      } else if (total === 9) {

        action = (up >= 3 && up <= 6) ? 'D' : 'H';

      } else {

        return 'H';
      }
    }

    // A double is only available on the first two cards
    if (action === 'D' && !canDouble(hand)) {
      return 'H';
    }

    return action;
  }


  // --------------------------------------------------
  // THE DRILL
  // The chart is 300 hands: ten pair rows, eight soft rows and the twelve
  // hard totals that still have a decision in them, each against the ten
  // dealer up cards. The bands and the weighting are the sketch's.
  // --------------------------------------------------

  // A cell is a pair (0), a soft hand (1) or a hard total (2)
  function cellKind(index) {

    if (index < CELL_SOFT) return 0;
    if (index < CELL_HARD) return 1;

    return 2;
  }


  function cellUp(index) { return UP_CARDS[index % 10]; }


  // The value of the pair, 1 for a pair of aces
  function cellPair(index) { return Math.floor(index / 10) + 1; }


  // The low card of a soft hand, so 2 is A,2 and 9 is A,9
  function cellSoftLow(index) { return Math.floor((index - CELL_SOFT) / 10) + 2; }


  function cellHardTotal(index) { return Math.floor((index - CELL_HARD) / 10) + 5; }


  // The hands that catch people out: the soft 18s that stand, the 16s that
  // do not, 15 against a ten, 11 against an ace and a pair of 9s against a 7
  function cellIsTricky(index) {

    const up = cellUp(index);
    const kind = cellKind(index);

    if (kind === 1 && cellSoftLow(index) === 7) {
      return up === 2 || up === 1 || up >= 7;
    }

    if (kind === 2) {

      const total = cellHardTotal(index);

      if (total === 16) return up === 1 || up >= 9;
      if (total === 15) return up === 10;
      if (total === 11) return up === 1;
    }

    if (kind === 0 && cellPair(index) === 9) {
      return up === 7;
    }

    return false;
  }


  // Which band a hand belongs to, from the hands that come up most to the
  // ones that need the most thought
  function cellLevel(index) {

    if (cellIsTricky(index)) return 5;

    const kind = cellKind(index);

    if (kind === 0) return 4;   // pairs
    if (kind === 1) return 3;   // soft hands

    const total = cellHardTotal(index);

    if (total <= 8) return 1;   // nothing to get wrong
    if (total <= 11) return 2;  // the doubles

    const up = cellUp(index);

    return (up === 1 || up >= 7) ? 2 : 1;
  }


  // --------------------------------------------------
  // STORAGE
  // The high score is the only thing that survives a reload, the way the
  // sketch keeps it in EEPROM. The bankroll starts again at $100 on every
  // load.
  // --------------------------------------------------

  function readStore(key) {

    try {
      return (typeof localStorage !== 'undefined') ? localStorage.getItem(key) : null;
    } catch (error) {
      return null;
    }
  }


  function writeStore(key, value) {

    try {
      if (typeof localStorage !== 'undefined') localStorage.setItem(key, value);
    } catch (error) {
      // Private mode or a full store: the run just does not survive a reload
    }
  }


  function loadStoredHighscore() {

    const raw = readStore(HIGH_KEY);
    const value = raw === null ? NaN : Number(raw);

    return (Number.isFinite(value) && value >= 0) ? Math.floor(value) : -1;
  }


  // --------------------------------------------------
  // GAME
  // --------------------------------------------------

  function createGame(options) {

    options = options || {};

    const rng = (typeof options.rng === 'function') ? options.rng : Math.random;

    const shoe = new Uint8Array(SHOE_SIZE);
    let shoePosition = 0;

    let bankroll = STARTING_BANKROLL;
    let highscore = 0;

    // Latched by ui.js the instant the switch moves
    let mode = 'train';

    // The bet set for the next hand, kept from hand to hand until it is
    // changed again. Not stored, so a run starts on $10.
    let betAmount = BET;

    // Train mode score, one count per decision. Not stored anywhere and
    // reset every time a train session starts.
    let correctDecisions = 0;
    let totalDecisions = 0;

    // The drill: one byte per chart hand, kept in the store, plus the hand
    // asked about this round and the band it may have just finished
    const mastery = new Uint8Array(CELL_COUNT);
    let masteredCells = 0;
    let drillCell = 0;
    let levelUp = 0;

    // The four hands of a round, as in the sketch: slots, not a growing
    // list, so a split shifts the later hands along
    const hands = [emptyHand(), emptyHand(), emptyHand(), emptyHand()];

    const dealer = emptyHand();

    let dealerUp = 0;
    let dealerHole = 0;


    // ------------------------------------------------
    // CARD SHOE
    // ------------------------------------------------

    function initialiseShoe() {

      let index = 0;

      for (let deck = 0; deck < DECK_COUNT; deck++) {

        // Four cards of each value from Ace to 9
        for (let value = 1; value <= 9; value++) {

          for (let i = 0; i < 4; i++) {
            shoe[index++] = value;
          }
        }

        // Sixteen 10-value cards per deck
        for (let i = 0; i < 16; i++) {
          shoe[index++] = 10;
        }
      }

      // Fisher-Yates shuffle
      for (let i = SHOE_SIZE - 1; i > 0; i--) {

        const j = Math.floor(rng() * (i + 1));

        const temp = shoe[i];
        shoe[i] = shoe[j];
        shoe[j] = temp;
      }

      shoePosition = 0;
    }


    function shoeIsLow() {

      return (SHOE_SIZE - shoePosition) < RESHUFFLE_AT;
    }


    // Draw a card from the shoe, removing it permanently
    function drawCard() {

      if (shoePosition >= SHOE_SIZE) {
        initialiseShoe();
      }

      return shoe[shoePosition++];
    }


    // ------------------------------------------------
    // BETTING
    // ------------------------------------------------

    // The bet that goes on the next hand, kept inside what the bank can
    // cover and under the ceiling. Nothing under the minimum can be bet
    // while the bank holds that much, and nothing above the bank at all.
    // A bank too small for the minimum goes on the table whole: $3 staked
    // is a better end to a run than $3 sat out.
    function chosenBet() {

      if (bankroll <= 0) return 0;

      if (bankroll < MIN_BET) return bankroll;

      if (betAmount < MIN_BET) return MIN_BET;

      if (betAmount > bankroll) return bankroll;

      if (betAmount > MAX_BET) return MAX_BET;

      return betAmount;
    }


    // One button's worth of change, held to the same limits: a double that
    // would pass the bank stops at the bank, and a half that would fall
    // under the minimum stops at the minimum. Returns false when the press
    // asked for more than the bank can cover, more than the ceiling allows
    // or less than the minimum, which the caller answers with the usual two
    // blinks so no end of the range is ever silent.
    function changeBet(button) {

      // An all in bet has nothing to move: there is no room above it and
      // the minimum is already out of reach, so every press is refused
      if (bankroll < MIN_BET) {

        betAmount = chosenBet();
        return false;
      }

      let bet = chosenBet();

      if (button === 'H') bet += BET_STEP;
      else if (button === 'S') bet -= BET_STEP;
      else if (button === 'D') bet *= 2;
      else if (button === 'P') bet = Math.floor(bet / 2);

      // The ceiling and the bank are two separate reasons to stop, and
      // either one refuses the press
      const overBank = bet > bankroll;
      const overMax = bet > MAX_BET;
      const underMin = bet < MIN_BET;

      if (overBank) bet = bankroll;
      if (overMax) bet = MAX_BET;
      if (underMin) bet = MIN_BET;

      betAmount = bet;

      return !overBank && !overMax && !underMin;
    }


    // What one bet costs this round, which is the bet the player has set.
    // Train mode has no bank, so there every hand is the same $10.
    function betSize() {

      if (mode === 'train') {
        return BET;
      }

      return chosenBet();
    }


    // Money not already riding on the table this round. A double or a
    // split needs another bet out of this, which is what stops the
    // bankroll from running below 0.
    function moneyLeft(handCount) {

      let committed = 0;

      for (let i = 0; i < handCount; i++) {
        committed += hands[i].wager;
      }

      return bankroll - committed;
    }


    // ------------------------------------------------
    // SPLITTING
    // ------------------------------------------------

    // The second card of the pair becomes its own one card hand. Returns
    // the new number of hands in the round.
    function splitHand(index, handCount, wager) {

      const hand = hands[index];
      const pair = hand.pairValue;

      const newHand = emptyHand();
      addCard(newHand, pair);
      newHand.wager = wager;

      // Take that card back off the original hand
      hand.hard -= pair;
      hand.count = 1;
      hand.pairValue = 0;
      hand.first = pair;

      // One ace went to the new hand
      hand.aces = (pair === 1) ? 1 : 0;

      // Make room for the new hand
      for (let i = handCount; i > index + 1; i--) {
        hands[i] = hands[i - 1];
      }

      hands[index + 1] = newHand;

      // Neither half of a split can make a natural
      hands[index].fromSplit = true;
      hands[index + 1].fromSplit = true;

      if (pair === 1) {

        // Split aces get one card each and nothing more
        hands[index].aceSplit = true;
        hands[index + 1].aceSplit = true;
      }

      return handCount + 1;
    }


    // ------------------------------------------------
    // MONEY
    // ------------------------------------------------

    function setMode(next) {

      mode = (next === 'play') ? 'play' : 'train';
    }


    function saveHighscore() {

      writeStore(HIGH_KEY, String(highscore));
    }


    function saveMastery() {

      writeStore(MASTERY_KEY, Array.prototype.join.call(mastery, ','));
    }


    function loadMastery() {

      const raw = readStore(MASTERY_KEY);

      masteredCells = 0;

      if (!raw) return;

      const parts = raw.split(',');

      for (let i = 0; i < CELL_COUNT; i++) {

        const value = Number(parts[i]);
        const kept = (Number.isFinite(value) && value >= 0 && value <= MASTERY_MAX)
          ? Math.floor(value)
          : 0;

        mastery[i] = kept;

        if (kept >= MASTERY_MAX) {
          masteredCells++;
        }
      }
    }


    // A blank store reads back as -1
    function loadHighscore() {

      highscore = loadStoredHighscore();

      if (highscore < 0) {
        highscore = 0;
      }

      // Every load starts a fresh run at $100
      bankroll = STARTING_BANKROLL;
    }


    // The change one hand makes to the bank. Train mode plays for accuracy,
    // not money.
    function outcomeFor(hand, dealerHand) {

      const playerTotal = handTotal(hand);
      const dealerTotal = handTotal(dealerHand);
      const playerNatural = handIsNatural(hand);
      const dealerNatural = handIsNatural(dealerHand);

      const wager = hand.wager;

      let outcome = 'push';
      let change = 0;

      if (playerTotal > 21) {

        outcome = 'lose';
        change = -wager;

      } else if (playerNatural && dealerNatural) {

        outcome = 'push';

      } else if (dealerNatural) {

        // A natural beats any 21 made with three or more cards
        outcome = 'lose';
        change = -wager;

      } else if (playerNatural) {

        // A natural beats a dealer 21 made with three or more cards, and
        // pays 3:2. Rounding is down to the cent, which only ever costs
        // half a cent on an odd wager.
        outcome = 'win';
        change = wager + Math.floor(wager / 2);

      } else if (dealerTotal > 21) {

        outcome = 'win';
        change = wager;

      } else if (playerTotal > dealerTotal) {

        outcome = 'win';
        change = wager;

      } else if (playerTotal < dealerTotal) {

        outcome = 'lose';
        change = -wager;
      }

      if (mode === 'play') {
        bankroll += change;
      }

      return outcome;
    }


    // Run after the last hand is paid: keep the bank sane, remember the
    // best one and carry the balance over to the next session
    function finishRound() {

      if (mode !== 'play') return;

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
        saveHighscore();
      }
    }


    // The bankroll the display shows: cents in, dollars out
    function displayBankroll() {

      return Math.floor(bankroll / 100);
    }


    function displayHighscore() {

      return Math.floor(highscore / 100);
    }


    // Out of money: a press starts a fresh bank
    function restartBank() {

      bankroll = STARTING_BANKROLL;
      betAmount = BET;
    }


    // The jackpot is a finished run: it comes back playable at 100, because
    // nothing of the run is stored
    function finishEgg() {

      bankroll = STARTING_BANKROLL;
    }


    // ------------------------------------------------
    // TRAIN MODE TALLY
    // ------------------------------------------------

    function resetAccuracy() {

      correctDecisions = 0;
      totalDecisions = 0;
    }


    // Marks one decision against the book and returns whether it was right.
    // The hand and the number of hands are passed in because the hand a round
    // is showing moves as it is played out; without them the drill's own hand
    // is used, which is what a single decision on the dealt hand is.
    function grade(chosen, best, hand, handCount) {

      const correct = (chosen === best);

      totalDecisions++;

      if (correct) {
        correctDecisions++;
      }

      // Every decision is stored against the row the hand stood on when the
      // answer was given, so the 12 that a hit makes out of an 8 is learned
      // as well as the 8 was. A hand the chart has no row for, such as a
      // bust or a 21, counts for the accuracy only.
      const cell = cellForHand(hand || hands[0], dealerUp, handCount || 1);

      if (cell !== NO_CELL) {
        recordDrill(cell, correct);
      }

      return correct;
    }


    // Accuracy as a percentage with two decimals, 0 to 10000
    function accuracyHundredths() {

      if (totalDecisions === 0) return 0;

      return Math.floor((correctDecisions * 10000) / totalDecisions);
    }


    // ------------------------------------------------
    // THE DRILL
    // ------------------------------------------------

    // The first band with anything left to master, or 0 when the whole
    // chart is mastered
    function firstUnmasteredLevel() {

      for (let level = 1; level <= 5; level++) {

        for (let i = 0; i < CELL_COUNT; i++) {

          if (cellLevel(i) === level && mastery[i] < MASTERY_MAX) {
            return level;
          }
        }
      }

      return 0;
    }


    // Four for a hand never answered, down to one for a hand a single right
    // answer from mastered
    function cellWeight(index) {

      const value = mastery[index];

      return (value >= MASTERY_MAX) ? 0 : MASTERY_MAX - value + 1;
    }


    // The hand to deal: from the first band not yet mastered, weighted
    // towards the hands answered wrong most often, with the odd hand from
    // a band below so the easy ones stay learned
    function chooseDrillCell() {

      const level = firstUnmasteredLevel();

      // Everything is mastered: the drill carries on evenly
      if (level === 0) return Math.floor(rng() * CELL_COUNT);

      const review = (level > 1) && (Math.floor(rng() * REVIEW_EVERY) === 0);
      let total = 0;

      for (let i = 0; i < CELL_COUNT; i++) {

        if (review) {

          if (cellLevel(i) < level && mastery[i] >= MASTERY_MAX) total++;

        } else if (cellLevel(i) === level) {

          total += cellWeight(i);
        }
      }

      if (total === 0) return Math.floor(rng() * CELL_COUNT);

      let pick = Math.floor(rng() * total);

      for (let i = 0; i < CELL_COUNT; i++) {

        if (review) {

          if (cellLevel(i) < level && mastery[i] >= MASTERY_MAX) {

            if (pick === 0) return i;

            pick--;
          }

        } else if (cellLevel(i) === level) {

          const weight = cellWeight(i);

          if (pick < weight) return i;

          pick -= weight;
        }
      }

      return Math.floor(rng() * CELL_COUNT);
    }


    // The two cards that make the chosen hand, and the dealer's up card.
    // Hard totals are built from two different values, so that no pair
    // stands in for a total the chart prints on a row of its own.
    function cellCards(index) {

      const kind = cellKind(index);
      let first;
      let second;

      if (kind === 0) {

        first = cellPair(index);
        second = first;

      } else if (kind === 1) {

        first = 1;
        second = cellSoftLow(index);

      } else {

        const total = cellHardTotal(index);
        const options = [];

        for (let a = 2; a <= 9; a++) {

          const b = total - a;

          if (b >= 2 && b <= 10 && b !== a) options.push([a, b]);
        }

        const pick = options[Math.floor(rng() * options.length)] || [2, total - 2];

        first = pick[0];
        second = pick[1];
      }

      return { first: first, second: second, up: cellUp(index) };
    }


    // Where an up card sits in a chart row: the ten columns run 2 to 9,
    // then 10, then the ace
    function upIndex(up) {

      if (up === 1 || up === 11) return 9;

      return up - 2;
    }


    // The chart row a hand is standing on, or NO_CELL when the chart has
    // none for it. A hand that can still be split is asked about on its pair
    // row, which is the row bestAction grades it against; once it cannot be
    // split, the total is what the book has to say.
    function cellForHand(hand, up, handCount) {

      if (hand.count < 2 || handIsBust(hand)) return NO_CELL;

      if (hand.count === 2 && hand.pairValue !== 0 && canSplit(hand, handCount)) {
        return (hand.pairValue - 1) * 10 + upIndex(up);
      }

      const total = handTotal(hand);

      if (handIsSoft(hand)) {

        // Soft 13 to 20 have rows of their own; a soft 21 stands
        if (total < 13 || total > 20) return NO_CELL;

        return CELL_SOFT + (total - 13) * 10 + upIndex(up);
      }

      // Hard 5 to 16 are the totals with a decision in them; 17 and up
      // stand, and nothing below 5 can be made of two cards
      if (total < 5 || total > 16) return NO_CELL;

      return CELL_HARD + (total - 5) * 10 + upIndex(up);
    }


    // One answer, stored against the chart row it was given on. Returns the
    // band number when that answer was the last one the band needed.
    function recordDrill(cell, correct) {

      const before = mastery[cell];
      let after;

      if (correct) {

        after = (before < MASTERY_MAX) ? before + 1 : before;

      } else {

        // Three in a row, so a miss starts the count again. A hand that was
        // already mastered only drops one step.
        after = (before >= MASTERY_MAX) ? 1 : 0;
      }

      if (after === before) return 0;

      mastery[cell] = after;

      if (before < MASTERY_MAX && after >= MASTERY_MAX) masteredCells++;
      if (before >= MASTERY_MAX && after < MASTERY_MAX) masteredCells--;

      saveMastery();

      const level = cellLevel(cell);

      for (let i = 0; i < CELL_COUNT; i++) {

        if (cellLevel(i) === level && mastery[i] < MASTERY_MAX) return 0;
      }

      levelUp = level;

      return level;
    }


    // How much of the chart is mastered, in hundredths of a percent
    function masteryHundredths() {

      return Math.floor((masteredCells * 10000) / CELL_COUNT);
    }


    // ------------------------------------------------
    // ROUND
    // ------------------------------------------------

    // Deal a fresh round: the dealer takes its two cards first, then the
    // player, exactly as the sketch draws them
    function startRound() {

      clearHand(hands[0]);

      if (mode === 'train') {

        // The drill deals the hand it wants to ask about: the chart cell
        // decides the player's two cards and the dealer's up card, and
        // only the hole card and the hits come off the shoe
        drillCell = chooseDrillCell();

        const dealt = cellCards(drillCell);

        dealerUp = dealt.up;
        dealerHole = drawCard();

        addCard(hands[0], dealt.first);
        addCard(hands[0], dealt.second);

      } else {

        dealerUp = drawCard();
        dealerHole = drawCard();

        addCard(hands[0], drawCard());
        addCard(hands[0], drawCard());
      }

      hands[0].wager = betSize();

      // Only the visible card is known until the hole card is turned
      clearHand(dealer);
      addCard(dealer, dealerUp);
    }


    // Boot, as setup() does it
    loadHighscore();
    loadMastery();
    initialiseShoe();


    return {
      // shoe
      initialiseShoe: initialiseShoe,
      shoeIsLow: shoeIsLow,
      drawCard: drawCard,
      shoeRemaining: function () { return SHOE_SIZE - shoePosition; },

      // hands
      hands: hands,
      dealer: dealer,
      clearHand: clearHand,
      addCard: addCard,
      handTotal: handTotal,
      handIsSoft: handIsSoft,
      handIsBust: handIsBust,
      handIsNatural: handIsNatural,
      cardValue: cardValue,
      canDouble: canDouble,
      canSplit: canSplit,
      bestAction: bestAction,
      splitHand: splitHand,
      startRound: startRound,
      dealerUp: function () { return dealerUp; },
      dealerHole: function () { return dealerHole; },

      // betting and money
      setMode: setMode,
      mode: function () { return mode; },
      betSize: betSize,
      betAmount: function () { return betAmount; },
      changeBet: changeBet,
      moneyLeft: moneyLeft,
      bankroll: function () { return bankroll; },
      highscore: function () { return highscore; },
      displayBankroll: displayBankroll,
      displayHighscore: displayHighscore,
      outcomeFor: outcomeFor,
      finishRound: finishRound,
      restartBank: restartBank,
      finishEgg: finishEgg,
      jackpot: function () { return mode === 'play' && bankroll >= MAX_BANK; },

      // train mode
      resetAccuracy: resetAccuracy,
      grade: grade,
      accuracyHundredths: accuracyHundredths,

      // drill
      masteryHundredths: masteryHundredths,
      cellLevel: cellLevel,
      cellCards: cellCards,
      drillCell: function () { return drillCell; },
      drillLevel: function () { return firstUnmasteredLevel() || 5; },
      takeLevelUp: function () { const level = levelUp; levelUp = 0; return level; },
      decisions: function () {
        return { correct: correctDecisions, total: totalDecisions };
      }
    };
  }


  return {
    createGame: createGame,
    bestAction: bestAction,
    handTotal: handTotal,
    handIsSoft: handIsSoft,
    handIsNatural: handIsNatural,
    canSplit: canSplit,
    canDouble: canDouble,
    constants: {
      DECK_COUNT: DECK_COUNT,
      SHOE_SIZE: SHOE_SIZE,
      RESHUFFLE_AT: RESHUFFLE_AT,
      MAX_HANDS: MAX_HANDS,
      STARTING_BANKROLL: STARTING_BANKROLL,
      BET: BET,
      MIN_BET: MIN_BET,
      MAX_BET: MAX_BET,
      BET_STEP: BET_STEP,
      MAX_BANK: MAX_BANK
    }
  };
});
