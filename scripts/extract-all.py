import os
import sys

if len(sys.argv) != 2:
    print("Usage: {} folder".format(sys.argv[0]))
    sys.exit(1)

for root, dirs, files in os.walk(sys.argv[1]):
    for dirname in dirs:
        # ignore meta.*
        if dirname.startswith('meta.'):
            print("Skipping {}...".format(dirname))
            continue

        community_name = dirname.replace(".stackexchange.com", "").replace(".com", "")
        file_name = os.path.join("sequences", "{}-sequences.bin".format(community_name))

        if os.path.isfile(file_name):
            print("{} has already been extracted, skipping...".format(dirname))
            continue

        print("Extracting sequences from {} into sequences/{}-sequences.bin...".format(dirname, community_name))
        os.system("./extract-sequences --time-slice=1 {} {}".format(os.path.join(root, dirname), file_name))
