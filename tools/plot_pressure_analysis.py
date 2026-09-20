"""Render figures and readable tables from the saved offline pressure analysis."""
import argparse
import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
import numpy as np

from analyze_pressure import DEFAULT, NAMES, load

COLORS=['#166b9c','#e28723','#368257','#77a94b','#b84d72','#7961ab','#60584d']


def setup():
    font=Path('C:/Windows/Fonts/meiryo.ttc')
    if font.exists():
        font_manager.fontManager.addfont(str(font))
        plt.rcParams['font.family']=font_manager.FontProperties(fname=str(font)).get_name()
    plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False,
                         'figure.facecolor':'white','axes.titlepad':12,'axes.unicode_minus':False})


def scatter_summary(ax, events, feature, scale=1, order=range(1,8)):
    for x,z in enumerate(order):
        v=np.array([e[feature] for e in events if e['zone_id']==z],dtype=float)/scale
        ax.scatter(x+np.linspace(-.12,.12,len(v)),v,s=28,color=COLORS[z-1],alpha=.8,zorder=3)
        ax.plot([x-.24,x+.24],[np.median(v)]*2,color='black',lw=2)
    ax.set_xticks(range(7),[NAMES[z-1] for z in order],rotation=22,ha='right')
    ax.grid(axis='y',alpha=.2)


def render(folder):
    setup()
    out=folder/'results'
    r=json.loads((out/'results.json').read_text(encoding='utf8'))
    events=[e for e in r['events'] if e['kind']=='seven_zone' and e['complete']]
    manifest=json.loads((folder/'manifest.json').read_text(encoding='utf8'))
    captures={Path(m['file']).name:load(folder/m['file'])[3:] for m in manifest['captures']}
    saved=[]
    def save(fig,name):
        fig.savefig(out/name,dpi=170,bbox_inches='tight')
        plt.close(fig)
        saved.append(name)

    fig,axs=plt.subplots(2,2,figsize=(12.2,8.4),layout='constrained')
    for ax,key,title,unit,scale in zip(axs.flat,
        ['rise_ms','area_width_ms','release_width_ms','area_ratio'],
        ['立ち上がり 10→90%','正側の面積 ÷ ピーク','負側の半値幅','負側面積 ÷ 正側面積'],
        ['ms','ms','ms','比（無次元）'],[1,1,1,1]):
        scatter_summary(ax,events,key,scale)
        ax.set(title=title,ylabel=unit)
    fig.suptitle('43回の完全波形：点は1回、黒線は中央値\n各部位1記録内の比較。接触力・速度は未計測',fontsize=14)
    save(fig,'feature-distributions.png')

    fig,axs=plt.subplots(1,2,figsize=(12,4.7),layout='constrained')
    order=manifest['topology']
    for ax,key,title,scale in zip(axs,['rise_ms','peak_counts'],['立ち上がり 10→90%（ms）','正側ピーク（百万 counts）'],[1,1e6]):
        scatter_summary(ax,events,key,scale,order)
        med=[np.median([e[key] for e in events if e['zone_id']==z])/scale for z in order]
        ax.plot(range(7),med,color='#555555',alpha=.5,lw=1)
        ax.set(title=title,xlabel='センサー側 → 設計上の接続順 → 末端')
    fig.suptitle('接続順だけでは応答を説明できない（実物の内部経路は未確認）',fontsize=14)
    save(fig,'topology.png')

    fig,axs=plt.subplots(2,7,figsize=(16,6.5),sharex='row',sharey='row',layout='constrained')
    for z in range(1,8):
        ee=[e for e in events if e['zone_id']==z]
        for row,center,amplitude,sign in [(0,'peak_s','peak_counts',1),(1,'trough_s','trough_counts',-1)]:
            ax=axs[row,z-1]
            for e in ee:
                t,y=captures[e['file']]
                keep=(t>=e[center]-.45)&(t<=e[center]+.65)
                ax.plot((t[keep]-e[center])*1000,sign*(y[keep]-e['baseline_counts'])/e[amplitude],
                        color=COLORS[z-1],alpha=.48,lw=1,marker='.',ms=2)
            ax.axhline(0,color='#888',lw=.6)
            ax.set(xlim=(-450,650),ylim=(-.3,1.15),xticks=[-250,0,500])
            if row==0:
                ax.set_title(f'{NAMES[z-1]}\nn={len(ee)}')
            else:
                ax.set_xlabel('ピークからの時刻 (ms)')
    axs[0,0].set_ylabel('正側 / 正側ピーク')
    axs[1,0].set_ylabel('負側の大きさ / 負側ピーク')
    fig.suptitle('振幅とピーク時刻を揃えた波形（点は実サンプル）\n正負を別々に整列。接触時刻・保持時間の差はこの図では評価しない',fontsize=14)
    save(fig,'normalized-waveforms.png')

    fig,axs=plt.subplots(1,2,figsize=(12,5.5),layout='constrained')
    for ax,data,labels,title in [
        (axs[0],r['exploratory_checks']['shape_ratios']['centroid']['loo'],NAMES,'7部位'),
        (axs[1],r['exploratory_four_class_checks']['shape_ratios']['centroid']['loo'],['しっぽ','背中','脚全体','首と頭'],'4群')]:
        cm=np.array(data['confusion'])
        row_fraction=cm/cm.sum(axis=1,keepdims=True)
        ax.imshow(row_fraction,cmap='Blues',vmin=0,vmax=1)
        for i,j in np.ndindex(cm.shape):
            ax.text(j,i,str(cm[i,j]),ha='center',va='center',color='white' if row_fraction[i,j]>.55 else 'black')
        ax.set(xticks=range(len(labels)),xticklabels=labels,yticks=range(len(labels)),yticklabels=labels,
               xlabel='予測',ylabel='指定部位',title=f'{title}：{data["correct"]}/{data["n"]} = {100*data["accuracy"]:.1f}%')
        ax.tick_params(axis='x',rotation=40)
    fig.suptitle('形状＋正負比・最近重心法：1回ずつ除く探索評価\n同一記録を学習と評価で共有。独立した使用時の正解率ではない',fontsize=14)
    save(fig,'confusion-exploratory.png')

    fig,axs=plt.subplots(1,2,figsize=(12,5),layout='constrained')
    earlier=[e for e in r['events'] if e['kind']=='alternating' and e['complete']]
    for ax,key,title,scale in zip(axs,['peak_counts','release_width_ms'],['正側ピーク（百万 counts）','負側の半値幅（ms）'],[1e6,1]):
        for z in [1,2]:
            med=[]
            for i,ee in enumerate([earlier,events]):
                v=np.array([e[key] for e in ee if e['zone_id']==z])/scale
                x=i+(z-1.5)*.19
                ax.scatter(x+np.linspace(-.04,.04,len(v)),v,color=COLORS[z-1],s=30,alpha=.8)
                med.append(np.median(v))
            ax.plot(np.arange(2)+(z-1.5)*.19,med,color=COLORS[z-1],lw=2,label=NAMES[z-1])
        ax.set(xticks=[0,1],xticklabels=['先行の交互試行\n各8回・順序による仮ラベル','部位別の連続試行\n各5回'],title=title)
        ax.legend()
        ax.grid(axis='y',alpha=.2)
    fig.suptitle('しっぽ／背中：記録をまたぐと振幅の関係が変化\n線は中央値。別の日・別の人の検証ではない',fontsize=14)
    save(fig,'cross-recording.png')

    fig,ax=plt.subplots(figsize=(11.5,5.5),layout='constrained')
    families=['amplitude','press_shape','release_shape','shape_ratios','compact_time_ratios','spectral_shape','baseline_control']
    labels=['振幅','正側形状','負側形状','形状＋正負比','少数の時間・比','周波数形状','基準値のみ※']
    for i,(split,name,col) in enumerate([('loo','1回ずつ除外 (43回)','#256b9c'),('chronological','前半→後半 (22回)','#d38c29'),('reverse_chronological','後半→前半 (22回)','#478b70')]):
        vals=[r['exploratory_checks'][f]['centroid'][split]['accuracy']*100 for f in families]
        ax.bar(np.arange(len(labels))+(i-1)*.24,vals,.23,label=name,color=col)
    ax.set(xticks=range(len(labels)),xticklabels=labels,ylabel='正解率 (%)',ylim=(0,100))
    ax.legend(loc='upper left',ncol=3,fontsize=9)
    ax.grid(axis='y',alpha=.2)
    ax.set_title('7部位・最近重心法の探索比較（すべて同日の同じ部位別記録）\n※ 基準値のみでも分類できる：記録条件の混入を示す診断。採用する特徴ではない',fontsize=12)
    save(fig,'model-comparison.png')

    with (out/'zone-statistics.csv').open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.writer(f)
        writer.writerow(['zone_id','zone_name','feature','n','min','q25','median','q75','max','mean','sd'])
        for z,stats in r['zone_statistics'].items():
            for key,s in stats.items():
                writer.writerow([z,NAMES[int(z)-1],key]+[s[k] for k in ['n','min','q25','median','q75','max','mean','sd']])
    with (out/'model-scores.csv').open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.writer(f)
        writer.writerow(['classes','family','model','split','correct','n','accuracy','balanced_accuracy','macro_f1'])
        for num,results in [(7,r['exploratory_checks']),(4,r['exploratory_four_class_checks'])]:
            for family,models in results.items():
                for model,checks in models.items():
                    for split in ['loo','chronological','reverse_chronological','balanced_first_five']:
                        s=checks[split]
                        writer.writerow([num,family,model,split]+[s[k] for k in ['correct','n','accuracy','balanced_accuracy','macro_f1']])
    print('Saved:',', '.join(saved))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,default=DEFAULT)
    render(parser.parse_args().data)
