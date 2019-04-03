import itertools
import matplotlib as mpl
mpl.use('Agg')

import numpy as np
import os
from sklearn.cluster import KMeans
from sklearn import metrics
import scikit_posthocs as sp
import seaborn
import pandas as pd
import csv
import matplotlib.pyplot as plt
from scipy import stats
from scipy.spatial.distance import cdist, pdist
import sys
from statsmodels.stats.multitest import multipletests

mpl.rc('pdf', fonttype=42)

translation = {
    1: 'eager asker',
    2: 'editor/moderator',
    3: 'careful asker',
    4: 'answerer',
    5: 'clarifier',
}

health_metrics = {
    'answers-per-question':
    lambda df: df['num_answers'] / (df['num_questions'] + 1),
    'percent-accepted-ans':
    lambda df: df['num_with_acc_ans'] / (df['num_questions'] + 1),
    'percent-answered':
    lambda df: (df['num_questions'] - df['num_unanswered']) / (df['num_questions'] + 1),
    'avg-response-time':
    lambda df: df['avg_response_time']
}

health_display = {
    'answers-per-question': ('Answers / Questions', None),
    'percent-accepted-ans': ('Fraction of questions w/ accepted answer', (0.0, 1.0)),
    'percent-answered': ('Fraction of questions answered', (0.0, 1.0)),
    'avg-response-time': ('Days until first answer', None),
}

group_map = {
    '3dprinting': 'technology',
    'academia': 'life/arts',
    'ai': 'science',
    'android': 'technology',
    'anime': 'culture/recreation',
    'apple': 'technology',
    'arabic': 'culture/recreation',
    'arduino': 'technology',
    'askubuntu': 'technology',
    'astronomy': 'science',
    'aviation': 'professional',
    'avp': 'life/arts',
    'beer': 'culture/recreation',
    'bicycles': 'culture/recreation',
    'biology': 'science',
    'bitcoin': 'technology',
    'blender': 'technology',
    'boardgames': 'culture/recreation',
    'bricks': 'culture/recreation',
    'buddhism': 'culture/recreation',
    'chemistry': 'science',
    'chess': 'culture/recreation',
    'chinese': 'culture/recreation',
    'christianity': 'culture/recreation',
    'civicrm': 'technology',
    'codegolf': 'technology',
    'codereview': 'technology',
    'coffee': 'life/arts',
    'cogsci': 'science',
    'computergraphics': 'technology',
    'cooking': 'life/arts',
    'craftcms': 'technology',
    'crafts': 'life/arts',
    'crypto': 'technology',
    'cs': 'science',
    'cstheory': 'science',
    'datascience': 'technology',
    'dba': 'technology',
    'diy': 'life/arts',
    'drupal': 'technology',
    'dsp': 'technology',
    'earthscience': 'science',
    'ebooks': 'technology',
    'economics': 'science',
    'electronics': 'technology',
    'elementaryos': 'technology',
    'ell': 'culture/recreation',
    'emacs': 'technology',
    'engineering': 'technology',
    'english': 'culture/recreation',
    'es.stackoverflow': 'technology',
    'esperanto': 'culture/recreation',
    'ethereum': 'technology',
    'expatriates': 'life/arts',
    'expressionengine': 'technology',
    'fitness': 'life/arts',
    'freelancing': 'professional',
    'french': 'culture/recreation',
    'gamedev': 'technology',
    'gaming': 'culture/recreation',
    'gardening': 'life/arts',
    'genealogy': 'life/arts',
    'german': 'culture/recreation',
    'gis': 'technology',
    'graphicdesign': 'life/arts',
    'ham': 'culture/recreation',
    'hardwarerecs': 'technology',
    'health': 'life/arts',
    'hermeneutics': 'culture/recreation',
    'hinduism': 'culture/recreation',
    'history': 'culture/recreation',
    'homebrew': 'culture/recreation',
    'hsm': 'science',
    'islam': 'culture/recreation',
    'italian': 'culture/recreation',
    'ja.stackoverflow': 'technology',
    'japanese': 'culture/recreation',
    'joomla': 'technology',
    'judaism': 'culture/recreation',
    'korean': 'culture/recreation',
    'languagelearning': 'culture/recreation',
    'latin': 'culture/recreation',
    'law': 'life/arts',
    'lifehacks': 'life/arts',
    'linguistics': 'science',
    'magento': 'technology',
    'martialarts': 'culture/recreation',
    'math': 'science',
    'matheducators': 'science',
    'mathematica': 'technology',
    'mathoverflow.net': 'science',
    'mechanics': 'culture/recreation',
    'moderators': 'professional',
    'monero': 'technology',
    'money': 'life/arts',
    'movies': 'life/arts',
    'music': 'life/arts',
    'musicfans': 'life/arts',
    'mythology': 'culture/recreation',
    'networkengineering': 'technology',
    'opendata': 'technology',
    'opensource': 'professional',
    'outdoors': 'culture/recreation',
    'parenting': 'life/arts',
    'patents': 'business',
    'pets': 'life/arts',
    'philosophy': 'science',
    'photo': 'life/arts',
    'physics': 'science',
    'pm': 'business',
    'poker': 'culture/recreation',
    'politics': 'culture/recreation',
    'portuguese': 'culture/recreation',
    'productivity': 'life/arts',
    'programmers': 'technology',
    'pt.stackoverflow': 'technology',
    'puzzling': 'culture/recreation',
    'quant': 'business',
    'raspberrypi': 'technology',
    'retrocomputing': 'technology',
    'reverseengineering': 'technology',
    'robotics': 'technology',
    'rpg': 'culture/recreation',
    'ru.stackoverflow': 'technology',
    'rus': 'culture/recreation',
    'russian': 'culture/recreation',
    'salesforce': 'technology',
    'scicomp': 'science',
    'scifi': 'life/arts',
    'security': 'technology',
    'serverfault': 'technology',
    'sharepoint': 'technology',
    'sitecore': 'technology',
    'skeptics': 'culture/recreation',
    'softwarerecs': 'technology',
    'sound': 'technology',
    'space': 'technology',
    'spanish': 'culture/recreation',
    'sports': 'culture/recreation',
    'sqa': 'technology',
    'stackapps': 'technology',
    'stackoverflow': 'technology',
    'startups': 'business',
    'stats': 'science',
    'superuser': 'technology',
    'sustainability': 'life/arts',
    'tex': 'technology',
    'tor': 'technology',
    'travel': 'culture/recreation',
    'tridion': 'technology',
    'unix': 'technology',
    'ux': 'technology',
    'vi': 'technology',
    'webapps': 'technology',
    'webmasters': 'technology',
    'windowsphone': 'technology',
    'woodworking': 'culture/recreation',
    'wordpress': 'technology',
    'workplace': 'professional',
    'worldbuilding': 'life/arts',
    'writers': 'professional'
}

frame = pd.DataFrame(columns=list(translation.keys()))

for filename in sys.argv[1:]:
    basename = os.path.basename(filename)
    community_name = basename.split('-')[0]

    props_df = pd.read_csv(filename)
    if len(props_df) == 0:
        for topic in translation.keys():
            props_df = props_df.append({
                'topic': topic,
                'probability': 1.0 / len(translation)
            },
                                       ignore_index=True)
    props_df['network'] = community_name
    props_df = props_df.pivot(
        index='network', columns='topic', values='probability')
    frame = pd.concat([frame, props_df], sort=True).fillna(0)

print("Num networks: {}".format(len(frame.groupby(level=0))))
sizes = {}
for network in frame.index:
    counts_frame = pd.read_csv(
            "../../build/counts-by-month/{}.csv".format(network))
    first_month = counts_frame[counts_frame['num-sessions'] >= 100]['month'].iloc[0]
    sizes[network] = len(counts_frame[counts_frame['month'] >= first_month])

filtered = frame.groupby(level=0).filter(lambda x: len(x) >= 24)

processed = set([])


def drop_first_12(x):
    if x.iloc[0].name not in processed:
        processed.add(x.iloc[0].name)
        return x[12:]
    return x


filtered = filtered.groupby(level=0).apply(drop_first_12)
avg_props = filtered.groupby(level=0).mean()
print(avg_props)

for network in avg_props.index:
    avg_props.at[network, 'group'] = group_map[network]

    health_frame = pd.read_csv(
        "../../build/health/{}-health.csv".format(network))
    for metric_name, metric_func in health_metrics.items():
        metric = metric_func(health_frame)[12:]
        avg_props.at[network, metric_name] = metric.mean()

for group in set(value for key, value in group_map.items()):
    group_df = avg_props[avg_props['group'] == group]
    print("Group: {}, size: {}".format(group, len(group_df)))
    for index, row in group_df.iterrows():
        print(" -> {} ({})".format(index, sizes[index]))
    print()

groups_to_compare = [
    'technology', 'culture/recreation', 'life/arts', 'science'
]
avg_props = avg_props[avg_props['group'].isin(groups_to_compare)]

def plot_health(avg_props, health_col):
    plt.figure(figsize=(3.5, 3))
    colors = seaborn.color_palette('magma', n_colors=8)[3:]
    ax = seaborn.boxenplot(
        x='group',
        y=health_col,
        data=avg_props,
        order=sorted(groups_to_compare),
        palette=colors)

    plt.xticks(rotation=45)
    labels = ['cult.', 'life', 'sci.', 'tech.']
    ax.set_xticklabels(labels)
    seaborn.despine()
    ax.minorticks_on()
    ax.tick_params(axis='x', which='minor', bottom=False)
    ax.set_xlabel('')

    ylabel, ylim = health_display[health_col]
    ax.set_ylabel(ylabel)
    if ylim:
        ax.set_ylim(ylim)

    plt.tight_layout()
    plt.savefig("{}.pdf".format(health_col))
    plt.close()


def compare_values(values_to_compare):
    pvals = []

    for value in values_to_compare:
        groups = [
            avg_props[avg_props['group'] == group][value]
            for group in groups_to_compare
        ]

        statistic, pval = stats.kruskal(*groups)
        pvals.append(pval)

    adj_pvals = multipletests(pvals, alpha=0.05, method='holm')[1]

    for idx, value in enumerate(values_to_compare):
        name = translation[value] if type(value) is int else value
        print("Comparing {}".format(name))

        for group in groups_to_compare:
            group = avg_props[avg_props['group'] == group]
            print("{}: {} (+/- {})".format(
                group['group'][0], group[value].mean(), group[value].std()))

        print("H-test adjusted p-value: {}".format(adj_pvals[idx]))
        print()

        opt = pd.get_option('display.float_format')
        pd.set_option('display.float_format', '{:.3g}'.format)
        print(
            sp.posthoc_conover(
                avg_props, val_col=value, group_col='group', p_adjust='holm'))
        pd.set_option('display.float_format', opt)
        print()

        if not type(value) is int:
            plot_health(avg_props, value)


proportions_pvals = compare_values(list(translation))
health_pvals = compare_values(list(health_metrics))

for group_name in groups_to_compare:
    group = avg_props[avg_props['group'] == group_name]
    print("Group: {}, size; {}".format(group_name, len(group)))

    group = group.drop(['group'] + list(health_metrics), axis=1)
    plotme = pd.melt(group, var_name='topic', value_name='probability')

    plt.figure(figsize=(3.5, 3.8))
    colors = seaborn.color_palette('magma', n_colors=8)[3:]
    ax = seaborn.boxenplot(
        x='topic', y='probability', data=plotme, palette=colors)
    plt.xticks(rotation=45)
    labels = [translation[k] for k in range(1, 6)]
    ax.set_xticklabels(labels)
    ax.set_ylim((0, 0.6))
    seaborn.despine()
    ax.minorticks_on()
    ax.tick_params(axis='x', which='minor', bottom=False)
    ax.set_xlabel('')
    plt.tight_layout()
    plt.savefig("{}.pdf".format(group_name.replace('/', '-')))
    plt.close()

#for n_clusters in range(2, 4):
#    clusterer = KMeans(
#        n_clusters=n_clusters, init='k-means++', random_state=42)
#    cluster_labels = clusterer.fit_predict(avg_props[list(translation.keys())])
#    for i, lbl in enumerate(cluster_labels):
#        avg_props.ix[i, 'cluster'] = lbl
#
#    for i, centroid in enumerate(clusterer.cluster_centers_):
#        print()
#        print("Cluster: {}".format(i))
#        print(centroid)
#
#        group = avg_props[avg_props['cluster'] == i]
#        for idx, row in group.iterrows():
#            print(" -> {}: {}".format(row.name, row['group']))
#
#    print("Silhouette: {}".format(
#        metrics.silhouette_score(avg_props[list(translation.keys())],
#                                 avg_props['cluster'])))
#
#    print("CH Score: {}".format(
#        metrics.calinski_harabaz_score(avg_props[list(translation.keys())],
#                                       avg_props['cluster'])))
#
#    print("Adjusted MI: {}".format(
#        metrics.adjusted_mutual_info_score(avg_props['group'],
#                                           avg_props['cluster'])))
#
#    print("Adjusted Rand: {}".format(
#        metrics.adjusted_rand_score(avg_props['group'], avg_props['cluster'])))
#
#    print("Completeness: {}".format(
#        metrics.completeness_score(avg_props['group'], avg_props['cluster'])))
#
#    print("FM score: {}".format(
#        metrics.fowlkes_mallows_score(avg_props['group'],
#                                      avg_props['cluster'])))
