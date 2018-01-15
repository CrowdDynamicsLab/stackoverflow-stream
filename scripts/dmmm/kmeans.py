import matplotlib as mpl
mpl.use('Agg')

import numpy as np
import os
from sklearn.cluster import KMeans
from sklearn.metrics import silhouette_score
import seaborn
import pandas as pd
import csv
import matplotlib.pyplot as plt
from scipy.stats import ttest_ind
from scipy.spatial.distance import cdist, pdist
import sys

translation = {
    1: 'eager asker',
    2: 'editor/moderator',
    3: 'careful asker',
    4: 'answerer',
    5: 'clarifier',
}

health_metrics = {
  'answers-per-question': lambda df: df['num_answers'] / (df['num_questions'] + 1),
  'percent-accepted-ans': lambda df: df['num_with_acc_ans'] / (df['num_questions'] + 1),
  'percent-answered':     lambda df: (df['num_questions'] - df['num_unanswered']) / (df['num_questions'] + 1),
  'avg-response-time':    lambda df: df['avg_response_time']
}

health_display = {
  'answers-per-question': ('Answers / Questions', None),
  'percent-accepted-ans': ('% of questions w/ accepted answer', (0.0, 0.8)),
  'percent-answered':     ('% of questions answered', (0.2, 1.0)),
  'avg-response-time':    ('Days until first answer', None),
}

health_metric = health_metrics[sys.argv[1]]
frame = pd.DataFrame(columns=list(translation.keys()))

for filename in sys.argv[2:]:
    basename = os.path.basename(filename)
    x = int(basename.split('-')[1]) + 1
    community_name = basename.split('-')[0]

    props_df = pd.read_csv(filename)
    if len(props_df) == 0:
        for topic in translation.keys():
            props_df = props_df.append({'topic': topic, 'probability': 1.0
                / len(translation)}, ignore_index=True)
    props_df['network'] = community_name
    props_df = props_df.pivot(index='network', columns='topic', values='probability')
    frame = pd.concat([frame, props_df]).fillna(0)

print(sorted(list(set(frame.index.tolist()))))
print(len(set(frame.index.tolist())))
print("Num groups: {}".format(len(frame.groupby(level=0))))

sizes = {label:len(df) for label, df in frame.groupby(level=0)}

filtered = frame.groupby(level=0).filter(lambda x: len(x) >= 24)
filtered = filtered.groupby(level=0).apply(lambda x: x[12:])
avg_props = filtered.groupby(level=0).mean()
print(avg_props)

best_clus = 2
best_silhouette = -1
for clus_num in range(2, 10):
    clusterer = KMeans(n_clusters=clus_num, init='k-means++', random_state=42)
    cluster_labels = clusterer.fit_predict(avg_props)
    silhouette_avg = silhouette_score(avg_props, cluster_labels)

    print("{} clusters gives silhouette: {}".format(clus_num, silhouette_avg))

    if silhouette_avg > best_silhouette:
        best_clus = clus_num
        best_silhouette = silhouette_avg

print("Using {} clusters".format(best_clus))
clusterer = KMeans(n_clusters=best_clus, init='k-means++', random_state=42)
cluster_labels = clusterer.fit_predict(avg_props)

for i, label in enumerate(cluster_labels):
    name = avg_props.ix[i].name
    avg_props.ix[i, 'cluster'] = label

    health_frame = pd.read_csv("../../build/health/{}-health.csv".format(name))
    metric = health_metric(health_frame)
    avg_props.ix[i, 'health'] = metric.mean()

group1 = avg_props[avg_props['cluster'] == 0]['health']
group2 = avg_props[avg_props['cluster'] == 1]['health']

print("Group 1: {} (+/- {}".format(group1.mean(), group1.std()))
print("Group 2: {} (+/- {}".format(group2.mean(), group2.std()))

statistic, pval = ttest_ind(group1, group2, equal_var=False)
print("t-statistic: {}, pvalue: {}".format(statistic, pval))

plt.figure(figsize=(3.5,3))
colors = seaborn.color_palette('magma', n_colors=5)[3:]
plot = seaborn.lvplot(data=[group1, group2], palette=colors)
seaborn.despine()
plot.minorticks_on()
plot.tick_params(axis='x', which='minor', bottom='off')
plot.set_xlabel('')
h_display = health_display[sys.argv[1]]
plot.set_ylabel(h_display[0])
if h_display[1] is not None:
    plot.set_ylim(h_display[1])
labels = ["Cluster {}".format(i) for i in range(1, 3)]
plot.set_xticklabels(labels)
plt.tight_layout()
plt.savefig("{}.pdf".format(sys.argv[1]))
plt.close()

for i, centroid in enumerate(clusterer.cluster_centers_):
    print()
    print("Cluster {}:".format(i))
    print(centroid)

    group = avg_props[avg_props['cluster'] == i]
    print("Size: {}".format(len(group)))
    for idx, row in group.iterrows():
        print("{}: {} (size {})".format(row.name, row['health'], sizes[row.name]))

    group = group.drop(['health', 'cluster'], axis=1)
    plotme = pd.melt(group, var_name='topic', value_name='probability')

    plt.figure(figsize=(3.5, 3.8))
    colors = seaborn.color_palette('magma', n_colors=8)[3:]
    ax = seaborn.lvplot(x='topic', y='probability', data=plotme,
        palette=colors)
    plt.xticks(rotation=45)
    labels = [translation[k] for k in range(1, 6)]
    ax.set_xticklabels(labels)
    ax.set_ylim((0, 0.6))
    seaborn.despine()
    ax.minorticks_on()
    ax.tick_params(axis='x', which='minor', bottom='off')
    ax.set_xlabel('')
    plt.tight_layout()
    plt.savefig("centroid{}.pdf".format(i))
    plt.close()
