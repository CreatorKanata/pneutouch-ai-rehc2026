// Web Serial subset for the Android app: exactly what src/shared/serial.mjs uses, backed by the
// native USB serial bridge. JS -> native: window.PneuTouchSerial (requestPort/open/write/close with a
// call id). Native -> JS: window.__pneuTouchSerial.settle(id, error|null), data(base64), lost(message).
(function install(window) {
  'use strict';
  const navigator = window.navigator, bridge = window.PneuTouchSerial;
  if (!navigator || 'serial' in navigator || !bridge || window.__pneuTouchSerial) return;
  const pending = new Map();
  let nextId = 1, active = null;

  // One native call; resolves or rejects (with the native Japanese message) when native settles it.
  function call(method, ...args) {
    const id = nextId++;
    return new Promise((resolve, reject) => {
      pending.set(id, {resolve, reject});
      try { bridge[method](id, ...args); } catch (error) {
        pending.delete(id); reject(new Error(String(error && error.message || error)));
      }
    });
  }
  function toBytes(base64) {
    const text = window.atob(base64), bytes = new Uint8Array(text.length);
    for (let i = 0; i < text.length; i += 1) bytes[i] = text.charCodeAt(i);
    return bytes;
  }
  function toBase64(bytes) {
    let text = '';
    for (let i = 0; i < bytes.length; i += 0x2000)
      text += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x2000));
    return window.btoa(text);
  }

  class AndroidSerialPort {
    constructor() { this.readable = null; this.writable = null; this.controller = null; }
    async open(options) {
      if (!options || !Number.isInteger(options.baudRate) || options.baudRate <= 0)
        throw new TypeError('baudRate が必要です');
      if (this.readable) throw new Error('すでに開いています');
      const settings = {baudRate: options.baudRate, dataBits: options.dataBits ?? 8, stopBits: options.stopBits ?? 1,
        parity: options.parity ?? 'none', flowControl: options.flowControl ?? 'none'};
      await call('open', JSON.stringify(settings));
      this.readable = new window.ReadableStream({start: controller => { this.controller = controller; },
        cancel: () => { this.controller = null; }});
      this.writable = new window.WritableStream({write: chunk => call('write', toBase64(new Uint8Array(chunk)))});
      active = this;
    }
    receive(bytes) { try { this.controller?.enqueue(bytes); } catch { this.controller = null; } }
    fail(message) {
      try { this.controller?.error(new Error(message)); } catch { /* already closed */ }
      this.controller = null;
    }
    async close() {
      try { this.controller?.close(); } catch { /* already errored or cancelled */ }
      this.controller = null; this.readable = null; this.writable = null;
      if (active === this) active = null;
      await call('close');
    }
  }

  Object.defineProperty(window, '__pneuTouchSerial', {value: Object.freeze({
    settle(id, error) {
      const entry = pending.get(id);
      if (!entry) return;
      pending.delete(id);
      if (error === null || error === undefined) entry.resolve(); else entry.reject(new Error(String(error)));
    },
    data(base64) { active?.receive(toBytes(base64)); },
    lost(message) { active?.fail(String(message)); },
  })});
  const serial = Object.freeze({
    androidUsb: true,
    async requestPort() { await call('requestPort'); return new AndroidSerialPort(); },
  });
  Object.defineProperty(navigator, 'serial', {value: serial, configurable: true, enumerable: true});
})(typeof window === 'undefined' ? globalThis : window);
