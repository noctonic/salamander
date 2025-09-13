import createModule from './rgcli.mjs';

const Module = await createModule();
const encode = Module.cwrap('wasm_encode', 'number',
  ['string', 'string', 'number', 'number', 'number', 'number', 'number', 'number', 'number']);
const decode = Module.cwrap('wasm_decode', 'number',
  ['number', 'number', 'number', 'number', 'number', 'number', 'number', 'number', 'number']);

let audioCtx;
let micTrack;
let sourceNode;
let workletNode;
let syncing = false;

let myCall = localStorage.getItem('callsign') || 'WEB';
let carrierFrequency = parseInt(localStorage.getItem('carrierFrequency')) || 1500;
let noiseSymbols = parseInt(localStorage.getItem('noiseSymbols')) || 0;
let fancyHeader = localStorage.getItem('fancyHeader') === '1';
let sampleRate = parseInt(localStorage.getItem('sampleRate')) || 48000;
let channel = parseInt(localStorage.getItem('channel')) || 0;

const messages = document.getElementById('messages');
const inputEl  = document.getElementById('input');
document.getElementById('send').onclick = send;
const settingsBtn = document.getElementById('settingsBtn');
const settingsPanel = document.getElementById('settingsPanel');

const callInput = document.getElementById('callsign');
callInput.value = myCall;
callInput.onchange = () => {
  myCall = callInput.value.trim().toUpperCase() || 'WEB';
  localStorage.setItem('callsign', myCall);
};

const carrierInput = document.getElementById('carrierFrequency');
carrierInput.value = carrierFrequency;
carrierInput.onchange = () => {
  carrierFrequency = parseInt(carrierInput.value) || 1500;
  localStorage.setItem('carrierFrequency', carrierFrequency);
};

const noiseInput = document.getElementById('noiseSymbols');
noiseInput.value = noiseSymbols;
noiseInput.onchange = () => {
  noiseSymbols = parseInt(noiseInput.value) || 0;
  localStorage.setItem('noiseSymbols', noiseSymbols);
};

const fancyInput = document.getElementById('fancyHeader');
fancyInput.checked = fancyHeader;
fancyInput.onchange = () => {
  fancyHeader = fancyInput.checked;
  localStorage.setItem('fancyHeader', fancyHeader ? '1' : '0');
};

const sampleRateSelect = document.getElementById('sampleRate');
sampleRateSelect.value = String(sampleRate);
sampleRateSelect.onchange = async () => {
  sampleRate = parseInt(sampleRateSelect.value);
  localStorage.setItem('sampleRate', sampleRate);
  await initMic();
};

const channelSelect = document.getElementById('channel');
channelSelect.value = String(channel);
channelSelect.onchange = () => {
  channel = parseInt(channelSelect.value);
  localStorage.setItem('channel', channel);
};

settingsBtn.onclick = () => settingsPanel.classList.toggle('hidden');

function pad2(n){ return String(n).padStart(2,'0'); }
function fmtTime(d = new Date()){
  return `[${pad2(d.getHours())}:${pad2(d.getMinutes())}]`;
}

function createRow(role, nick, plainText, timeStr = fmtTime()){
  const row = document.createElement('div');
  row.className = `row ${role}`;
  row.dataset.ts = timeStr;
  row.dataset.nick = nick;

  const ts = document.createElement('span');
  ts.className = 'ts';
  ts.textContent = timeStr;

  const nk = document.createElement('span');
  nk.className = 'nick';
  nk.textContent = `<${nick || (role==='system' ? '***' : '???')}>`;

  const tx = document.createElement('span');
  tx.className = 'text';
  tx.textContent = plainText;

  row.appendChild(ts);
  row.appendChild(nk);
  row.appendChild(tx);
  return row;
}

function shouldStick(){
  const threshold = 36;
  return messages.scrollHeight - messages.scrollTop - messages.clientHeight < threshold;
}
function scrollToBottom(){
  messages.scrollTop = messages.scrollHeight;
}

function addMessage(role, nick, text){
  const stick = shouldStick();
  const row = createRow(role, nick, text);
  messages.appendChild(row);
  if (stick) scrollToBottom();
  if (document.hidden) {
    pendingUnread++;
    updateTitleBadge();
  }
}

let baselineTitle = document.title || 'Salamander Chat';
let pendingUnread = 0;
function updateTitleBadge(){
  document.title = pendingUnread > 0 ? `(${pendingUnread}) ${baselineTitle}` : baselineTitle;
}
window.addEventListener('focus', () => { pendingUnread = 0; updateTitleBadge(); });
document.addEventListener('visibilitychange', () => {
  if (!document.hidden) { pendingUnread = 0; updateTitleBadge(); }
});

inputEl.addEventListener('keydown', (e) => {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault();
    send();
  }
});

async function deriveKeyFromPSK(psk) {
  const enc = new TextEncoder();
  const hash = await crypto.subtle.digest('SHA-256', enc.encode(psk));
  return crypto.subtle.importKey('raw', hash, { name: 'AES-GCM' }, false, ['encrypt', 'decrypt']);
}
async function aesEncryptToWireBase64(plaintextBytes, psk) {
  const key = await deriveKeyFromPSK(psk);
  const iv  = crypto.getRandomValues(new Uint8Array(12));
  const ct  = new Uint8Array(await crypto.subtle.encrypt({ name: 'AES-GCM', iv }, key, plaintextBytes));
  const wireBytes = new Uint8Array(iv.length + ct.length);
  wireBytes.set(iv, 0);
  wireBytes.set(ct, iv.length);
  let bin = '';
  for (let i = 0; i < wireBytes.length; i++) bin += String.fromCharCode(wireBytes[i]);
  return btoa(bin);
}
async function aesDecryptFromWireBase64(b64, psk) {
  const bin = atob(b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  if (bytes.length < 12 + 16) throw new Error('cipher too short');
  const iv = bytes.subarray(0, 12);
  const ct = bytes.subarray(12);
  const key = await deriveKeyFromPSK(psk);
  const pt  = await crypto.subtle.decrypt({ name: 'AES-GCM', iv }, key, ct);
  return new Uint8Array(pt);
}

function calcOnAirBytes(plainLen, aesOn) {
  if (!aesOn) return plainLen;
  const raw = 12 + plainLen + 16;
  return 4 * Math.ceil(raw / 3);
}

async function initMic() {
  const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
  if (micTrack) micTrack.stop();
  micTrack = stream.getAudioTracks()[0];

  if (audioCtx) await audioCtx.close();
  audioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate });
  await audioCtx.audioWorklet.addModule('decoder-worklet.js');

  sourceNode = audioCtx.createMediaStreamSource(stream);
  workletNode = new AudioWorkletNode(audioCtx, 'decoder-worklet');
  workletNode.port.onmessage = ({ data }) => {
    const samples = data;
    const ptr = Module._malloc(samples.length * 2);
    Module.HEAP16.set(samples, ptr / 2);
    const outPtr = Module._malloc(256);
    const callPtr = Module._malloc(16);
    const len = decode(ptr, samples.length, audioCtx.sampleRate, 1, 0, outPtr, 256, callPtr, 16);

    if (len > 0) {
      const msg = Module.UTF8ToString(outPtr);
      const call = (Module.UTF8ToString(callPtr) || '???').toUpperCase();

      (async () => {
        try {
          const aesOnRx = !!document.getElementById('useAesGcm')?.checked;
          let text;
          if (aesOnRx) {
            const psk = (document.getElementById('psk')?.value || '').trim();
            if (!psk) throw new Error('PSK missing');
            const ptBytes = await aesDecryptFromWireBase64(msg, psk);
            text = new TextDecoder().decode(ptBytes);
          } else {
            text = msg;
          }
          addMessage('them', call, text);
        } catch (e) {
          addMessage('system', '***', `decrypt failed: ${e.message || 'unknown error'}`);
          addMessage('them', call, '[unreadable]');
        } finally {
          syncing = false;
        }
      })();

    } else if (len === -2) {
      if (!syncing) addMessage('system', '***', 'signal detected');
      syncing = true;
    } else if (len === -5) {
      addMessage('system', '***', 'preamble nope');
      syncing = false;
    } else if (len === -6) {
      addMessage('system', '***', 'preamble ping');
      syncing = false;
    } else if (len === -1) {
      addMessage('system', '***', 'decode failed');
      syncing = false;
    }

    Module._free(ptr);
    Module._free(outPtr);
    Module._free(callPtr);
  };

  const gain = audioCtx.createGain();
  gain.gain.value = 0;
  sourceNode.connect(workletNode).connect(gain).connect(audioCtx.destination);

  addMessage('system', '***', `mic initialized @ ${audioCtx.sampleRate} Hz`);
}

async function send() {
  const text = inputEl.value.trim();
  if (!text) return;

  const aesOn = !!document.getElementById('useAesGcm')?.checked;
  const psk   = (document.getElementById('psk')?.value || '').trim();

  const plainLen = new TextEncoder().encode(text).length;
  const predicted = calcOnAirBytes(plainLen, aesOn);
  if (predicted > 170) {
    addMessage('system','***', `message too long ${aesOn ? 'after encryption ' : ''}(${predicted} > 170). Shorten it.`);
    return;
  }


  let wireStr;
  if (aesOn) {
    if (!psk) { addMessage('system','***','encryption enabled but PSK empty'); return; }
    try {
      const ptBytes = new TextEncoder().encode(text);
      wireStr = await aesEncryptToWireBase64(ptBytes, psk);
    } catch (e) {
      addMessage('system','***', `encryption error: ${e.message || e}`);
      return;
    }
  } else {
    wireStr = text;
  }

  addMessage('me', myCall, aesOn ? `🔒 ${text}` : text);
  inputEl.value = '';
  updateByteIndicator();

  if (audioCtx && audioCtx.state === 'suspended') {
    try { await audioCtx.resume(); } catch {}
  } else if (!audioCtx) {
    await initMic();
  }

  const rate = audioCtx.sampleRate;
  const maxSamples = rate * 10;
  const ptr = Module._malloc(maxSamples * 2);
  const written = encode(wireStr, myCall, carrierFrequency, noiseSymbols, fancyHeader ? 1 : 0, rate, channel, ptr, maxSamples);

  const samples = new Int16Array(Module.HEAP16.buffer, ptr, written);
  const buffer = audioCtx.createBuffer(1, written, rate);
  const float = new Float32Array(written);
  for (let i = 0; i < written; i++) float[i] = samples[i] / 32768;
  buffer.getChannelData(0).set(float);
  Module._free(ptr);

  const src = audioCtx.createBufferSource();
  src.buffer = buffer;
  src.connect(audioCtx.destination);

  const mute = document.getElementById('muteDuringTx').checked;
  if (mute && micTrack) micTrack.enabled = false;

  src.start();
  src.onended = () => {
    if (mute && micTrack) micTrack.enabled = true;
  };
}

initMic().catch(err => {
  console.error(err);
  addMessage('system', '***', 'mic init failed — allow microphone access');
});

document.querySelectorAll('#messages .row').forEach(row => {
  const ts = row.querySelector('.ts');
  const nk = row.querySelector('.nick');
  if (ts && !ts.textContent) ts.textContent = row.dataset.ts || fmtTime();
  if (nk && !nk.textContent) nk.textContent = `<${row.dataset.nick || '???'}>`;
});

const input = document.getElementById('input');
const byteIndicator = document.getElementById('byteIndicator');
const maxBytes = 170;

const aesCheckbox = document.getElementById('useAesGcm');
const pskInput = document.getElementById('psk');

function updateByteIndicator() {
  const plainLen = new TextEncoder().encode(input.value).length;
  const aesOn = !!aesCheckbox?.checked;
  const wireBytes = calcOnAirBytes(plainLen, aesOn);

  byteIndicator.textContent = `${wireBytes} / ${maxBytes} bytes${aesOn ? ' (enc)' : ''}`;
  byteIndicator.classList.toggle('over', wireBytes > maxBytes);
}

input.addEventListener('input', updateByteIndicator);
aesCheckbox?.addEventListener('change', updateByteIndicator);

updateByteIndicator();

const dangerZone = document.getElementById('dangerZone');
const dangerZoneFields = document.getElementById('dangerZoneFields');

if (dangerZone && dangerZoneFields) {
  dangerZoneFields.classList.toggle('hidden', !dangerZone.checked);
  dangerZone.addEventListener('change', () => {
    dangerZoneFields.classList.toggle('hidden', !dangerZone.checked);
    updateByteIndicator();
  });
}
