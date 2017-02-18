# StackExchange HMM Clustering

This repo will contain code for performing clustering of "clickstream" data
extracted from StackExchange data dumps.

## Requirements
- A relatively new C++ compiler with at least C++14 support
- CMake >= 3.2.0
- [MeTA][meta] should be compiled somewhere (use the develop branch). CMake
  should detect it for you.
- libarchive
- liblzma

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
