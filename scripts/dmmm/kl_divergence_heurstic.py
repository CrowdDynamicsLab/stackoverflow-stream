import os
import pandas as pd
from scipy import stats
import sys


def load_topics(basedir):
    topics = []
    for filename in os.listdir(basedir):
        csv = pd.read_csv("{}/{}".format(basedir, filename))
        topics.append(csv['probability'])
    return topics


def max_min_kldiv(prev_topics, curr_topics):
    return min(
        max(stats.entropy(p_topic, q_topic) for p_topic in prev_topics)
        for q_topic in curr_topics)


base_topics = load_topics(sys.argv[1])

for currdir in sys.argv[2:]:
    curr_topics = load_topics(currdir)
    max_min_div = max_min_kldiv(base_topics, curr_topics)

    print("{} -> {}: {}".format(
        len(base_topics), len(curr_topics), max_min_div))
    base_topics = curr_topics
