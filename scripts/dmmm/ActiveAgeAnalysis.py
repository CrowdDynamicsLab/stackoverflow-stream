import numpy as np
import csv
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import axes3d, Axes3D



data_file = open('Monthly_User_Activity.csv')
csv_data = csv.reader(data_file)
first_row = next(csv_data)
current_site = first_row[0]

all_sites = {}
site_info = []
for rows in csv_data:
	if rows[0] == current_site:
		site_info.append(rows[1:3])
	else:
		#break
		all_sites[current_site] = site_info
		site_info = []
		site_info.append(rows[1:3])
		current_site = rows[0]
all_sites[current_site] = site_info

site_cnt = 0
site_cnt_total = len(all_sites.keys())
for site in all_sites.keys():
	site_cnt += 1
	site_dic = {}
	for ele in all_sites[site]:
		if ele[0] not in site_dic.keys():
			site_dic[ele[0]] = []
			site_dic[ele[0]].append(ele[1])
		else:
			site_dic[ele[0]].append(ele[1])

	#print("This is user information for this site.")
	#print(site_dic)
	#print("The total number of users is:")
	#print(len(academia_dic))



### Make graphs about active age of each group of user ###
	active_stats = {}
	for usr in site_dic.keys():
		active_months = len(site_dic[usr])
		if active_months not in active_stats.keys():
			active_stats[active_months] = 1
		else:
			active_stats[active_months] += 1

	#print(active_stats)
	max_months = max(active_stats.keys())
	#print(max_months)
	cnt = 1
	x = []
	y = []
	while cnt <= max_months:
		if cnt in active_stats.keys():
			x.append(cnt)
			y.append(active_stats[cnt])
			#print(str(cnt)+" : "+str(active_stats[cnt]))
		cnt += 1

	print("("+str(site_cnt)+"/"+str(site_cnt_total)+") "+"Processing site "+site+" ...")
	plt.figure(figsize=(7, 7))
	plt.plot(x, y, 'o', label='data')
	plt.title('Active Age Distribution')
	plt.legend(loc=0)
	plt.xlabel('Active age (Months)')
	plt.ylabel('User number (Persons)')
	plt.draw()
	plt.savefig(site+"_Active_Distribution")
	plt.close()

### Make dictionaries about valid months distribution ###

	x = []
	y = []
	z = []
	alive_list = []

	for dead_month in range(0, 13):

		trimmed_site_dic = {}
		for usr in site_dic.keys():
			trimmed_usr = []
			cur_mon = int(site_dic[usr][0])
			for ele in site_dic[usr]:
				if int(ele) - cur_mon <= dead_month:
					trimmed_usr.append(ele)
					cur_mon = int(ele)
				else:
					break
			trimmed_site_dic[usr] = trimmed_usr
		#print(trimmed_site_dic)

		alive_cnt = 0
		inactive_stats = {}
		for usr in site_dic.keys():
			active_frac = float(len(trimmed_site_dic[usr]))/float(len(site_dic[usr]))
			active_frac = int(active_frac*100)
			if active_frac == 100:
				alive_cnt += 1
			if active_frac not in inactive_stats.keys():
				inactive_stats[active_frac] = 1
			else:
				inactive_stats[active_frac] += 1
		alive_list.append(alive_cnt)


		max_frac = max(inactive_stats.keys())
		cnt = 0
		z_temp = []
		while cnt <= 100:
			if cnt in inactive_stats.keys():
				z_temp.append(inactive_stats[cnt])
			else:
				z_temp.append(0)
			cnt += 1
		z.append(z_temp)
	x = np.arange(0, 101, 1)
	y = np.arange(0, 13, 1)
	x, y = np.meshgrid(x, y)
	fig = plt.figure(figsize=(7, 7))
	ax = Axes3D(fig)
	ax.plot_wireframe(x, y, z, rstride=1, cstride=5)
	plt.title('Alive Rate Distribution')
	#plt.legend(loc=0)
	ax.set_xlabel('Alive proportions (%)')
	ax.set_ylabel('Dead Month (Months)')
	ax.set_zlabel('Users (Persons)')
	plt.draw()
	plt.savefig(site+"_Active_Proportions")
	plt.close()

### Plot number of alive users ###
	alive_month = np.arange(0, 13, 1)
	plt.figure(figsize=(7, 7))
	plt.scatter(alive_month, alive_list)
	plt.plot(alive_month, alive_list)
	plt.title('Number of Alive Users')
	#plt.legend(loc=0)
	plt.xlabel('Dead Month (Months)')
	plt.ylabel('User Alive (Persons)')
	plt.draw()
	plt.savefig(site+"_Alive")
	plt.close()







