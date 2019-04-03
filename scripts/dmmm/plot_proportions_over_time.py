import itertools
import os
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

matplotlib.rc('pdf', fonttype=42)

translation = {
    1: 'eager asker',
    2: 'editor/moderator',
    3: 'careful asker',
    4: 'answerer',
    5: 'clarifier',
}

health_metrics = {
    'answers-per-question':
    lambda df: df['num_answers'] / df['num_questions'],
    'percent-accepted-ans':
    lambda df: df['num_with_acc_ans'] / df['num_questions'],
    'percent-answered':
    lambda df: (df['num_questions'] - df['num_unanswered']) / df['num_questions'],
    'avg-response-time':
    lambda df: df['avg_response_time']
}

#health_metric = health_metrics[sys.argv[1]]
#healths = pd.read_csv(sys.argv[2])

community_name = None
frame = pd.DataFrame(columns=['topic', 'probability', 'month'])
for filename in sys.argv[1:]:
    basename = os.path.basename(filename)
    x = int(basename.split('-')[1]) + 1
    community_name = basename.split('-')[0]

    props_df = pd.read_csv(filename)
    if len(props_df) == 0:
        for topic in translation.keys():
            props_df = props_df.append({
                'topic': topic,
                'probability': 1.0 / len(translation)
            },
                                       ignore_index=True)
    props_df['month'] = x
    frame = pd.concat([frame, props_df]).fillna(0)

# define the start of the time series to be plotted as the first month
# where there are at least 100 sessions
counts_frame = pd.read_csv(
    "../../build/counts-by-month/{}.csv".format(community_name))
first_month = counts_frame[counts_frame['num-sessions'] >= 100]['month'].iloc[0]
frame['month'] = frame['month'] - first_month
frame = frame[frame['month'] >= 0]

sns.set_palette('magma')

df = frame
fig, ax = plt.subplots(1, 1, figsize=(4.32, 2.92))
labels = []
markers = itertools.cycle(('o', '^', 's', '*', 'd'))
for label, df in df.groupby('topic'):
    df.plot(
        x='month',
        y='probability',
        kind='line',
        ax=ax,
        markevery=5,
        marker=next(markers))
    labels.append(translation[label])
ax.set_ylim((0.0, 0.7))
ax.set_ylabel('probability')
ax.minorticks_on()
ax.margins(x=0)
sns.despine()
lines, _ = ax.get_legend_handles_labels()
legend = ax.legend(lines, labels, loc='best')
legend.remove()
plt.tight_layout()
plt.savefig("{}-props-over-time.pdf".format(community_name))
