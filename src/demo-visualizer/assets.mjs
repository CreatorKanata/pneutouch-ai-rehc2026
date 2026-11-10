// assets.json: per object a label, part names, status images (DINO only; null = no picture) and videos.
import {KNOWN_PARTS} from './protocol.mjs';
const isText = v => typeof v === 'string' && v.length > 0;
function requireKeys(where, table, keys, what) {
  for (const key of keys) if (!isText(table?.[key])) throw new Error(`${where} の${what} ${key} の設定がありません`);
}
function checkObject(tag, entry) {
  const parts = KNOWN_PARTS[tag];
  if (!entry) throw new Error(`${tag} の設定がありません`);
  if (!isText(entry.label)) throw new Error(`${tag} の表示名の設定がありません`);
  if (JSON.stringify(entry.parts) !== JSON.stringify(parts)) throw new Error(`${tag} の部位は ${parts.join('/')} です`);
  requireKeys(tag, entry.names, parts, '部位名');
  requireKeys(tag, entry.videos, ['static', ...parts], '動画');
  // Only DINO has status pictures; another object may set images to null (text-only status card).
  if (entry.images !== null || tag === 'DINO') requireKeys(tag, entry.images, ['idle', 'pressed', ...parts], '状態画像');
}
// Returns the same object when every object has its videos (and DINO its images); throws otherwise.
export function validateAssets(assets) {
  if (!assets?.objects) throw new Error('assets.json に objects がありません');
  for (const tag of Object.keys(KNOWN_PARTS)) checkObject(tag, assets.objects[tag]);
  return assets;
}
