/*
  BLACKJACK STRATEGY TRAINER - display, LEDs and the round

  The board's half of the sketch: four digits, seven LEDs, five buttons
  and the play/train switch. Everything the round does runs here, on the
  sketch's own timing, because the sketch's pauses and its display
  refresh are the same thing: holding the bank button keeps showing the
  bankroll while the dealer draws, exactly as pause() does in the sketch.
*/

(function () {

  'use strict';

  const SVG_NS = 'http://www.w3.org/2000/svg';

  // Standard seven segment layout inside a 68 x 116 box
  const SEGMENTS = {
    a: [16, 8, 52, 8],
    b: [59, 15, 59, 54],
    c: [59, 62, 59, 101],
    d: [16, 108, 52, 108],
    e: [9, 62, 9, 101],
    f: [9, 15, 9, 54],
    g: [16, 58, 52, 58]
  };

  // Bit per segment, as the sketch's font table has them
  const BITS = { a: 0x01, b: 0x02, c: 0x04, d: 0x08, e: 0x10, f: 0x20, g: 0x40 };

  const SEGMENT_BLANK = 0x00;
  const SEGMENT_DP = 0x80;

  const FONT = [
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
  ];

  // Enough letters for the four action words. A seven segment cell only
  // ever draws a rough letter, so these are the usual shapes.
  const LETTER_B = 0x7C;   // c d e f g
  const LETTER_D = 0x5E;   // b c d e g
  const LETTER_G = 0x3D;   // a c d e f, a C that closes
  const LETTER_H = 0x76;   // b c e f g
  const LETTER_I = 0x06;   // b c
  const LETTER_L = 0x38;   // d e f
  const LETTER_N = 0x54;   // c e g
  const LETTER_O = 0x3F;   // a b c d e f
  const LETTER_P = 0x73;   // a b e f g
  const LETTER_S = 0x6D;   // a c d f g
  const LETTER_T = 0x78;   // d e f g
  const LETTER_U = 0x3E;   // b c d e f

  // The label shown when the best bank is on the display
  const HIGH_WORD = [LETTER_H, LETTER_I, LETTER_G, LETTER_H];

  const ACTION_WORDS = {
    H: [LETTER_H, LETTER_I, LETTER_T],
    S: [LETTER_S, LETTER_T, LETTER_N, LETTER_D],
    D: [LETTER_D, LETTER_O, LETTER_U, LETTER_B],
    P: [LETTER_S, LETTER_P, LETTER_L, LETTER_T]
  };

  const els = {
    playerDigits: document.getElementById('playerDigits'),
    dealerDigits: document.getElementById('dealerDigits'),
    modeSwitch: document.getElementById('modeSwitch'),
    leds: {
      pair: document.getElementById('ledPair'),
      soft: document.getElementById('ledSoft'),
      win: document.getElementById('ledWin'),
      push: document.getElementById('ledPush'),
      lose: document.getElementById('ledLose'),
      correct: document.getElementById('ledCorrect'),
      wrong: document.getElementById('ledWrong')
    }
  };

  const game = window.BlackjackEngine.createGame();


  // --------------------------------------------------
  // SEVEN SEGMENT DISPLAY
  // --------------------------------------------------

  function makeDigit() {

    const svg = document.createElementNS(SVG_NS, 'svg');

    svg.setAttribute('viewBox', '0 0 68 116');
    svg.setAttribute('class', 'digit');
    svg.setAttribute('aria-hidden', 'true');

    Object.keys(SEGMENTS).forEach(function (name) {

      const s = SEGMENTS[name];
      const line = document.createElementNS(SVG_NS, 'line');

      line.setAttribute('x1', s[0]);
      line.setAttribute('y1', s[1]);
      line.setAttribute('x2', s[2]);
      line.setAttribute('y2', s[3]);
      line.dataset.bit = BITS[name];

      svg.appendChild(line);
    });

    // The decimal point has a pin of its own on the board, and only the
    // accuracy readout ever lights it
    const dp = document.createElementNS(SVG_NS, 'circle');

    // Bottom right of the cell, clear of the segment ends, where the point
    // sits on the real module
    dp.setAttribute('cx', '64');
    dp.setAttribute('cy', '104');
    dp.setAttribute('r', '3.4');
    dp.setAttribute('class', 'dp');
    dp.dataset.bit = SEGMENT_DP;

    svg.appendChild(dp);

    return svg;
  }


  const playerDigits = [makeDigit(), makeDigit()];
  const dealerDigits = [makeDigit(), makeDigit()];

  playerDigits.forEach(function (d) { els.playerDigits.appendChild(d); });
  dealerDigits.forEach(function (d) { els.dealerDigits.appendChild(d); });

  // Digits run left to right as 1 to 4: player on 1 and 2, dealer on 3 and 4
  const digits = [playerDigits[0], playerDigits[1], dealerDigits[0], dealerDigits[1]];


  function writePattern(svg, pattern) {

    svg.querySelectorAll('[data-bit]').forEach(function (element) {

      element.classList.toggle('on', (pattern & Number(element.dataset.bit)) !== 0);
    });
  }


  // --------------------------------------------------
  // WHAT THE DISPLAY SHOWS
  // --------------------------------------------------

  // What the game wants shown, one segment pattern per digit
  const displayBuffer = [SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK];

  // What the bank button shows, rebuilt whenever it is sampled
  const holdDigits = [SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK, SEGMENT_BLANK];

  const leds = {
    pair: false,
    soft: false,
    win: false,
    push: false,
    lose: false,
    correct: false,
    wrong: false
  };

  let bankDown = false;
  let bankHeldSince = 0;


  function clearDisplay() {

    for (let i = 0; i < 4; i++) {
      displayBuffer[i] = SEGMENT_BLANK;
    }
  }


  // Writes value into width digits at offset, right justified with blank
  // leading digits. Only the requested digits are touched.
  function numberToDigits(out, offset, width, value) {

    for (let i = 0; i < width; i++) {
      out[offset + i] = SEGMENT_BLANK;
    }

    if (value < 0) value = 0;

    let i = width;

    while (i > 0) {

      i--;

      out[offset + i] = FONT[value % 10];
      value = Math.floor(value / 10);

      if (value === 0) break;
    }
  }


  // Accuracy as a percentage with two decimals, 00.00 to 99.99. A
  // flawless run would need five digits plus the point, so 100 is shown
  // as 100. with the last digit blank.
  function accuracyToDigits(out) {

    const hundredths = game.accuracyHundredths();

    if (hundredths >= 10000) {

      out[0] = FONT[1];
      out[1] = FONT[0];
      out[2] = FONT[0] | SEGMENT_DP;
      out[3] = SEGMENT_BLANK;

      return;
    }

    out[0] = FONT[Math.floor(hundredths / 1000)];
    out[1] = FONT[Math.floor(hundredths / 100) % 10] | SEGMENT_DP;
    out[2] = FONT[Math.floor(hundredths / 10) % 10];
    out[3] = FONT[hundredths % 10];
  }


  // What holding the bank button shows: the money in play mode, the
  // accuracy in train mode
  function fillHoldView(out) {

    if (trainingMode) {
      accuracyToDigits(out);
    } else {
      // Cents in, dollars out: the half dollar is kept, not shown
      numberToDigits(out, 0, 4, game.displayBankroll());
    }
  }


  // A long hold on the bank button moves on from the bankroll to the best
  // bank: HIGH for 200 ms, the score for 200 ms, round again, for as long
  // as the button is down. Train mode has no best bank, so there the hold
  // stays on the accuracy.
  function holdWantsHighWord() {

    if (trainingMode) return false;

    const held = performance.now() - bankHeldSince;

    if (held < 1000) return false;

    return (Math.floor((held - 1000) / 200) % 2) === 0;
  }


  function holdPatterns() {

    if (holdWantsHighWord()) {
      return HIGH_WORD;
    }

    fillHoldView(holdDigits);

    return holdDigits;
  }


  function paint() {

    const patterns = bankDown ? holdPatterns() : displayBuffer;

    digits.forEach(function (svg, index) {
      writePattern(svg, patterns[index] || SEGMENT_BLANK);
    });

    Object.keys(leds).forEach(function (name) {
      els.leds[name].classList.toggle('on', !!leds[name]);
    });
  }


  // --------------------------------------------------
  // PACE
  // The sketch keeps refreshing the display while it waits, so every wait
  // here paints too: the bank button works in any part of the game.
  // --------------------------------------------------

  function sleep(ms) {

    return new Promise(function (resolve) {
      setTimeout(resolve, Math.max(1, ms));
    });
  }


  async function pause(ms) {

    const end = performance.now() + ms;

    paint();

    while (performance.now() < end) {
      await sleep(Math.min(40, Math.max(4, end - performance.now())));
      paint();
    }
  }


  // --------------------------------------------------
  // WHAT THE ROUND SHOWS
  // --------------------------------------------------

  // Player's total on the first two digits, the dealer's value on the
  // last two
  function showTable(hand, dealerValue) {

    numberToDigits(displayBuffer, 0, 2, game.handTotal(hand));
    numberToDigits(displayBuffer, 2, 2, dealerValue);
  }


  // Blinks the bankroll, or the accuracy in train mode, for half a second
  async function flashSummary() {

    for (let i = 0; i < 2; i++) {

      fillHoldView(displayBuffer);
      await pause(125);

      clearDisplay();
      await pause(125);
    }
  }


  // On the way into play mode: the label, then the best bank so far
  async function showHighScore() {

    clearDisplay();

    for (let i = 0; i < 4; i++) {
      displayBuffer[i] = HIGH_WORD[i];
    }

    await pause(200);

    numberToDigits(displayBuffer, 0, 4, game.displayHighscore());

    await pause(200);
  }


  // The word for the action the book wanted, left justified, held for the
  // half second the sketch pauses for
  async function showActionWord(action) {

    clearDisplay();

    const word = ACTION_WORDS[action];

    word.forEach(function (pattern, index) {
      displayBuffer[index] = pattern;
    });

    await pause(500);
  }


  // Two quick blinks: this action was not allowed, nothing changes
  async function rejectAction() {

    const saved = displayBuffer.slice();

    for (let i = 0; i < 2; i++) {

      clearDisplay();
      await pause(80);

      for (let d = 0; d < 4; d++) {
        displayBuffer[d] = saved[d];
      }

      await pause(80);
    }
  }


  function setOutcomeLed(outcome) {

    leds.win = (outcome === 'win');
    leds.push = (outcome === 'push');
    leds.lose = (outcome === 'lose');
  }


  // --------------------------------------------------
  // INPUT
  // --------------------------------------------------

  let actionWaiter = null;
  let pressWaiter = null;

  // Latched from the mode switch at the start of each hand
  let trainingMode = false;
  let modeKnown = false;
  let switchPosition = 'train';


  function takePress(action) {

    if (pressWaiter) {

      const resolve = pressWaiter;
      pressWaiter = null;
      resolve(action);

      return;
    }

    if (actionWaiter && action !== 'B') {

      const resolve = actionWaiter;
      actionWaiter = null;
      resolve(action);
    }
  }


  // Waits for one of the four action buttons. The bank button is not an
  // action: holding it only changes what the display shows.
  function waitForAction() {

    // Nothing else paints while the board sits waiting for a press
    paint();

    return new Promise(function (resolve) {
      actionWaiter = resolve;
    });
  }


  // The press and its release both have to finish first
  function waitForRestart() {

    paint();

    return new Promise(function (resolve) {
      pressWaiter = resolve;
    });
  }


  // The sketch refreshes the display in its idle loop, which is what makes
  // the long hold blink while the board waits for a press. Idle here paints
  // nothing, so the hold runs its own ticker.
  let bankTicker = null;


  function showBank() {

    if (bankDown) return;

    bankDown = true;
    bankHeldSince = performance.now();

    clearInterval(bankTicker);
    bankTicker = setInterval(paint, 50);

    paint();
  }


  function hideBank() {

    if (!bankDown) return;

    bankDown = false;

    clearInterval(bankTicker);
    bankTicker = null;

    paint();
  }


  document.querySelectorAll('[data-action]').forEach(function (button) {

    button.addEventListener('click', function () {

      button.blur();
      takePress(button.dataset.action);
    });
  });


  const bankButton = document.querySelector('[data-action="B"]');

  bankButton.addEventListener('pointerdown', function (event) {
    event.preventDefault();
    showBank();
  });

  window.addEventListener('pointerup', hideBank);
  window.addEventListener('pointercancel', hideBank);
  window.addEventListener('blur', hideBank);


  function setSwitch(position) {

    switchPosition = position;

    els.modeSwitch.setAttribute('aria-checked', position === 'train' ? 'true' : 'false');
  }


  els.modeSwitch.addEventListener('click', function () {

    els.modeSwitch.blur();
    setSwitch(switchPosition === 'train' ? 'play' : 'train');
  });


  window.addEventListener('keydown', function (event) {

    if (event.metaKey || event.ctrlKey || event.altKey) return;

    const key = event.key.length === 1 ? event.key.toUpperCase() : event.key;

    if (key === 'B') {

      event.preventDefault();

      // Key repeat is the button being held, not pressed again
      if (!event.repeat) showBank();

      return;
    }

    if (key === 'T') {

      event.preventDefault();
      setSwitch(switchPosition === 'train' ? 'play' : 'train');

      return;
    }

    if (key === 'H' || key === 'S' || key === 'D' || key === 'P') {

      event.preventDefault();

      if (!event.repeat) takePress(key);
    }
  });


  window.addEventListener('keyup', function (event) {

    if (event.key.length === 1 && event.key.toUpperCase() === 'B') {
      hideBank();
    }
  });


  // --------------------------------------------------
  // ONE ROUND
  // --------------------------------------------------

  async function playRound() {

    if (game.shoeIsLow()) {
      game.initialiseShoe();
    }

    // Switching back from train mode with an empty bank
    if (!trainingMode && game.bankroll() <= 0) {
      await waitForBankRestart();
    }

    // New round, no result showing yet
    setOutcomeLed(null);
    leds.correct = false;
    leds.wrong = false;

    const hands = game.hands;
    let handCount = 1;

    game.startRound();

    const dealer = game.dealer;
    const up = game.dealerUp();

    let current = 0;

    while (current < handCount) {

      const hand = hands[current];

      // A freshly split hand holds one card and needs a second
      if (hand.count === 1) {

        game.addCard(hand, game.drawCard());

        showTable(hand, game.cardValue(up));
        leds.soft = game.handIsSoft(hand);
        leds.pair = hand.pairValue !== 0;

        await pause(500);

        // Split aces are played with that single card
        if (hand.aceSplit) {
          current++;
          continue;
        }
      }

      // A natural blackjack has nothing to decide
      if (game.handIsNatural(hand)) {

        showTable(hand, game.cardValue(up));
        leds.soft = game.handIsSoft(hand);
        leds.pair = false;

        await pause(700);

        current++;
        continue;
      }

      let restartHand = false;

      while (true) {

        showTable(hand, game.cardValue(up));
        leds.soft = game.handIsSoft(hand);
        leds.pair = hand.pairValue !== 0;

        const chosen = await waitForAction();

        // Reject actions this hand cannot make, without grading them.
        // Train mode has no bankroll, so there only the cards can say no;
        // in play mode the money has to be there as well.
        if (chosen === 'D' &&
            (!game.canDouble(hand) ||
             (!trainingMode && game.moneyLeft(handCount) < game.betSize()))) {

          await rejectAction();
          continue;
        }

        if (chosen === 'P' &&
            (!game.canSplit(hand, handCount) ||
             (!trainingMode && game.moneyLeft(handCount) < game.betSize()))) {

          await rejectAction();
          continue;
        }

        // Play mode takes the decision at face value and says nothing
        // about it. Train mode grades every decision: the LEDs stay lit
        // until the next decision replaces them, and a wrong one also
        // shows the action the book wanted before the hand carries on.
        if (trainingMode) {

          const correct = game.bestAction(hand, up, handCount);
          const correctChoice = game.grade(chosen, correct);

          leds.correct = correctChoice;
          leds.wrong = !correctChoice;

          if (!correctChoice) {
            await showActionWord(correct);
          }
        }

        if (chosen === 'S') {
          break;
        }

        if (chosen === 'P') {

          handCount = game.splitHand(current, handCount, game.betSize());
          restartHand = true;
          break;
        }

        // Doubling puts another bet on the hand
        if (chosen === 'D') {
          hand.wager += game.betSize();
        }

        // Hit or double both draw one card
        game.addCard(hand, game.drawCard());

        showTable(hand, game.cardValue(up));
        leds.soft = game.handIsSoft(hand);
        leds.pair = hand.pairValue !== 0;

        await pause(350);

        if (game.handIsBust(hand)) {
          await pause(400);
          break;
        }

        if (game.handTotal(hand) === 21) {
          await pause(400);
          break;
        }

        if (chosen === 'D') {
          break;
        }
      }

      if (!restartHand) {
        current++;
      }
    }

    // ------------------------------------------------
    // DEALER'S TURN
    // ------------------------------------------------

    // Standing ends the hand the instant the decision is made, so a train
    // session holds the grade up long enough to be seen before the dealer
    // starts
    if (trainingMode) {
      await pause(500);
    }

    leds.correct = false;
    leds.wrong = false;
    leds.soft = false;
    leds.pair = false;

    // Nothing to beat if every hand has already busted. The first two
    // digits keep following the last hand that is still alive.
    let dealerPlays = false;
    let displayHand = 0;

    for (let i = 0; i < handCount; i++) {

      if (!game.handIsBust(hands[i])) {

        dealerPlays = true;
        displayHand = i;
      }
    }

    if (dealerPlays) {

      // Turn the hole card over and let the player see it
      game.addCard(dealer, game.dealerHole());

      showTable(hands[displayHand], game.handTotal(dealer));

      await pause(600);

      // Dealer stands on all 17s, including soft 17. One card every half
      // second so the total climbs where it can be watched.
      while (game.handTotal(dealer) < 17) {

        game.addCard(dealer, game.drawCard());

        showTable(hands[displayHand], game.handTotal(dealer));

        await pause(500);
      }

    } else {

      game.addCard(dealer, game.dealerHole());
    }

    // ------------------------------------------------
    // RESULT
    // ------------------------------------------------

    const dealerTotal = game.handTotal(dealer);

    for (let i = 0; i < handCount; i++) {

      const outcome = game.outcomeFor(hands[i], dealer);

      showTable(hands[i], dealerTotal);
      setOutcomeLed(outcome);

      await pause(900);
    }

    game.finishRound();

    // Easter egg: the bank cannot be beaten, and it never comes back
    if (game.jackpot()) {
      await easterEgg();
      return;
    }

    // Show what is left in the bank, or the accuracy, blinking
    await flashSummary();

    if (!trainingMode && game.bankroll() === 0) {
      await waitForBankRestart();
    }
  }


  // --------------------------------------------------
  // OUT OF MONEY AND THE JACKPOT
  // --------------------------------------------------

  // Out of money: the lose LED stays on and nothing happens until a
  // button is pressed, which starts a fresh bank
  async function waitForBankRestart() {

    clearDisplay();
    numberToDigits(displayBuffer, 0, 4, 0);

    setOutcomeLed('lose');
    leds.soft = false;
    leds.pair = false;
    leds.correct = false;
    leds.wrong = false;

    paint();

    await waitForRestart();

    game.restartBank();

    setOutcomeLed(null);

    paint();
  }


  // A bankroll of 9999 is the most four digits can show, so getting there
  // sets every LED going on its own coin toss at its own interval. The
  // game never picks up again: only a reload ends it.
  async function easterEgg() {

    // The light show never ends, so the run is banked as finished here: a
    // reload then comes back playable at 100 instead of dropping straight
    // into the egg all over again
    game.finishEgg();

    // Nothing above 9999 could be displayed anyway
    clearDisplay();
    numberToDigits(displayBuffer, 0, 4, 9999);

    while (true) {

      Object.keys(leds).forEach(function (name) {
        leds[name] = Math.random() < 0.5;
      });

      await pause(20 + Math.floor(Math.random() * 100));
    }
  }


  // --------------------------------------------------
  // MAIN LOOP
  // --------------------------------------------------

  async function loop() {

    while (true) {

      // The switch is read between hands, not in the middle of one
      const wantTraining = (switchPosition === 'train');
      let enteredPlay = false;

      if (!modeKnown || wantTraining !== trainingMode) {

        modeKnown = true;
        enteredPlay = !wantTraining;
        trainingMode = wantTraining;

        game.setMode(trainingMode ? 'train' : 'play');

        setOutcomeLed(null);
        leds.correct = false;
        leds.wrong = false;

        if (trainingMode) {

          // Every train session starts from nothing
          game.resetAccuracy();
        }
      }

      // HIGH, then the best bank, before the first hand
      if (enteredPlay) {
        await showHighScore();
      }

      await playRound();
    }
  }


  // The sketch deals as soon as it boots
  setSwitch('train');
  paint();

  loop().catch(function (error) {
    // Nothing here should throw; if it does, do not leave a dead board
    console.error(error);
  });

})();
