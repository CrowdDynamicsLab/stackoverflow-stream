import csv
import os
import sys

for filename in sys.argv[1:]:
    basename = os.path.basename(filename)
    month = int(basename.split('.bin.')[1].split('.health')[0])
    community_name = basename.split('-')[0]

    csv_filename = "{}-health.csv".format(community_name)
    already_exists = os.path.exists(csv_filename)

    with open(filename, 'r') as inputfile:
        reader = csv.DictReader(inputfile)
        with open(csv_filename, 'a') as csvfile:
            fieldnames = ['month', 'num_questions', 'num_answers',
                          'num_with_acc_ans', 'num_unanswered',
                          'avg_response_time', 'stdev_response_time']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

            if not already_exists:
                writer.writeheader()

            for row in reader:
                row['month'] = month
                writer.writerow(row)
