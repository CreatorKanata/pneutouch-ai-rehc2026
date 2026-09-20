// One Web Serial reader; cancellation releases the lock before closing the port.
import {LineBuffer} from './protocol.mjs';

export class SerialConnection {
  constructor(settings, callbacks) { this.settings = settings; Object.assign(this, callbacks); }
  async start(port) {
    if (this.port) throw new Error('すでに接続中です');
    await port.open({baudRate: this.settings.baudRate, dataBits: 8,
      stopBits: 1, parity: 'none', flowControl: 'none'});
    this.port = port;
    this.running = this.read(port);
  }
  async read(port) {
    const buffer = new LineBuffer(this.settings.maxLine), decoder = new TextDecoder();
    try {
      if (!port.readable) throw new Error('読み取りストリームがありません');
      this.reader = port.readable.getReader();
      for (;;) {
        const {value, done} = await this.reader.read();
        if (done) break;
        const before = buffer.dropped;
        for (const line of buffer.feed(decoder.decode(value, {stream: true}))) this.onLine(line);
        if (buffer.dropped > before) this.onError('長すぎる受信行を除外しました');
      }
    } catch (error) { this.onError(`受信停止: ${error.message}`); }
    finally {
      this.reader?.releaseLock(); this.reader = undefined;
      try { await port.close(); } catch (error) { this.onError(`ポート終了: ${error.message}`); }
      this.port = undefined; this.onClose();
    }
  }
  async stop() { if (this.reader) await this.reader.cancel(); await this.running; }
}
