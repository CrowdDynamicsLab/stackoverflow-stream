import numpy as np
from os import listdir
from sklearn.cluster import KMeans
from sklearn.metrics import silhouette_score
import csv
import matplotlib.pyplot as plt


filenames = listdir(".")
filenames = [filename for filename in filenames if filename.endswith(".csv")]

#print(filenames)
#print(len(filenames))
X = []
for filename in filenames:
	data_file = open(filename)
	csv_data = csv.reader(data_file)
	x = []
	flag = 0
	for rows in csv_data:
		if flag == 0:
			flag = 1
			continue
		else:
			x.append(rows[1])
	X.append(x)
#print(X)


### TEST FIELD FOR BEST NUMBER OF CLUSTERS BY CALCULATING SILHOUETTE COEFFICIENT ###
"""
best_clus = 2
best_silhouette = -1
for clus_num in range(2, 10):
	clusterer = KMeans(n_clusters=clus_num, random_state=0)
	cluster_labels = clusterer.fit_predict(X)
	silhouette_avg = silhouette_score(X, cluster_labels)

	print("For n_clusters = "+str(clus_num)+", The average silhouette_score is : "+str(silhouette_avg))

	if silhouette_avg > best_silhouette:
		best_clus = clus_num
		best_silhouette = silhouette_avg

clus_num = best_clus
"""
clus_num = 2

clusterer = KMeans(n_clusters=clus_num, random_state=0)
cluster_labels = clusterer.fit_predict(X)
silhouette_avg = silhouette_score(X, cluster_labels)

print("For n_clusters = "+str(clus_num)+", The average silhouette_score is : "+str(silhouette_avg))
print("There are "+str(clus_num)+" clusters in total. They are the followings:")
for i in range(len(clusterer.cluster_centers_)):
	print()
	print("____________________NEW LINE____________________")
	print("Stack Exchange led by cluster centroid")
	print(clusterer.cluster_centers_[i])
	print()
	for j in range(len(clusterer.labels_)):
		if clusterer.labels_[j] == i:
			print(filenames[j])
	print("____________________NEW LINE____________________")
	print()

	# Draw Graphs
	objects = ('Topic 1', 'Topic 2', 'Topic 3', 'Topic 4', 'Topic 5')
	y_pos = np.arange(len(objects))
	performance = clusterer.cluster_centers_[i]

	plt.figure(figsize=(7, 7))
	plt.ylim(0, 0.6)
	plt.bar(y_pos, performance, align='center', alpha=0.5)
	plt.xticks(y_pos, objects)
	plt.title('Proportions Distribution')
	plt.xlabel('Topic type')
	plt.ylabel('Proportions')
	plt.draw()
	plt.savefig("centroid_"+str(i))
	plt.close()

print("Summary:")
print("Centroids:")
print(clusterer.cluster_centers_)
print("Labels for Each Stack Exchange:")
print(clusterer.labels_)


