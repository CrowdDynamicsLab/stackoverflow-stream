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
(BEING WRITTEN)

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

[meta]: https://github.com/meta-toolkit/meta
