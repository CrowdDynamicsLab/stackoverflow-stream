# StackExchange HMM Clustering

This repo will contain code for performing clustering of "clickstream" data
extracted from StackExchange data dumps.

## Requirements
- A relatively new C++ compiler with at least C++14 support
- libarchive
- liblzma
- libxml2
- CMake >= 3.2.0
- [MeTA][meta] should be compiled somewhere (use the develop branch). CMake
  should detect it for you without having to install it. You will need to
  build with liblzma support for `meta::io::xz{i,o}fstream` support.

## `repack` tool
The `repack` executable is designed to get the StackExchange data dumps
into a format that is more amenable to processing using standard Unix
utilities while still being (almost) as compressed as the original data.
The repacked format will be separate `.xz` compressed files in a folder
named by the community.

It is designed to be run in a working directory that contains all of the
StackExchange `.7z` files. It will "repack" the files into the folder
`repacked/$community-name`, where `$community-name` is something like
"3dprinting.stackexchange.com". You can then move this wherever you want.

StackOverflow itself is a special little snowflake since they decided to
split its archive up into separate files. You'll have to do some post
cleanup for that one community (not really worth adding as a special case
to `repack`).

## `extract-sequences` tool

The `extract-sequences` tool is designed to extract action sequences from a
repacked StackExchange community dump. It currently extracts the following
action types (listed below with their id):

1. post question
2. post answer
3. comment
4. edit title (`PostHistoryTypeId`s 4 and 7)
5. edit body (`PostHistoryTypeId`s 5 and 8)
6. edit tags (`PostHistoryTypeId`s 6 and 9)
7. mod vote (`PostHistoryTypeId`s 10-13)
8. mod action (`PostHistoryTypeId`s 14-22)

Every user is associated with their list of actions, which is then sorted
by timestamp. These are then further decomposed into "sessions" by grouping
all consecutive actions that have a gap of less than 6 hours between them.

The output is written to `sequences.bin` in the current working directory.

## `cluster-sequences` tool

The `cluster-sequences` tool runs the actual two-layer hidden Markov model
on the extracted sequences from the previous step. It is invoked with the
path to `sequences.bin` and a number of states as its two command line
arguments and writes out the HMM model file to `hmm-model.bin` in the
current working directory.

## `print-hmm` and `plot_X.py` tools

First, install the required libraries for the Python code (see
`scripts/requirements.txt`). Then, plots of the distributions learned by
the two-layer hidden Markov model can be created by using

```bash
../build/print-hmm json ../build/hmm-model.bin | python plot_models.py
```

and

```bash
../build/print-hmm json-trans ../build/hmm-model.bin | python plot_trans.py
```

These two scripts will create files `stateX.png` and `trans.png` in the
current working directory. Each of the `stateX.png` files reflects the
observation distribution (in the form of a Markov model) for that latent
state id. Node sizes are determined based on their personalized PageRank
score (with the personalization vector equal to the initial probability
according to the Markov model), and edge widths leaving a node are
determined based on the conditional probability of taking that edge given
that a random walk is currently at that node.

Similarly, the `trans.png` file depicts the transition matrix between each
of the latent states. Node sizes and edge widths are set using the same
strategy detailed above.

[meta]: https://github.com/meta-toolkit/meta
