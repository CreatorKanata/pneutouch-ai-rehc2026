"""Train and evaluate the physical Solist-AI through CN9 UART-B (COM7).

Requires the PAI1 validation firmware. No physical touches are required.
Every run resets the chip model; predictions send NO teacher label.
On exit, MODE 0 resumes physical acquisition. Training is volatile.
"""
import argparse
import binascii
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import serial
from analyze_pressure import DEFAULT, ROOT, score, describe


def encode(values):
    a = np.asarray(values, dtype='<f4')
    if not np.isfinite(a).all():
        raise ValueError('Nonfinite input')
    return ''.join(f'{int(v):04x}' for v in (a.view('<u4') >> 16))


def decode(words):
    a = np.array([int(w, 16) << 16 for w in words], dtype='<u4').view('<f4')
    if not np.isfinite(a).all():
        raise ValueError(f'Nonfinite chip output: {words}')
    return a.astype(float)


class Device:
    def __init__(self, port, log):
        self.serial = serial.Serial(port, 115200, timeout=.2, write_timeout=5)
        self.log = log
        self.token = 0

    def command(self, operation, *fields, timeout=8):
        payload = ','.join(['PAI1', operation, *map(str, fields)]).encode('ascii')
        packet = payload+f'*{binascii.crc_hqx(payload, 0xffff):04x}\n'.encode('ascii')
        self.log.write('TX '+packet.decode())
        if operation == 'MODE':
            # Live PNEU1 output is blocking and the UART has a small RX buffer.
            # Clear an incomplete command, then pace the mode handshake. Once
            # MODE 1 is acknowledged, acquisition is paused and bulk RX is safe.
            self.serial.write(b'\n')
            time.sleep(.05)
            self.serial.reset_input_buffer()
            for byte in packet:
                self.serial.write(bytes([byte]))
                time.sleep(.015)
        else:
            self.serial.write(packet)
        deadline = time.monotonic()+timeout
        while time.monotonic() < deadline:
            line = self.serial.readline().decode('ascii', errors='replace').strip()
            if not line:
                continue
            self.log.write('RX '+line+'\n')
            if line.startswith('# PAI1,ERROR,'):
                raise RuntimeError(line)
            if line.startswith('# PAI1,'+operation+','):
                response = line[2:].split(',')
                if response[2] != str(fields[0]):
                    raise RuntimeError('Response token mismatch: '+line)
                return response
        raise TimeoutError(f'No response to {operation}')

    def initialize(self, inputs, hidden, seed, activation):
        self.token += 1
        self.command('INIT', self.token, inputs, hidden, seed, activation)

    def sample(self, x, label=None):
        self.token += 1
        if label is None:
            r = self.command('PREDICT', self.token, encode(x))
            return decode(r[4:8]), int(r[3])
        r = self.command('TRAIN', self.token, int(label), encode(x))
        return int(r[3])


def partitions(zones):
    ids = np.arange(len(zones))
    for name, reverse in [('chronological', False), ('reverse_chronological', True)]:
        train = np.concatenate([np.flatnonzero(zones == z)[-3:] if reverse else
                                np.flatnonzero(zones == z)[:3] for z in np.unique(zones)])
        test = np.setdiff1d(ids, train)
        assert len(train) == 21 and len(test) == 22 and not np.intersect1d(train, test).size
        yield name, train, test


def transform(x, train, pattern):
    if pattern == 'features12':
        x = np.log(x)
        mean, sd = x[train].mean(0), x[train].std(0)
        sd = np.where(sd > 1e-10, sd, 1)
        # Keep most values in the nonsaturated range of hard sigmoid.
        return (x-mean)/(3*sd), dict(log=True, mean=mean.tolist(), scale=(3*sd).tolist())
    return x, dict(log=False, scaling='per-window baseline subtraction and absolute peak normalization')


def self_test(device, hidden, activation):
    x = np.zeros((4, 12))
    x[np.arange(4), np.arange(4)] = 1
    device.initialize(12, hidden, 1, activation)
    for _ in range(8):
        for i in range(4):
            device.sample(x[i], i+1)
    scores = [device.sample(row)[0].tolist() for row in x]
    predictions = (np.argmax(scores, axis=1)+1).tolist()
    result = dict(scores=scores, predictions=predictions, expected=[1, 2, 3, 4])
    if predictions != result['expected']:
        raise RuntimeError('Chip sanity check failed: '+json.dumps(result))
    return result


def run(args):
    source = DEFAULT/'results/solist-inputs.json'
    data = json.loads(source.read_text(encoding='utf8'))
    events = data['events']
    y = np.array([e['label'] for e in events])
    zones = np.array([e['zone'] for e in events])
    dest = args.output
    dest.parent.mkdir(parents=True, exist_ok=True)
    report = dict(status='running', utc=datetime.now(timezone.utc).isoformat(),
        backend='physical ML63Q2557 / SolistAi_Library_2_256_64.a', port=args.port,
        input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        library_sha256=hashlib.sha256((ROOT/'src/pneutouch-solist/S_Library/SolistAi_Library_2_256_64.a').read_bytes()).hexdigest(),
        parameters=dict(hidden=args.hidden, seeds=args.seeds, epochs=args.epochs,
                        activation=args.activation, forgetting_factor=1, alpha=1, gamma=0, leak=1,
                        input_bfloat16='float32 high 16 bits (truncate)', output='four one-hot targets, MSE'),
        initialization='ODL_Initialize/Reset followed by Beta=0, P=identity (ridge=1)',
        classes={'1':'しっぽ','2':'背中','3':'足全体','4':'首と頭'}, runs=[],
        limitations=['Exploratory, one recording per original zone.',
                     'Hyperparameters fixed before scored runs; no independent recording test.',
                     'Wave64 ready 1.375 s and wave128 ready 2.975 s after trigger.',
                     'Kernel timings exclude serial transfer, preprocessing and result copying.',
                     'Reverse split overlaps chronological training/test membership; do not pool as independent events.'])

    def save():
        dest.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf8')

    with dest.with_suffix('.uart.log').open('w', encoding='utf8', buffering=1) as log:
        device = Device(args.port, log)
        try:
            device.command('MODE', 1)
            report['sanity_check'] = self_test(device, args.hidden, args.activation)
            print('Physical-chip four-class sanity check passed.', flush=True)
            save()
            for pattern in args.patterns:
                x = np.array([e[pattern] for e in events])
                for split, train, test in partitions(zones):
                    scaled, preprocessing = transform(x, train, pattern)
                    for seed in args.seeds:
                        device.initialize(x.shape[1], args.hidden, seed, args.activation)
                        rng = np.random.default_rng(seed)
                        training_us, predictions, scores, inference_us, orders = [], [], [], [], []
                        for _ in range(args.epochs):
                            order = rng.permutation(train)
                            orders.append(order.tolist())
                            for index in order:
                                training_us.append(device.sample(scaled[index], y[index]))
                        for index in test:
                            output, elapsed = device.sample(scaled[index])
                            predictions.append(int(np.argmax(output)+1))
                            scores.append(output.tolist())
                            inference_us.append(elapsed)
                        result = dict(pattern=pattern, split=split, seed=seed,
                            train_indices=train.tolist(), test_indices=test.tolist(),
                            training_orders=orders, preprocessing=preprocessing,
                            predictions=predictions, targets=y[test].tolist(), scores=scores,
                            train_us=describe(training_us), predict_us=describe(inference_us),
                            metrics=score(y[test], np.array(predictions), np.arange(1, 5)))
                        report['runs'].append(result)
                        save()
                        print(pattern, split, seed, result['metrics']['correct'], '/', len(test),
                              'predict median', result['predict_us']['median'], 'us', flush=True)
            report['status'] = 'complete'
        except Exception as exc:
            report['status'] = 'failed'
            report['error'] = str(exc)
            raise
        finally:
            try:
                device.command('MODE', 0)
                report['acquisition_resumed'] = True
            except Exception as exc:
                report['acquisition_resumed'] = False
                report['resume_error'] = str(exc)
            device.serial.close()
            save()
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM7')
    parser.add_argument('--hidden', type=int, default=32)
    parser.add_argument('--activation', type=int, default=1)
    parser.add_argument('--epochs', type=int, default=4)
    parser.add_argument('--seeds', type=int, nargs='+', default=[1, 7, 42])
    parser.add_argument('--patterns', nargs='+', choices=['features12','wave64','wave128'],
                        default=['features12','wave64','wave128'])
    parser.add_argument('--output', type=Path, default=DEFAULT/'results/solist-chip-validation.json')
    run(parser.parse_args())
