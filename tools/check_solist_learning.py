"""Small synthetic-only calibration; never uses pressure validation labels."""
import json
import numpy as np
from validate_solist_ai import Device, DEFAULT


def run():
    results = []
    path = DEFAULT/'results/solist-sanity-characterization-current.json'
    with path.with_suffix('.uart.log').open('w', encoding='utf8', buffering=1) as log:
        d = Device('COM7', log)
        try:
            d.command('MODE', 1)
            for activation in (1, 3, 2, 0):
                for amplitude in (1, 4):
                    x = np.zeros((4, 12))
                    x[np.arange(4), np.arange(4)] = amplitude
                    d.initialize(12, 32, 1, activation)
                    for epoch in range(1, 17):
                        for i in range(4):
                            d.sample(x[i], i+1)
                        if epoch in (1, 4, 16):
                            scores = [d.sample(row)[0].tolist() for row in x]
                            pred = (np.argmax(scores, axis=1)+1).tolist()
                            r = dict(activation=activation, amplitude=amplitude,
                                     epochs=epoch, scores=scores, predictions=pred)
                            results.append(r)
                            path.write_text(json.dumps(results, indent=2), encoding='utf8')
                            print(activation, amplitude, epoch, pred, flush=True)
        finally:
            d.command('MODE', 0)
            d.serial.close()


if __name__ == '__main__':
    run()
