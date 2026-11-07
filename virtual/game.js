/*
  BLACKJACK STRATEGY TRAINER - browser engine

  A port of firmware/blackjack-keychain/blackjack-keychain.ino. The rules,
  the six deck shoe, the split/double restrictions and the basic strategy
  table are copied from the sketch, so this plays exactly the same game:

    - 6 deck shoe, reshuffled between hands when it runs low
    - dealer stands on soft 17
    - double on any first two cards, double after split allowed
    - up to 4 hands by splitting (no resplitting aces, split aces get
      one card only)
    - a two card 21 is a natural and is not asked to act
    - a natural beats any 21 made with three or more cards
    - two naturals push, two ordinary 21s push as normal
    - $100 to start, a flat $10 a hand, a split or a double costs $10 more
    - wins pay even money, pushes return the bet
    - the game ends when $10 can no longer be covered

  The play/train switch (SW6 on the board) picks what the session keeps.
  Train marks every choice against basic strategy, keeps that accuracy
  tally and wagers nothing, so a session runs until it is restarted.
  Play keeps no tally, wagers the $100 bankroll at a flat $10, and stops
  when a bet can no longer be covered.

  Everything is exposed through createGame(): startRound(), act(),
  playDealer(), canAct(), restart() and getState(). Every step calls the
  onEvent listener so the display can follow along.
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
  const SHOE_SIZE = DECK_COUNT * 52;
  const RESHUFFLE_AT = 40;
  const MAX_HANDS = 4;
  const STARTING_BANKROLL = 100;
  const BET = 10;

  const ACTION_NAMES = { H: 'HIT', S: 'STAND', D: 'DOUBLE', P: 'SPLIT' };


  // --------------------------------------------------
  // HAND CALCULATIONS
  // Cards are kept as values: 1 = Ace, 2-9 as themselves, 10 for
  // 10/J/Q/K. The card list is kept as well, but only so the screen
  // can show ranks; the totals never look at it.
  // --------------------------------------------------

  function emptyHand() {

    return {
      hard: 0,        // total with every ace counted as 1
      aces: 0,
      count: 0,
      first: 0,
      wager: 0,
      pairValue: 0,
      aceSplit: false,
      cards: []
    };
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
    hand.cards.push(card);

    if (hand.count === 2) {

      hand.pairValue = (card === hand.first) ? card : 0;

    } else if (hand.count > 2) {

      hand.pairValue = 0;
    }
  }


  function handTotal(hand) {

    let total = hand.hard;

    if (hand.aces > 0 && total + 10 <= 21) {
      total += 10;
    }

    return total;
  }


  function handIsSoft(hand) {

    return hand.aces > 0 && hand.hard + 10 <= 21;
  }


  function handIsBust(hand) {

    return handTotal(hand) > 21;
  }


  function handIsNatural(hand) {

    return hand.count === 2 && !hand.aceSplit && handTotal(hand) === 21;
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

    if (pair === 1 || pair === 8) return true;   // always split aces and 8s
    if (pair === 5 || pair === 10) return false; // never split 5s or 10s

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
    const up = (dealerUp === 1) ? 11 : dealerUp;

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
  // GAME
  // --------------------------------------------------

  function createGame(options) {

    options = options || {};

    const rng = (typeof options.rng === 'function') ? options.rng : Math.random;
    const listener = (typeof options.onEvent === 'function') ? options.onEvent : null;

    const state = {
      shoe: new Uint8Array(SHOE_SIZE),
      shoePosition: 0,
      bankroll: STARTING_BANKROLL,
      gameOver: false,
      phase: 'idle',        // idle | player | dealer | results | gameover
      mode: 'train',        // SW6: train grades, play does not
      hands: [],
      current: 0,
      dealer: null,
      dealerUp: 0,
      dealerHole: 0,
      correctDecisions: 0,
      totalDecisions: 0,
      lastGrade: null,
      results: []
    };


    function notify(event) {

      if (listener) {
        listener(event);
      }
    }


    function log(text, cls) {

      notify({ type: 'log', text: text, cls: cls || '' });
    }


    function cardName(card) {

      return card === 1 ? 'A' : String(card);
    }


    // ------------------------------------------------
    // CARD SHOE
    // ------------------------------------------------

    function initialiseShoe() {

      let index = 0;

      for (let deck = 0; deck < DECK_COUNT; deck++) {

        // Four cards of each value from Ace to 9
        for (let value = 1; value <= 9; value++) {

          for (let i = 0; i < 4; i++) {
            state.shoe[index++] = value;
          }
        }

        // Sixteen 10-value cards per deck
        for (let i = 0; i < 16; i++) {
          state.shoe[index++] = 10;
        }
      }

      // Fisher-Yates shuffle
      for (let i = SHOE_SIZE - 1; i > 0; i--) {

        const j = Math.floor(rng() * (i + 1));

        const temp = state.shoe[i];
        state.shoe[i] = state.shoe[j];
        state.shoe[j] = temp;
      }

      state.shoePosition = 0;
    }


    function shoeIsLow() {

      return (SHOE_SIZE - state.shoePosition) < RESHUFFLE_AT;
    }


    function drawCard() {

      if (state.shoePosition >= SHOE_SIZE) {
        initialiseShoe();
      }

      return state.shoe[state.shoePosition++];
    }


    // ------------------------------------------------
    // LOG HELPERS, mirroring the serial output
    // ------------------------------------------------

    function logHandLabel(index) {

      return state.hands.length > 1 ? 'Hand ' + (index + 1) + ': ' : 'Player: ';
    }


    function logPlayerHand(index) {

      const hand = state.hands[index];
      let text = logHandLabel(index) + handTotal(hand);

      if (hand.pairValue !== 0) text += ' Pair';
      if (handIsSoft(hand)) text += ' Soft';

      log(text);
    }


    function logDraw(card, index) {

      const prefix = state.hands.length > 1 ? 'Hand ' + (index + 1) + ' draws: ' : 'Card: ';
      log(prefix + cardName(card));
    }


    // ------------------------------------------------
    // SPLITTING
    // ------------------------------------------------

    function splitHand(index) {

      const hand = state.hands[index];
      const pair = hand.pairValue;
      const secondCard = hand.cards[1];

      // The second card of the pair becomes its own one card hand
      const newHand = emptyHand();
      addCard(newHand, secondCard);
      newHand.wager = BET;

      // Take that card back off the original hand
      hand.hard -= pair;
      hand.count = 1;
      hand.cards = [hand.cards[0]];
      hand.pairValue = 0;
      hand.first = pair;

      // One ace went to the new hand
      hand.aces = (pair === 1) ? 1 : 0;

      state.hands.splice(index + 1, 0, newHand);

      if (pair === 1) {

        // Split aces get one card each and nothing more
        state.hands[index].aceSplit = true;
        state.hands[index + 1].aceSplit = true;
      }
    }


    // ------------------------------------------------
    // PLAYER PHASE
    // ------------------------------------------------

    // Walk forward until a hand needs a decision, then wait for input.
    function advance() {

      while (state.current < state.hands.length) {

        const hand = state.hands[state.current];

        // A freshly split hand holds one card and needs a second
        if (hand.count === 1) {

          const card = drawCard();
          addCard(hand, card);

          logDraw(card, state.current);

          if (hand.aceSplit) {

            // Split aces are played with that single card
            state.current++;
            continue;
          }
        }

        logPlayerHand(state.current);

        // A natural blackjack has nothing to decide
        if (handIsNatural(hand)) {

          log('Blackjack!', 'win');
          state.current++;
          continue;
        }

        state.phase = 'player';
        notify({ type: 'prompt' });
        notify({ type: 'update' });
        return;
      }

      state.phase = 'dealer';
      notify({ type: 'dealerReady' });
      notify({ type: 'update' });
    }


    function startRound() {

      if (state.gameOver) return false;
      if (state.phase === 'player' || state.phase === 'dealer') return false;

      if (shoeIsLow()) {

        log('');
        log('--- Shuffling a fresh shoe ---', 'dim');
        initialiseShoe();
      }

      state.hands = [emptyHand()];
      state.results = [];
      state.lastGrade = null;
      state.current = 0;

      state.dealerUp = drawCard();
      state.dealerHole = drawCard();

      addCard(state.hands[0], drawCard());
      addCard(state.hands[0], drawCard());

      state.hands[0].wager = BET;

      // Dealer's visible card only, the hole card is added at the end
      state.dealer = emptyHand();
      addCard(state.dealer, state.dealerUp);

      log('');
      log('==============================', 'sep');
      log('Bankroll: $' + state.bankroll + '  Bet: $' + BET);
      log('Dealer: ' + (state.dealerUp === 1 ? 11 : state.dealerUp));

      notify({ type: 'roundStart' });
      advance();

      return true;
    }


    function act(action) {

      if (state.phase !== 'player') return false;

      const hand = state.hands[state.current];
      if (!hand) return false;

      // Reject actions this hand cannot make, without grading them
      if (action === 'D' && !canDouble(hand)) {

        log('Cannot double after hitting. Choose again.', 'dim');
        notify({ type: 'update' });
        return false;
      }

      if (action === 'P' && !canSplit(hand, state.hands.length)) {

        log('Cannot split this hand. Choose again.', 'dim');
        notify({ type: 'update' });
        return false;
      }

      log(ACTION_NAMES[action], 'action');

      // Only the train position of the switch marks the decision
      if (state.mode === 'train') {

        const best = bestAction(hand, state.dealerUp, state.hands.length);
        const correct = (action === best);

        state.totalDecisions++;

        if (correct) {
          state.correctDecisions++;
        }

        state.lastGrade = { correct: correct, chosen: action, best: best };

        if (correct) {

          log('CORRECT', 'correct');

        } else {

          log('INCORRECT', 'wrong');
          log('Correct action: ' + ACTION_NAMES[best], 'wrong');
        }

        notify({ type: 'grade', correct: correct, chosen: action, best: best });
      }

      if (action === 'S') {

        state.current++;
        advance();
        return true;
      }

      if (action === 'P') {

        splitHand(state.current);
        advance();
        return true;
      }

      // Doubling puts another bet on the hand
      if (action === 'D') {
        hand.wager += BET;
      }

      // Hit or double both draw one card
      const card = drawCard();
      addCard(hand, card);

      logDraw(card, state.current);

      if (handIsBust(hand)) {

        log('Bust!', 'wrong');
        state.current++;
        advance();
        return true;
      }

      if (handTotal(hand) === 21) {

        log('21, standing.');
        state.current++;
        advance();
        return true;
      }

      if (action === 'D') {

        state.current++;
        advance();
        return true;
      }

      // Otherwise this hand is asked again
      notify({ type: 'prompt' });
      notify({ type: 'update' });

      return true;
    }


    // ------------------------------------------------
    // DEALER'S TURN AND RESULT
    // ------------------------------------------------

    function playDealer() {

      if (state.phase !== 'dealer') return false;

      state.phase = 'dealer-playing';

      addCard(state.dealer, state.dealerHole);

      log('Dealer hole card: ' + cardName(state.dealerHole) +
          ' (dealer total ' + handTotal(state.dealer) + ')');

      if (handIsNatural(state.dealer)) {
        log('Dealer has blackjack.', 'wrong');
      }

      // Dealer stands on all 17s, including soft 17
      while (handTotal(state.dealer) < 17) {

        const card = drawCard();
        addCard(state.dealer, card);

        log('Dealer draws: ' + cardName(card) +
            ' (dealer total ' + handTotal(state.dealer) + ')');

        notify({ type: 'update' });
      }

      settle();
      return true;
    }


    function settle() {

      const dealerTotal = handTotal(state.dealer);
      const dealerNatural = handIsNatural(state.dealer);
      const handCount = state.hands.length;
      const results = [];

      // Train keeps the accuracy tally, play keeps the bankroll
      const move = function (delta) {
        if (state.mode === 'play') state.bankroll += delta;
      };

      log('');

      state.hands.forEach(function (hand, i) {

        const playerTotal = handTotal(hand);
        const playerNatural = handIsNatural(hand);
        const wager = hand.wager;

        const prefix = handCount > 1 ? 'Hand ' + (i + 1) + ': ' : '';

        let outcome;
        let text;

        if (playerTotal > 21) {

          outcome = 'lose';
          text = 'Player busts. Dealer wins.';
          move(-wager);

        } else if (playerNatural && dealerNatural) {

          outcome = 'push';
          text = 'Both blackjack. Push.';

        } else if (dealerNatural) {

          // A natural beats any 21 made with three or more cards
          outcome = 'lose';
          text = 'Dealer blackjack. Dealer wins.';
          move(-wager);

        } else if (playerNatural) {

          // A natural beats a dealer 21 made with three or more cards
          outcome = 'win';
          text = 'Player blackjack. Player wins.';
          move(wager);

        } else if (dealerTotal > 21) {

          outcome = 'win';
          text = 'Dealer busts. Player wins.';
          move(wager);

        } else if (playerTotal > dealerTotal) {

          outcome = 'win';
          text = 'Player wins.';
          move(wager);

        } else if (playerTotal < dealerTotal) {

          outcome = 'lose';
          text = 'Dealer wins.';
          move(-wager);

        } else {

          outcome = 'push';
          text = 'Push.';
        }

        results.push({
          index: i,
          outcome: outcome,
          total: playerTotal,
          wager: wager,
          text: prefix + text
        });

        log(prefix + text, outcome);
      });

      state.results = results;

      log('');
      log('Bankroll: $' + state.bankroll);
      log('');

      log('Decisions: ' + state.correctDecisions + '/' + state.totalDecisions);

      // A blackjack round asks nothing, so guard the percentage
      if (state.totalDecisions > 0) {

        log('Accuracy: ' +
            Math.floor((state.correctDecisions * 100) / state.totalDecisions) + '%');
      }

      log('Cards remaining: ' + (SHOE_SIZE - state.shoePosition));

      // Only the play position wagers, so only play can go broke
      if (state.mode === 'play' && state.bankroll < BET) {

        log('');
        log('Out of money. Game over.', 'wrong');
        state.gameOver = true;
        state.phase = 'gameover';

      } else {

        state.phase = 'results';
      }

      notify({ type: 'settle', results: results });
      notify({ type: 'update' });
    }


    // ------------------------------------------------
    // PUBLIC STATE
    // ------------------------------------------------

    function snapshotHand(hand) {

      return {
        total: handTotal(hand),
        soft: handIsSoft(hand),
        bust: handIsBust(hand),
        pair: hand.pairValue,
        count: hand.count,
        wager: hand.wager,
        cards: hand.cards.slice(),
        natural: handIsNatural(hand),
        aceSplit: hand.aceSplit
      };
    }


    function getState() {

      const hands = state.hands.map(snapshotHand);
      const dealer = state.dealer ? snapshotHand(state.dealer) : null;

      return {
        phase: state.phase,
        mode: state.mode,
        bankroll: state.bankroll,
        bet: BET,
        gameOver: state.gameOver,
        hands: hands,
        handCount: hands.length,
        current: state.current,
        dealer: dealer,
        accuracy: {
          correct: state.correctDecisions,
          total: state.totalDecisions,
          percent: state.totalDecisions
            ? Math.floor((state.correctDecisions * 100) / state.totalDecisions)
            : 0
        },
        shoeRemaining: SHOE_SIZE - state.shoePosition,
        results: state.results,
        lastGrade: state.lastGrade,
        inPlay: hands.reduce(function (sum, hand) { return sum + hand.wager; }, 0)
      };
    }


    function canAct(action) {

      if (state.phase !== 'player') return false;

      const hand = state.hands[state.current];
      if (!hand) return false;

      if (action === 'D') return canDouble(hand);
      if (action === 'P') return canSplit(hand, state.hands.length);

      return action === 'H' || action === 'S';
    }


    function setMode(mode) {

      const next = (mode === 'play') ? 'play' : 'train';

      if (state.mode !== next) {

        state.mode = next;
        state.lastGrade = null;

        notify({ type: 'mode', mode: next });
      }

      return state.mode;
    }


    function restart() {

      state.bankroll = STARTING_BANKROLL;
      state.gameOver = false;
      state.phase = 'idle';
      state.hands = [];
      state.current = 0;
      state.dealer = null;
      state.results = [];
      state.lastGrade = null;
      state.correctDecisions = 0;
      state.totalDecisions = 0;

      initialiseShoe();

      log('');
      log('--- New game ---', 'dim');
      log('Starting bankroll: $' + STARTING_BANKROLL + '  Bet: $' + BET);

      notify({ type: 'roundStart' });
      return startRound();
    }


    // Setup, as in the sketch
    initialiseShoe();

    log('');
    log('BLACKJACK STRATEGY TRAINER', 'header');
    log('6 deck shoe | S17 | DAS');
    log('Starting bankroll: $' + STARTING_BANKROLL + '  Bet: $' + BET);

    return {
      startRound: startRound,
      act: act,
      playDealer: playDealer,
      canAct: canAct,
      setMode: setMode,
      restart: restart,
      getState: getState
    };
  }


  return {
    createGame: createGame,
    bestAction: bestAction,
    handTotal: handTotal,
    handIsSoft: handIsSoft,
    canSplit: canSplit,
    canDouble: canDouble,
    constants: {
      DECK_COUNT: DECK_COUNT,
      SHOE_SIZE: SHOE_SIZE,
      RESHUFFLE_AT: RESHUFFLE_AT,
      MAX_HANDS: MAX_HANDS,
      STARTING_BANKROLL: STARTING_BANKROLL,
      BET: BET
    }
  };
});
