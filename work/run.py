#!/usr/bin/python

import os
import re
import sys

path = sys.argv[1]

CSI="\x1B["
reset=CSI+"m"
bold='\033[1m'

def numaline(n):
	if n == 1:
		return ""
	elif n <= 8:
	    return "numactl -i 0 "
	elif n <= 16:
		return "numactl -i 0,1 "
	elif n <= 24:
		return "numactl -i 0,1,6 "
	elif n <= 32:
		return "numactl -i 0,1,6,7 "
	elif n <= 40:
		return "numactl -i 0,1,6,7,2 "
	else:
		return "numactl -i 0,1,6,7,2,3 "

#parses a file fi
def parse_file(f, value_name, unit):
	content = f.read()
	items=re.findall("{0}.*$".format(value_name), content, re.MULTILINE)
	#print content
	value_s = '0'
	for item in items:
		value_s = item.partition(':')[2]
		value_s = value_s.partition(unit)[0]
	return float(value_s)

def valid_line(f, col):
	content = f.read()
	elems = content.split(' ')
	if(elems[col].split('\n')[0] == '0'):
		return False;
	return True;

#function to plot scalability graphs
def plot_scalability(apps, title,  xlabel, xlabel_range, ylabel, ylabel_range, zlabel, zlabel_range, dump_file_format, unit):
	for app in apps:
		for i in range(0, len(ylabel_range)):
			datafile_n = "perfs/{0}/{0}_{1}={2}_{3}={4}_scale_over_{5}.dat".format(app, ylabel, ylabel_range[i], zlabel, zlabel_range[i], xlabel)
			datafile = open(datafile_n, 'w')
			for x in xlabel_range:
				datafile.write("{0}".format(x))
				value_v = [];
				total_time = 0;
				try:
					dumpfile_n = dump_file_format.replace("#0", x)
					dumpfile_n = dumpfile_n.replace("#1", ylabel_range[i])
					dumpfile_n = dumpfile_n.replace("#2", zlabel_range[i])
					dumpfile_n = "perfs/{0}/{0}_{1}.dump".format(app, dumpfile_n)
					#get value
					dumpfile = open(dumpfile_n)
					total_time = parse_file(dumpfile, "performances")
				except IOError:
					total_time = 0
				datafile.write(" {0}".format(total_time))
				datafile.write("\n")
	        datafile.close();

def plot_scalability(apps, title,  xlabel, xlabel_range, ylabel, ylabel_range, dump_file_format, unit):
	for app in apps:
		for i in range(0, len(ylabel_range)):
			datafile_n = "perfs/{0}/{0}_{1}={2}_scale_over_{3}.dat".format(app, ylabel, ylabel_range[i], xlabel)
			datafile = open(datafile_n, 'w')
			for x in xlabel_range:
				datafile.write("{0}".format(x))
				value_v = [];
				total_time = 0;
				try:
					dumpfile_n = dump_file_format.replace("#0", x)
					dumpfile_n = dumpfile_n.replace("#1", ylabel_range[i])
					dumpfile_n = "perfs/{0}/{0}_{1}.dump".format(app, dumpfile_n)
					#get value
					dumpfile = open(dumpfile_n)
					total_time = parse_file(dumpfile, "performances", unit)
				except IOError:
					total_time = 0
				datafile.write(" {0}".format(total_time))
				datafile.write("\n")
	        datafile.close();


problem = ["s3dkt3m2","memplus"]
ncores = ['1', '2', '4', '6', '8', '10','12','14', '16']

# matrix_order = ['10000', '14000', '18000', '22000']
# partition_order = ['500', '700', '900', '1100']

# #create hpx dump directory
# dumpdir = 'perfs/pvector_view_transpose'
# if not os.path.exists(dumpdir):
#  	print "creating "+dumpdir+" directory"
#  	os.makedirs(dumpdir)

# #hpx actions version
# for n in ncores:
# 	for i in range(0, len(matrix_order)):
# 		dumpfile = dumpdir+"/pvector_view_transpose_matrix_order={0}_partition_order={1}_ncores={2}.dump".format(matrix_order[i], partition_order[i], n)

# 		cmdline = "mpirun -np "+n+" ./bin/pvector_view_transpose_test -t 1 --matrix_order {0} --partition_order {1} > {2}".format(
# 		  matrix_order[i]
# 		, partition_order[i]
# 		, dumpfile
# 		)
# 		print cmdline
# 		os.system(cmdline)

# #create hpx dump directory
# dumpdir = 'perfs/pvector_view_transpose_with_tasks'
# if not os.path.exists(dumpdir):
# 	print "creating "+dumpdir+" directory"
# 	os.makedirs(dumpdir)


# #hpx tasks version
# for n in ncores:
# 	for i in range(0, len(matrix_order)):
# 		dumpfile = dumpdir+"/pvector_view_transpose_with_tasks_matrix_order={0}_partition_order={1}_ncores={2}.dump".format(matrix_order[i], partition_order[i], n)

# 		cmdline = numaline(int(n)) +"./bin/pvector_view_transpose_with_tasks_test -t "+n+" --matrix_order {0} --partition_order {1} > {2}".format(
# 		  matrix_order[i]
# 		, partition_order[i]
# 		, dumpfile
# 		)
# 		print cmdline
# 		os.system(cmdline)

# create hpx dump directory
dumpdir = 'perfs/pvector_view_spmv'
if not os.path.exists(dumpdir):
	print "creating "+dumpdir+" directory"
	os.makedirs(dumpdir)

#hpx actions version
for n in ncores:
	for i in range(0, len(problem)):
		dumpfile = dumpdir+"/pvector_view_spmv_problem={0}_ncores={1}.dump".format(problem[i], n)

		cmdline = "mpirun -np "+n+" ./bin/pvector_view_spmv_test -t 1 --filename {0}/pvector_view/tests/performance/spmv/ExampleMatrices/{1}.mtx > {2}".format(
		  path
		, problem[i]
		, dumpfile
		)
		print cmdline
		os.system(cmdline)

#create hpx for_each dump directory
dumpdir = 'perfs/hpx_threaded_spmv'
if not os.path.exists(dumpdir):
	print "creating "+dumpdir+" directory"
	os.makedirs(dumpdir)

#hpx for_each version
for n in ncores:
	for i in range(0, len(problem)):
		dumpfile = dumpdir+"/hpx_threaded_spmv_problem={0}_ncores={1}.dump".format(problem[i], n)

		cmdline = "./bin/hpx_threaded_spmv_test -t "+ n +" --filename {0}/pvector_view/tests/performance/spmv/ExampleMatrices/{1}.mtx --grain_factor 4 > {2}".format(
		  path
		, problem[i]
		, dumpfile
		)
		print cmdline
		os.system(cmdline)


plot_scalability(["pvector_view_spmv","hpx_threaded_spmv"], "Scalability of spmv with co-array"
		            , "ncores", ncores
					, "problem", problem
					, "problem=#1_ncores=#0"
					, 'GFlops'
					)
