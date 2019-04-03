import os
import sys

import matplotlib as mpl
mpl.use('Agg')

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

sns.set_palette('magma', n_colors=15)
plt.rcParams['font.size'] = 14
mpl.rc('pdf', fonttype=42)

bold_labels = set(['answer (oq)', 'comment (oq)', 'comment (oa-oq)'])

#for filename in os.listdir(sys.argv[1]):
for filename in sys.argv[1:]:
    plt.figure(figsize=(6.4, 5.0))
    plt.ylim(0, 0.6)
    #dset = pd.read_csv(os.path.join(sys.argv[1], filename))
    dset = pd.read_csv(filename)
    x = 'topic' if filename.endswith('proportions.csv') else 'action'
    sns.set_style('ticks')
    plot = sns.barplot(x=x, y="probability", data=dset)
    if x == 'action':
        plt.xticks(rotation=90)
    sns.despine()
    plot.minorticks_on()
    plot.tick_params(axis='x', which='minor', bottom='off')
    plot.set_xlabel('')
    plt.tight_layout()
    for ticklabel in plot.get_xticklabels():
        if ticklabel.get_text() in bold_labels:
            ticklabel.set_fontweight('bold')
            ticklabel.set_color('#381B5F')
    plt.savefig(os.path.basename(filename.replace('.csv', '.pdf')))
    plt.close()
