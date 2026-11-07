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
    - $100 to start, a fixed $10 on every hand
    - a split hand costs another $10, a double costs another $10
    - money is counted in cents, a natural pays 3:2, and the half dollar
      it can pay on a short bet is kept (the display drops it)
    - a bank under $10 goes on the next hand whole, and only an empty
      bank stops the game
    - the bank stops at $9999, which is the jackpot
    - the bankroll and the best bank are kept between sessions

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
  const BET = 1000;                       // $10.00
  const MAX_BANK = 999900;                // $9999.00, the most four digits show

  // Bumped whenever the stored meaning changes. This one counts cents and
  // pays 3:2, so a balance left by an older build is ignored.
  const BANKROLL_VERSION = 2;

  const BANK_KEY = 'blackjack-keychain.bankroll';
  const HIGH_KEY = 'blackjack-keychain.highscore';


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

      if (total === 11) return 'D';

      if (total === 10) return (up <= 9) ? 'D' : 'H';

      if (total === 9) return (up >= 3 && up <= 6) ? 'D' : 'H';

      return 'H';
    }

    // A double is only available on the first two cards
    if (action === 'D' && !canDouble(hand)) {
      return 'H';
    }

    return action;
  }


  // --------------------------------------------------
  // STORAGE
  // The bankroll is stored with its complement behind a format tag, the
  // way the sketch keeps it in EEPROM. A write cut short can only damage
  // one value, so a damaged save reads back as a mismatch instead of a
  // wrong balance.
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


  function loadStoredBank() {

    const raw = readStore(BANK_KEY);

    if (!raw) return -1;

    let saved;

    try {
      saved = JSON.parse(raw);
    } catch (error) {
      return -1;
    }

    if (!saved || saved.version !== BANKROLL_VERSION) return -1;
    if (saved.check !== ~saved.bankroll) return -1;

    return saved.bankroll;
  }


  function saveStoredBank(bankroll) {

    writeStore(BANK_KEY, JSON.stringify({
      version: BANKROLL_VERSION,
      bankroll: bankroll,
      check: ~bankroll
    }));
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

    // What is already stored, so a hand that changed nothing costs no
    // write at all
    let savedBankroll = STARTING_BANKROLL;

    // Latched by ui.js at the start of each hand
    let mode = 'train';

    // Train mode score, one count per decision. Not stored anywhere and
    // reset every time a train session starts.
    let correctDecisions = 0;
    let totalDecisions = 0;

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

    // What one bet costs this round. The bank can fall below a full bet,
    // and then the whole of it goes on the table: $5 staked is better
    // than $5 sat out. Train mode has no bank, so it always plays full
    // bets.
    function betSize() {

      if (mode === 'train') {
        return BET;
      }

      return (bankroll < BET) ? bankroll : BET;
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


    function saveBankroll() {

      saveStoredBank(bankroll);
      savedBankroll = bankroll;
    }


    function saveHighscore() {

      writeStore(HIGH_KEY, String(highscore));
    }


    // -1 when nothing trustworthy is stored, which is what a blank store,
    // a half written pair and an older build's balance all look like
    function loadBankroll() {

      const stored = loadStoredBank();

      highscore = loadStoredHighscore();

      // A blank store, a damaged save or a balance from an older build
      // means whatever high score is left behind is in the old units too,
      // so it goes with it
      if (stored < 0) {
        highscore = 0;
        saveHighscore();
      }

      if (highscore < 0) {
        highscore = 0;
      }

      // Pick up the run where it was left. A run that finished wiped out,
      // or on the 9999 jackpot, starts over at 100.
      if (stored < 0 || stored === 0 || stored >= MAX_BANK) {

        bankroll = STARTING_BANKROLL;
        saveBankroll();

      } else {

        bankroll = stored;
        savedBankroll = bankroll;
      }
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

      // Carry the balance over to the next session, unless a push left it
      // exactly where it was
      if (bankroll !== savedBankroll) {
        saveBankroll();
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
      saveBankroll();
    }


    // The jackpot is a finished run: it comes back playable at 100 next
    // time instead of dropping straight into the egg again
    function finishEgg() {

      bankroll = STARTING_BANKROLL;
      saveBankroll();
    }


    // ------------------------------------------------
    // TRAIN MODE TALLY
    // ------------------------------------------------

    function resetAccuracy() {

      correctDecisions = 0;
      totalDecisions = 0;
    }


    // Marks one decision against the book and returns whether it was right
    function grade(chosen, best) {

      const correct = (chosen === best);

      totalDecisions++;

      if (correct) {
        correctDecisions++;
      }

      return correct;
    }


    // Accuracy as a percentage with two decimals, 0 to 10000
    function accuracyHundredths() {

      if (totalDecisions === 0) return 0;

      return Math.floor((correctDecisions * 10000) / totalDecisions);
    }


    // ------------------------------------------------
    // ROUND
    // ------------------------------------------------

    // Deal a fresh round: the dealer takes its two cards first, then the
    // player, exactly as the sketch draws them
    function startRound() {

      clearHand(hands[0]);

      dealerUp = drawCard();
      dealerHole = drawCard();

      addCard(hands[0], drawCard());
      addCard(hands[0], drawCard());

      hands[0].wager = betSize();

      // Only the visible card is known until the hole card is turned
      clearHand(dealer);
      addCard(dealer, dealerUp);
    }


    // Boot, as setup() does it
    loadBankroll();
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
      MAX_BANK: MAX_BANK
    }
  };
});
