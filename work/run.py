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
	else:
		return "numactl -i 0,1 "

def hybridline(cmd,n):
	if n <= 8:
	    return "mpirun -np 1 "+ cmd +" --hpx:numa-sensitive -t {0}".format(n)
	elif n <= 16:
		return "mpirun -np 2 "+ cmd +" --hpx:bind=balanced --hpx:cores=8 --hpx:numa-sensitive -t {0}".format(n/2)
	else:
		return "mpirun -np 2 "+ cmd +" --hpx:bind=balanced --hpx:cores=8 --hpx:numa-sensitive -t {0}".format(n/2)


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

#function to plot scalability graphs
def plot_scalability_3(apps, title,  xlabel, xlabel_range, ylabel, ylabel_range, zlabel, zlabel_range, dump_file_format, unit):
	for app in apps:
		for i in range(0, len(ylabel_range)):
			datafile_n = "perfs/{0}/{0}_{1}={2}_{3}={4}_unit={6}_scale_over_{5}.dat".format(app, ylabel, ylabel_range[i], zlabel, zlabel_range[i], xlabel, unit)
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
					total_time = parse_file(dumpfile, "performances", unit)
				except IOError:
					total_time = 0
				datafile.write(" {0}".format(total_time))
				datafile.write("\n")
	        datafile.close();

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
					#get value
					dumpfile = open(dumpfile_n)
					total_time = parse_file(dumpfile, "performances", unit)
				except IOError:
					total_time = 0
				datafile.write(" {0}".format(total_time))
				datafile.write("\n")
	        datafile.close();

def plot_scalability_1(apps, title,  xlabel, xlabel_range, dump_file_format, unit):
	for app in apps:
		datafile_n = "perfs/{0}/{0}_unit={2}_scale_over_{1}.dat".format(app, xlabel, unit)
		datafile = open(datafile_n, 'w')
		for x in xlabel_range:
			datafile.write("{0}".format(x))
			value_v = [];
			total_time = 0;
			try:
				dumpfile_n = dump_file_format.replace("#0", x)
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

matrix_order = ['10000', '14000', '18000', '22000']
partition_order = ['500', '700', '900', '1100']


#create stream dump directory
dumpdir = 'perfs/stream'
if not os.path.exists(dumpdir):
 	print "creating "+dumpdir+" directory"
 	os.makedirs(dumpdir)

# stream test
for n in ncores:
	dumpfile = dumpdir+"/stream_ncores={0}.dump".format(n)
	cmdline = "./bin/stream_test "+ n +" 20 100000000 0 > {0}".format(dumpfile)
	os.system(cmdline)

plot_scalability_1(["stream"], "Scalability of stream"
	      		  , "ncores", ncores
			      , "ncores=#0"
	              , 'GBs'
			      )

#create hpx dump directory
dumpdir = 'perfs/pvector_view_transpose'
if not os.path.exists(dumpdir):
 	print "creating "+dumpdir+" directory"
 	os.makedirs(dumpdir)

#hpx actions version
for n in ncores:
	for i in range(0, len(matrix_order)):
		dumpfile = dumpdir+"/pvector_view_transpose_matrix_order={0}_partition_order={1}_ncores={2}.dump".format(matrix_order[i], partition_order[i], n)

		cmdline = "mpirun -np "+n+" ./bin/pvector_view_transpose_test -t 1 --matrix_order {0} --partition_order {1} > {2}".format(
		  matrix_order[i]
		, partition_order[i]
		, dumpfile
		)
		print cmdline
		os.system(cmdline)

#create hpx dump directory
dumpdir = 'perfs/pvector_view_transpose_with_tasks'
if not os.path.exists(dumpdir):
	print "creating "+dumpdir+" directory"
	os.makedirs(dumpdir)


#hpx tasks version
for n in ncores:
	for i in range(0, len(matrix_order)):
		dumpfile = dumpdir+"/pvector_view_transpose_with_tasks_matrix_order={0}_partition_order={1}_ncores={2}.dump".format(matrix_order[i], partition_order[i], n)

		cmdline = numaline(int(n)) +"./bin/pvector_view_transpose_with_tasks_test  --hpx:bind=balanced  -t "+n+" --matrix_order {0} --partition_order {1} > {2}".format(
		  matrix_order[i]
		, partition_order[i]
		, dumpfile
		)
		print cmdline
		os.system(cmdline)

plot_scalability_3(["pvector_view_transpose","pvector_view_transpose_with_tasks"], "Scalability of transpose with co-array"
	    , "ncores", ncores
			, "matrix_order", matrix_order
			, "partition_order", partition_order
			, "matrix_order=#1_partition_order=#2_ncores=#0"
	    , 'GBs'
			)

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

		cmdline = numaline(int(n)) +"./bin/hpx_threaded_spmv_test --hpx:bind=balanced -t "+ n +" --filename {0}/pvector_view/tests/performance/spmv/ExampleMatrices/{1}.mtx --grain_factor 2 > {2}".format(
		  path
		, problem[i]
		, dumpfile
		)
		print cmdline
		os.system(cmdline)

#create hpx hybrid dump directory
dumpdir = 'perfs/pvector_view_spmv_with_tasks'
if not os.path.exists(dumpdir):
	print "creating "+dumpdir+" directory"
	os.makedirs(dumpdir)

#hpx hybrid version
for n in ncores:
	for i in range(0, len(problem)):
		dumpfile = dumpdir+"/pvector_view_spmv_with_tasks_problem={0}_ncores={1}.dump".format(problem[i], n)

		cmdline = hybridline("./bin/pvector_view_spmv_with_tasks_test",int(n)) + " --filename {0}/pvector_view/tests/performance/spmv/ExampleMatrices/{1}.mtx --grain_factor 2 > {2}".format(
		  path
		, problem[i]
		, dumpfile
		)
		print cmdline
		os.system(cmdline)


plot_scalability_2(["pvector_view_spmv","hpx_threaded_spmv","pvector_view_spmv_with_tasks"], "Scalability of spmv with co-array"
		            , "ncores", ncores
					, "problem", problem
					, "problem=#1_ncores=#0"
					, 'GBs'
					)
plot_scalability_2(["pvector_view_spmv","hpx_threaded_spmv","pvector_view_spmv_with_tasks"], "Scalability of spmv with co-array"
		            , "ncores", ncores
					, "problem", problem
					, "problem=#1_ncores=#0"
					, 'GFlops'
					)
