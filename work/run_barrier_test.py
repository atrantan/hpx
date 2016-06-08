#!/usr/bin/python

import os
import re
import sys

CSI="\x1B["
reset=CSI+"m"
bold='\033[1m'

#parses a file fi
def parse_file(f, value_name, unit):
	content = f.read()
	items=re.findall("{0}.*$".format(value_name), content, re.MULTILINE)
	#print content
	value_max = '0'
	for item in items:
		value_s = item.partition(':')[2]
		if value_s.partition(unit)[1] == unit:
			value_s = value_s.partition(unit)[0]
			if float(value_s) > float(value_max):
				value_max = value_s
	return float(value_max)

def valid_line(f, col):
	content = f.read()
	elems = content.split(' ')
	if(elems[col].split('\n')[0] == '0'):
		return False;
	return True;

def plot_scalability_2(apps, title,  xlabel, xlabel_range, ylabel, ylabel_range, dump_file_format, unit):
	for app in apps:
		for i in range(0, len(ylabel_range)):
			datafile_n = "perfs/{0}/{0}_{1}={2}_unit={4}_scale_over_{3}.dat".format(app, ylabel, ylabel_range[i], xlabel, unit)
			datafile = open(datafile_n, 'w')

			for x in xlabel_range:
				datafile.write("{0}".format(x))
				value_v = [];
				total_time = 0;
				try:
					dumpfile_n = dump_file_format.replace("#0", x)
					dumpfile_n = dumpfile_n.replace("#1", ylabel_range[i])
					dumpfile_n = "perfs/{0}/{0}_{1}.dump".format(app, dumpfile_n)

					# get value
					dumpfile = open(dumpfile_n)
					total_time = parse_file(dumpfile, "Time", unit)
				except IOError:
					total_time = 0
				datafile.write(" {0}".format(total_time))
				datafile.write("\n")
	        datafile.close();

barrier_policy = ["central","dissemination","pairwise"]
nlocs = ['1', '2', '4', '6', '8', '10','12','14', '16']

# create dump directory
dumpdir = 'perfs/barrier_algorithms_10'
if not os.path.exists(dumpdir):
	print "creating "+dumpdir+" directory"
	os.makedirs(dumpdir)

# run benchmarks
for n in nlocs:
	for i in range(0, len(barrier_policy)):
		dumpfile = dumpdir+"/barrier_algorithms_10_policy={0}_nlocs={1}.dump".format(barrier_policy[i], n)

		cmdline = "mpirun -np "+n+" ./bin/foreach_test -t 1 --vector_size 10 --work_delay 1000000 --barrier_policy {0} > {1}".format(
		  barrier_policy[i]
		, dumpfile
		)
		print cmdline
		os.system(cmdline)

# create dump directory
dumpdir = 'perfs/barrier_algorithms_100'
if not os.path.exists(dumpdir):
	print "creating "+dumpdir+" directory"
	os.makedirs(dumpdir)

# run benchmarks
for n in nlocs:
	for i in range(0, len(barrier_policy)):
		dumpfile = dumpdir+"/barrier_algorithms_100_policy={0}_nlocs={1}.dump".format(barrier_policy[i], n)

		cmdline = "mpirun -np "+n+" ./bin/foreach_test -t 1 --vector_size 100 --work_delay 1000000 --barrier_policy {0} > {1}".format(
		  barrier_policy[i]
		, dumpfile
		)
		print cmdline
		os.system(cmdline)

plot_scalability_2(["barrier_algorithms_10","barrier_algorithms_100"]
	                , "Overhead of barrier algorithm"
		            , "nlocs", nlocs
					, "policy", barrier_policy
					, "policy=#1_nlocs=#0"
					, 'ns'
					)
