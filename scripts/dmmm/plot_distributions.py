import os
import sys

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

sns.set_style('whitegrid')

for filename in os.listdir(sys.argv[1]):
    plt.figure()
    plt.ylim(0, 0.65)
    dset = pd.read_csv(os.path.join(sys.argv[1], filename))
    x = 'topic' if filename.endswith('proportions.csv') else 'action'
    plot = sns.barplot(x=x, y="probability", data=dset)
    plt.savefig(filename.replace('.csv', '.png'))
    plt.close()