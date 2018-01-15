import os
import sys

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

sns.set_palette('magma', n_colors=15)
plt.rcParams['font.size'] = 14

for filename in os.listdir(sys.argv[1]):
    plt.figure(figsize=(6.4, 5.8))
    plt.ylim(0, 0.6)
    dset = pd.read_csv(os.path.join(sys.argv[1], filename))
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
    plt.savefig(filename.replace('.csv', '.pdf'))
    plt.close()
