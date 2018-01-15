import os
import sys

import numpy as np
import pandas as pd
import statsmodels.tsa.stattools as stattools
from statsmodels.tsa.api import VAR

translation = {
    1: 'eager asker',
    2: 'editor/moderator',
    3: 'clear asker',
    4: 'answerer',
    5: 'clarifier',
}

health_metrics = {
  'answers-per-question': lambda df: df['num_answers'] / (df['num_questions'] + 1),
  'percent-accepted-ans': lambda df: df['num_with_acc_ans'] / (df['num_questions'] + 1),
  'percent-answered':     lambda df: (df['num_questions'] - df['num_unanswered']) / (df['num_questions'] + 1),
  'avg-response-time':    lambda df: df['avg_response_time']
}

health_metric = health_metrics[sys.argv[1]]
frame = pd.DataFrame(columns=list(translation.keys()))

for filename in sys.argv[2:]:
    basename = os.path.basename(filename)
    x = int(basename.split('-')[1]) + 1
    community_name = basename.split('-')[0]

    props_df = pd.read_csv(filename)
    if len(props_df) == 0:
        props_df = props_df.append({'topic': 1, 'probability': 0},
                ignore_index=True)
    props_df['network'] = community_name
    props_df = props_df.pivot(index='network', columns='topic', values='probability')
    frame = pd.concat([frame, props_df]).fillna(0)

filtered = frame.groupby(level=0).filter(lambda x: len(x) >= 30)

for group in filtered.groupby(level=0).indices.items():
    health_frame = pd.read_csv("../../build/health/{}-health.csv".format(group[0]))
    metric = health_metric(health_frame)

    for index, health in metric.iteritems():
        filtered.ix[index, 'health'] = health

filtered['askers'] = filtered[1] + filtered[3]
filtered['ask per ans'] = filtered['askers'] / (filtered[4] + 1)
filtered = filtered[20:]
data = np.log(filtered[list(translation.keys()) + ['health', 'askers', 'ask per ans']]).diff().dropna()
data.index = pd.Index(np.arange(len(data)))
data.index = pd.to_datetime(data.index, unit='D')
model = VAR(data)
results = model.fit(maxlags=5, ic='aic', verbose=True)
results.test_causality('health', list(translation.keys()) + ['askers',
    'ask per ans'])

"""
for health_label, health_metric in health_metrics.items():
    print("\n========== {} ==========\n".format(health_label))
    health_ts = health_metric(healths)
    health_diff_ts = np.log(health_ts).diff()

    for idx, values in dset.items():
        label = translation[idx]
        print("{}: ".format(label))
        prop_diff_ts = np.log(pd.Series(values)).diff()
        model = VAR(np.column_stack((health_diff_ts[20:],
            prop_diff_ts[20:])))
        results = model.fit(maxlags=6, ic='bic')
        print(results.summary())
        print("Best lag according to BIC: {}".format(results.k_ar))

        print("\n=================\n")
        print("Does {} granger-cause {}?".format(label, health_label))
        stattools.grangercausalitytests(
            np.column_stack((health_diff_ts[20:], prop_diff_ts[20:])),
            results.k_ar)

        print("\n=================\n")
        print("Does {} granger-cause {}?".format(health_label, label))
        stattools.grangercausalitytests(
            np.column_stack((prop_diff_ts[20:], health_diff_ts[20:])),
            results.k_ar)

    asker_prop_ts = pd.Series(dset[4])
    answerer_prop_ts = pd.Series(np.array(dset[1]) + np.array(dset[2]))
    ratio_ts = asker_prop_ts / answerer_prop_ts
    ratio_diff_ts = np.log(ratio_ts).diff()

    model = VAR(np.column_stack((health_diff_ts[20:], ratio_diff_ts[20:])))
    results = model.fit(maxlags=6, ic='bic')
    print(results.summary())
    print("Best lag according to BIC: {}".format(results.k_ar))

    print("\n=================\n")
    print("Does asker/answerer ratio granger-cause {}?".format(health_label))
    stattools.grangercausalitytests(
        np.column_stack((health_diff_ts[20:], ratio_diff_ts[20:])),
        results.k_ar)

    print("\n=================\n")
    print("Does {} granger-cause asker/answerer ratio?".format(health_label))
    stattools.grangercausalitytests(
        np.column_stack((ratio_diff_ts[20:], health_diff_ts[20:])),
        results.k_ar)
"""
