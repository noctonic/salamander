class DecoderWorklet extends AudioWorkletProcessor {
  constructor() {
    super();
    this._buf = null;
  }

  process(inputs) {
    const chs = inputs[0];
    if (!chs || chs.length === 0 || !chs[0]) return true;

    const ch0 = chs[0];
    const N = ch0.length;

    if (!this._buf || this._buf.length !== N) {
      this._buf = new Int16Array(N);
    }

    for (let i = 0; i < N; i++) {
      const s = ch0[i];
      this._buf[i] = s <= -1 ? -32768 : s >= 1 ? 32767 : (s * 32767) | 0;
    }

    this.port.postMessage(this._buf.slice());
    return true;
  }
}

registerProcessor('decoder-worklet', DecoderWorklet);
