import test from 'node:test';
import assert from 'node:assert/strict';
import {crc16, framePacket} from '../src/shared/crc16.mjs';
test('CRC-16/CCITT-FALSE check value and a real PAI1 packet', () => {
  assert.equal(crc16('123456789'), 0x29b1);
  // Expected from Python: binascii.crc_hqx(b'PAI1,MODE,1', 0xffff)
  assert.equal(crc16('PAI1,MODE,1'), 0x1f24);
  assert.equal(crc16(''), 0xffff);
});
test('command packets carry four lower-case hex digits after the star', () => {
  assert.equal(framePacket('PAI1,MODE,1'), 'PAI1,MODE,1*1f24');
  // binascii.crc_hqx(b'PST1,INFO,1', 0xffff) == 0xdeac
  assert.equal(framePacket('PST1,INFO,1'), 'PST1,INFO,1*deac');
});
