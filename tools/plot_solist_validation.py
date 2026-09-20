"""Render actual-chip comparison; each point is one run, not a new recording."""
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from analyze_pressure import DEFAULT


def main():
    out = DEFAULT/'results'
    report = json.loads((out/'solist-chip-validation.json').read_text(encoding='utf8'))
    assert report['status'] == 'complete'
    patterns = ['features12','wave64','wave128']
    names = ['12 shape features','Raw waveform: 64 points','Raw waveform: 128 points']
    colors = ['#17675c','#c26b28','#5769aa']
    splits = ['chronological','reverse_chronological']
    fig, axes = plt.subplots(1, 2, figsize=(11, 4), layout='constrained')
    for ax, key, title in zip(axes, ['accuracy','balanced_accuracy'],
                             ['Accuracy (22 held-out events / run)', 'Mean recall across the four classes']):
        for i, pattern in enumerate(patterns):
            for j, split in enumerate(splits):
                runs = [r for r in report['runs'] if r['pattern'] == pattern and r['split'] == split]
                yy = i+(j-.5)*.28
                vals = [r['metrics'][key]*100 for r in runs]
                ax.plot([min(vals),max(vals)], [yy,yy], color=colors[i], lw=3)
                ax.scatter(vals, yy+np.linspace(-.035,.035,len(vals)), c=colors[i],
                           marker='o' if j == 0 else 's', s=38)
        ax.set(yticks=range(3), yticklabels=names, xlim=(0,100), xlabel='Percent', title=title)
        ax.invert_yaxis(); ax.grid(axis='x',alpha=.2)
    axes[0].axvline(100*14/22, color='#888888', ls='--', lw=1)
    axes[1].axvline(25, color='#888888', ls='--', lw=1)
    fig.suptitle('Solist-AI on the physical ML63Q2557 | 1 recording per zone, exploratory splits\n'
                 'Circle: first 3 / zone train; square: last 3 / zone train. Three seeds, same recordings.', fontsize=11)
    fig.savefig(out/'solist-chip-comparison.png', dpi=170)
    plt.close(fig)
    fig, axes = plt.subplots(2, 3, figsize=(10, 6.5), layout='constrained')
    for row, split in enumerate(splits):
        for col, pattern in enumerate(patterns):
            r = next(r for r in report['runs'] if r['pattern']==pattern and r['split']==split and r['seed']==1)
            cm = np.array(r['metrics']['confusion'])
            ax = axes[row,col]
            ax.imshow(cm, cmap='Blues', vmin=0, vmax=14)
            for i in range(4):
                for j in range(4):
                    ax.text(j,i,str(cm[i,j]),ha='center',va='center',color='white' if cm[i,j]>7 else '#172330')
            ax.set(xticks=range(4),yticks=range(4),xticklabels=['Tail','Back','Legs','Head'],
                   yticklabels=['Tail','Back','Legs','Head'],xlabel='Predicted',ylabel='Instructed zone group',
                   title=f'{names[col]}\n{split.replace("_", " ")}: {r["metrics"]["correct"]}/22')
    fig.suptitle('Physical-chip confusion matrices | seed 1 | counts, not probabilities',fontsize=12)
    fig.savefig(out/'solist-chip-confusion.png',dpi=170)
    plt.close(fig)


if __name__ == '__main__':
    main()
