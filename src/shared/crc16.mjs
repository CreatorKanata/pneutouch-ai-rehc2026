// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF): the PAI1/PST1 command trailer, as firmware crc16.c.
const encoder = new TextEncoder();
export function crc16(text) {
  let crc = 0xffff;
  for (const byte of encoder.encode(text)) {
    crc ^= byte << 8;
    for (let bit = 0; bit < 8; bit++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
  }
  return crc;
}
// One command line body without the line ending: `<body>*<crc hex>`.
export const framePacket = body => `${body}*${crc16(body).toString(16).padStart(4, '0')}`;
