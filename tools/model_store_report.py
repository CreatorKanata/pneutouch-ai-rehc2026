"""Object profiles and the text reports of tools/model_store.py (info, train, tune).

The object of a FeRAM slot lives in its name (S_PneuTouch/object_profile.c): a name
starting with a known tag belongs to that object, any other name (legacy slots) to DINO.
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import feature_select as fs  # noqa: E402

OBJECTS = {'DINO': ['TAIL', 'BACK', 'LEGS', 'HEAD'], 'KRAG': ['HAT', 'FACE']}
DEFAULT_OBJECT = 'DINO'
ZONE_JA = {'TAIL': 'しっぽ', 'BACK': '背中', 'LEGS': '足', 'HEAD': '頭', 'HAT': '帽子', 'FACE': '顔'}
TUNE_FOLDS, ADVICE_RECALL_PCT, RECALL_ABSENT = 4, 90, 255
TUNE_MIN_SIZE, SCREEN_PER_CLASS, SCREEN_PASSES, TUNE_MARGIN_PCT = 3, 10, 2, 2  # model_tune.h
MODEL_ERRORS = {1: 'STORE', 2: 'NO_SLOT', 3: 'RECORD', 4: 'SELECT', 5: 'AI'}


def object_of_name(name):
    """Tag the slot name starts with (case-sensitive), else DINO."""
    return next((tag for tag in OBJECTS if name.startswith(tag)), DEFAULT_OBJECT)


def zone_names(obj=DEFAULT_OBJECT):
    """{label: zone} of an object, labels 1..classes."""
    return {i + 1: name for i, name in enumerate(OBJECTS[obj])}


def describe_object(obj):
    return f'{obj} ({"/".join(OBJECTS[obj])})'


def mask_features(mask):
    """Pool indices of a feature mask (bit j = pool feature j), ascending."""
    return [j for j in range(fs.POOL_COUNT) if mask >> j & 1]


def describe_training(result, obj=None):
    names = ', '.join(fs.FEATURE_NAMES[j] for j in result['selected']) or 'none'
    status = 'READY' if result['ready'] else f'ERROR {result["error"]}'
    line = (f'{status}: {result["count"]} records, {result["k"]} features selected ({names}), '
            f'training {result["training_us"] / 1e6:.2f} s')
    return f'{line}, object {describe_object(obj)}' if obj else line


def _zone(label, obj=DEFAULT_OBJECT):
    name = zone_names(obj).get(label, f'LABEL{label}')
    return f'{ZONE_JA.get(name, name)}（{name}）'


def _names(mask):
    return ', '.join(fs.FEATURE_NAMES[j] for j in mask_features(mask)) or 'none'


def shortlist_size(candidates):
    """Shortlist size whose subsets of TUNE_MIN_SIZE.. members number `candidates` (0 if none)."""
    for size in range(TUNE_MIN_SIZE, fs.SELECT_MAX + 1):
        if sum(math.comb(size, k) for k in range(TUNE_MIN_SIZE, size + 1)) == candidates:
            return size
    return 0


def tune_margin(count):
    """Records a challenger must win by more than (model_tune.c margin())."""
    return max(1, (count * TUNE_MARGIN_PCT + 50) // 100)


def _tune_summary(result):
    count, champion = result['count'], result['champion_mask']
    finals = 1 + sum(f['mask'] != champion for f in result['finalists'])
    judged = result['screened'] * SCREEN_PASSES
    lines = [f'tune: {count} records, shortlist {shortlist_size(result["candidates"])} features, '
             f'{result["candidates"]} subsets screened ({SCREEN_PER_CLASS}/class × {SCREEN_PASSES} passes), '
             f'{finals} finalists ({result["elapsed_us"] / 1e6:.1f} s)',
             f'  champion (top-{len(mask_features(champion))}): {_names(champion)}  4-fold '
             f'{result["champion_correct"]}/{count} = {100 * result["champion_correct"] / count:.1f}%']
    for i, f in enumerate(result['finalists']):
        lines.append(f'  {"finalists:" if i == 0 else "":<10}  {len(mask_features(f["mask"]))} features '
                     f'{_names(f["mask"])}   screen {f["screen"]}/{judged}  4-fold {f["correct"]}/{count}')
    if result['mask'] == champion:
        lines.append(f'  chosen: champion (no challenger beat it by more than {tune_margin(count)} records)')
    else:
        lines.append(f'  chosen: {result["best_k"]} features {_names(result["mask"])} '
                     f'(beat champion by {result["correct"] - result["champion_correct"]})')
    return lines


def _recall_text(recall):
    return '-' if recall == RECALL_ABSENT else f'{recall}%'


def describe_tune(result, obj=DEFAULT_OBJECT):
    """Lines for the CLI over the object's zones; RuntimeError (error code name) when the chip could not tune."""
    if not result['ok']:
        raise RuntimeError(f'TUNE failed: {MODEL_ERRORS.get(result["error"], result["error"])}')
    names = OBJECTS[obj]
    lines = _tune_summary(result) + [
        '  ' + '  '.join(f'{names[c]} {_recall_text(r)} ({n})'
                         for c, (r, n) in enumerate(zip(result['recall'][:len(names)], result['counts'])))]
    lines += confusion_table(result['confusion'], obj)
    advice = result['advice']
    if not advice:
        return lines + [f'  提案: なし（すべての部位が {ADVICE_RECALL_PCT}% 以上）']
    return lines + _advice_lines(result['confusion'], advice, obj)


def recall_advice(recall):
    """Class 1..4 with the lowest recall below ADVICE_RECALL_PCT (ties -> lower label), 0 = none.

    From the slot header alone (no record counts), so a tie does not use the counts as model_tune.c does.
    """
    present = [(r, c + 1) for c, r in enumerate(recall) if r != RECALL_ABSENT]
    if not present:
        return 0
    lowest, label = min(present)
    return label if lowest < ADVICE_RECALL_PCT else 0


def describe_slot(slot):
    """One `info` line from a SLOT reply: count, object, tuned feature set, recall per class and advice."""
    obj = object_of_name(slot['name'])
    head = f'slot {slot["slot"]}: {slot["count"]} records  {describe_object(obj)}'
    if not slot['tuned_k'] and not slot['mask']:
        return f'{head}  (未調整)'
    names = ', '.join(fs.FEATURE_NAMES[j] for j in mask_features(slot['mask']))
    zones = OBJECTS[obj]
    recall = ' '.join(f'{zones[c]} {_recall_text(r)}' for c, r in enumerate(slot['recall'][:len(zones)]))
    advice = recall_advice(slot['recall'][:len(zones)])
    return (f'{head}  k={slot["tuned_k"]} [{names}]  recall {recall}  '
            f'提案: {_zone(advice, obj) if advice else "なし"}')


def describe_info(store):
    """INFO, then SLOT for every slot with records (call inside paused())."""
    i = store.info()
    lines = [f'firmware: {i["firmware"] or "unknown (older build)"}', f'active slot: {i["active"]}']
    return lines + [describe_slot(store.slot(n)) if c else f'slot {n}: 0 records' for n, c in enumerate(i['counts'])]


def confusion_table(confusion, obj=DEFAULT_OBJECT):
    """Chosen set's matrix over the object's classes: rows = true class, columns = predicted class."""
    names = OBJECTS[obj]
    size = len(names)
    return (['  混同行列（行 = 正解、列 = 判定）', '        ' + ' '.join(f'{n:>4}' for n in names)]
            + [f'  {names[t]:<4}  ' + ' '.join(f'{n:>4}' for n in row[:size])
               for t, row in enumerate(confusion[:size])])


def _advice_lines(confusion, advice, obj=DEFAULT_OBJECT):
    """Both directions of the advised class A and its main partner B, then what to collect."""
    names = OBJECTS[obj]
    a = advice - 1
    row = confusion[a]
    b = max((p for p in range(len(names)) if p != a), key=lambda p: (row[p], -p))
    counts = [sum(r) for r in confusion]
    name_a, name_b = names[a], names[b]
    first = (f'  提案: {_zone(advice, obj)}のデータを追加してください。{name_a} → {name_b} {row[b]} 件、'
             f'{name_b} → {name_a} {confusion[b][a]} 件（件数 {name_a} {counts[a]} 対 {name_b} {counts[b]}）')
    if counts[a] < counts[b]:
        return [first, f'  （件数の少ない部位に境界が不利になるので、まず {name_a} を {name_b} と同程度まで増やす）']
    return [first, '  （件数は足りているので、境界付近の押し方（弱め・位置違い）を増やす）']
