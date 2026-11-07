/*
  BLACKJACK STRATEGY TRAINER - screen, LEDs and buttons

  Nothing here but what the board itself shows: four digits, seven
  LEDs, five buttons and the play/train switch. Rounds advance on
  their own, the way the sketch loops after delay(1000).
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

  const DIGIT_SEGMENTS = {
    '0': 'abcdef',
    '1': 'bc',
    '2': 'abdeg',
    '3': 'abgcd',
    '4': 'fgbc',
    '5': 'afgcd',
    '6': 'afgecd',
    '7': 'abc',
    '8': 'abcdefg',
    '9': 'abcdfg'
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
      line.dataset.seg = name;

      svg.appendChild(line);
    });

    return svg;
  }


  const playerDigits = [makeDigit(), makeDigit()];
  const dealerDigits = [makeDigit(), makeDigit()];

  playerDigits.forEach(function (d) { els.playerDigits.appendChild(d); });
  dealerDigits.forEach(function (d) { els.dealerDigits.appendChild(d); });


  function setDigit(svg, character) {

    const lit = DIGIT_SEGMENTS[character] || '';

    svg.querySelectorAll('line').forEach(function (line) {
      line.classList.toggle('on', lit.indexOf(line.dataset.seg) !== -1);
    });
  }


  // Two digits per side, leading zero blank
  function setPair(digits, value) {

    if (value === null || value === undefined) {
      setDigit(digits[0], ' ');
      setDigit(digits[1], ' ');
      return;
    }

    const text = String(value);

    if (text.length >= 2) {
      setDigit(digits[0], text[text.length - 2]);
      setDigit(digits[1], text[text.length - 1]);
    } else {
      setDigit(digits[0], ' ');
      setDigit(digits[1], text);
    }
  }


  // The bank readout across all four digits: percent in train, money in play
  function setBankDisplay(value) {

    const text = (value === null || value === undefined) ? '' : String(Math.max(0, value));
    const slots = [playerDigits[0], playerDigits[1], dealerDigits[0], dealerDigits[1]];
    const pad = slots.length - text.length;

    slots.forEach(function (svg, i) {

      const index = i - pad;

      setDigit(svg, index >= 0 ? text.charAt(index) : ' ');
    });
  }


  function setLed(name, on) {
    els.leds[name].classList.toggle('on', !!on);
  }


  // --------------------------------------------------
  // GAME
  // --------------------------------------------------

  let game = null;
  let dealerTimer = null;
  let nextRoundTimer = null;

  // The bank readout is momentary, with a floor so a quick tap is visible
  const BANK_MIN_SHOW = 700;

  let bankHeld = false;
  let bankShownAt = 0;
  let bankHideTimer = null;


  function clearTimers() {

    clearTimeout(dealerTimer);
    clearTimeout(nextRoundTimer);

    dealerTimer = null;
    nextRoundTimer = null;
  }


  function render() {

    if (!game) return;

    const state = game.getState();
    const index = Math.max(0, Math.min(state.current, state.hands.length - 1));
    const hand = state.hands[index] || null;

    if (bankHeld) {

      // The bank button borrows the whole display while it is held:
      // accuracy in the train position, the bankroll in the play position
      setBankDisplay(state.mode === 'train'
        ? (state.accuracy.total ? state.accuracy.percent : null)
        : state.bankroll);

    } else {

      // Player on the left, dealer on the right, as on the board
      setPair(playerDigits, hand ? hand.total : null);
      setPair(dealerDigits, state.dealer && state.dealer.cards.length ? state.dealer.total : null);
    }

    // Pair and soft describe the hand being played
    setLed('pair', !!(hand && hand.pair));
    setLed('soft', !!(hand && hand.soft));

    // Action buttons follow the same restrictions as the firmware
    ['H', 'S', 'D', 'P'].forEach(function (action) {

      const button = document.querySelector('[data-action="' + action + '"]');

      if (button) button.disabled = !game.canAct(action);
    });

    const train = state.mode === 'train';

    els.modeSwitch.setAttribute('aria-checked', train ? 'true' : 'false');
  }


  function handleEvent(event) {

    switch (event.type) {

      case 'roundStart':
        setLed('win', false);
        setLed('push', false);
        setLed('lose', false);
        setLed('correct', false);
        setLed('wrong', false);
        break;

      case 'grade':
        setLed('correct', event.correct);
        setLed('wrong', !event.correct);
        break;

      case 'mode':
        // Talley marks belong to the train position only
        setLed('correct', false);
        setLed('wrong', false);
        break;

      case 'dealerReady':
        // Short pause so the player's last card is readable before the
        // hole card turns over
        clearTimeout(dealerTimer);
        dealerTimer = setTimeout(function () {
          dealerTimer = null;
          game.playDealer();
        }, 900);
        break;

      case 'settle':
        setLed('win', event.results.some(function (r) { return r.outcome === 'win'; }));
        setLed('push', event.results.some(function (r) { return r.outcome === 'push'; }));
        setLed('lose', event.results.some(function (r) { return r.outcome === 'lose'; }));

        // Same as the sketch: one second, then deal again
        if (game.getState().phase === 'results') {
          clearTimeout(nextRoundTimer);
          nextRoundTimer = setTimeout(function () {
            nextRoundTimer = null;
            game.startRound();
          }, 1000);
        }
        break;
    }

    render();
  }


  function toggleMode() {
    game.setMode(game.getState().mode === 'train' ? 'play' : 'train');
  }


  function showBank() {

    clearTimeout(bankHideTimer);
    bankHideTimer = null;

    bankShownAt = Date.now();
    bankHeld = true;

    render();
  }


  function hideBank() {

    if (!bankHeld) return;

    const remaining = BANK_MIN_SHOW - (Date.now() - bankShownAt);

    clearTimeout(bankHideTimer);

    if (remaining > 0) {
      bankHideTimer = setTimeout(hideBank, remaining);
      return;
    }

    bankHeld = false;

    render();
  }


  game = window.BlackjackEngine.createGame({ onEvent: handleEvent });

  render();

  // The sketch deals as soon as it boots
  game.startRound();


  // --------------------------------------------------
  // INPUT
  // --------------------------------------------------

  document.querySelectorAll('[data-action]').forEach(function (button) {

    button.addEventListener('click', function () {

      const action = button.dataset.action;

      button.blur();

      // The bank button is wired but has no game action, as in the sketch
      if (action === 'B') return;

      game.act(action);
    });
  });


  const bankButton = document.querySelector('[data-action="B"]');

  bankButton.addEventListener('pointerdown', function (event) {
    event.preventDefault();
    showBank();
  });

  bankButton.addEventListener('pointerleave', hideBank);

  window.addEventListener('pointerup', hideBank);
  window.addEventListener('pointercancel', hideBank);
  window.addEventListener('blur', hideBank);


  els.modeSwitch.addEventListener('click', function () {
    els.modeSwitch.blur();
    toggleMode();
  });


  window.addEventListener('keydown', function (event) {

    if (event.repeat || event.metaKey || event.ctrlKey || event.altKey) return;

    const key = event.key.length === 1 ? event.key.toUpperCase() : event.key;

    if (key === 'H' || key === 'S' || key === 'D' || key === 'P') {
      event.preventDefault();
      game.act(key);
      return;
    }

    if (key === 'B') {
      event.preventDefault();
      showBank();
      return;
    }

    if (key === 'T') {
      event.preventDefault();
      toggleMode();
      return;
    }

    if (key === 'R') {
      event.preventDefault();
      clearTimers();
      game.restart();
    }
  });


  window.addEventListener('keyup', function (event) {

    if (event.key.length === 1 && event.key.toUpperCase() === 'B') {
      hideBank();
    }
  });

})();
