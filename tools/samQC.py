#! /nfs/team71/phd/js18/software/Python-2.6.4/python

import pysamhack as pysam
import sys
import getopt
import math
import numpy
import pylab
from string import maketrans

class ReportType:
	POS=1
	COUNT=2
	QUAL=3

def reverseComplement(seq):
	intab = "ACGT"
	outtab = "TGCA"
	transtab = maketrans(intab, outtab)
	t = seq.translate(transtab)
	t = t[::-1]
	return t

def readFasta(filename):
	sequence = ""
	name = ""

	infile = open(filename)
	out = dict()
	while 1:
		line = infile.readline()
		if line.startswith(">"):
			if len(sequence) > 0:
				out[name] = sequence
				sequence = ""
			name = line[1:].split()[0]
		else:
			if line == "":
				break
			sequence += line.rstrip()

	if len(sequence) > 0:
		out[name] = sequence
	return out


def getErrorPositions(data, s1, s2):
	for i, x in enumerate(zip(s1, s2)):
		if x[0] != x[1]:
			if i not in data:
				data[i] = 1
			else:
				data[i] += 1

def getErrorCounts(data, s1, s2):
	count = 0;
	for i, x in enumerate(zip(s1, s2)):
		if x[0] != x[1]:
			count += 1
	if count not in data:
		data[count] = 1
	else:
		data[count] += 1

def getErrorQual(data, s1, s2, q):
	for x in zip(s1, s2, q):
		t = [0, 0]
		if x[2] in data:
			t = data[x[2]]
		t[0] += 1
		if x[0] != x[1]:
			t[1] += 1
		data[x[2]] = t

def illuminaQual2Score(c):
	s = ord(c) - 64
	q =  10 * math.log(1 + 10 ** s / 10.0) / math.log(10)
	return q

def sangerQual2Score(c):
	q = ord(c) - 33
	return q

def score2prob(q):
	p = 1.0 / (1.0 + 10 ** (q / 10.0))
	return p

def usage():
	print 'usage: samQC.py [-p|-c|-s] [-t val] [-n num] [-d filename] [--range chr:start:stop] in.bam|in.sam ref.fasta'
	print 'Options:'
	print '    -p       Output summary of number of base calling errors per read-position'
	print '    -c       Output summary of number of base calling errors per read'
	print '    -q       Output the number of mismatches at each quality symbol'
	print '    -s       Output general summary (total reads, total error rate, etc)'
	print '    -t=VAL   Only use the first VAL bases for calculating stats'
	print '    -n=VAL   Only use the first VAL reads to calculate statistics'
	print '    -d=FILE  Plot the result and save the figure in FILE'
	print '             If FILE is "-" the figure will be displayed immediately'
	print '    --range  The region of the reference to process stats for, in format chr:start:end'

# Set defaults and parse options
reportType = ReportType.POS
bOutputSummary = False

chr = None
start = None
stop = None
numMaxReads = 10000
trim = None
drawFile = None

try:
	opts, args = getopt.gnu_getopt(sys.argv[1:], 'pcshqt:d:n:', ["range="])
except getopt.GetoptError, err:
		print str(err)
		usage()
		sys.exit(2)
	
for (oflag, oarg) in opts:
		if oflag == '-p':
			reportType = ReportType.POS
		if oflag == '-c':
			reportType = ReportType.COUNT
		if oflag == '-q':
			reportType = ReportType.QUAL
		if oflag == '-s':
			bOutputSummary = True
		if oflag == '-t':
			trim = int(oarg)
		if oflag == '-d':
			drawFile = oarg
		if oflag == '-n':
			limit = int(oarg)
		if oflag == '-h':
			usage()
			sys.exit(0)
		if oflag == '--range':
			chr,start,stop = oarg.split(":")

if len(args) == 2:
	samFilename = args[0]
	refFilename = args[1]
else:
	usage()
	sys.exit(2)

# Read in the reference sequence
reference = readFasta(refFilename)

for i,s in reference.items():
	print i, len(s)

# Load the samfile
samfile = pysam.Samfile(samFilename, "rb")

# Intialize the count
data = dict()

nb = 0
nr = 0
iter = samfile.fetch(chr, start, stop)
for alignment in iter:
	if nr >= numMaxReads:
		break

	# Get the sequence in the reference that this read aligns to
	if alignment.is_unmapped:
		continue
	ref_name = samfile.getrname(alignment.rname)
	ref_slice = reference[ref_name][alignment.pos:alignment.pos+alignment.rlen]
	seq_slice = alignment.seq
	qual_slice = alignment.qual

	if trim != None:
		ref_slice = ref_slice[0:trim]
		seq_slice = seq_slice[0:trim]
		qual_slice = qual_slice[0:trim]

	if alignment.is_reverse:
		seq_slice = reverseComplement(seq_slice)
		ref_slice = reverseComplement(ref_slice)

	if alignment.seq.find("N") == -1:
		nb += len(seq_slice)
		nr += 1
	
		if reportType == ReportType.POS:
			getErrorPositions(data, seq_slice, ref_slice)
		elif reportType == ReportType.COUNT:
			getErrorCounts(data, seq_slice, ref_slice)
		elif reportType == ReportType.QUAL:
			getErrorQual(data, seq_slice, ref_slice, qual_slice)

# Set up the output and plotting
plot_x = []
plot_y = []

if reportType == ReportType.POS or reportType == ReportType.COUNT:
	so = 0
	eo = max(data.keys())
	sum = 0
	
	plot_x = [0] * (eo + 1)
	plot_y = [0] * (eo + 1)

	for i in range(so, eo+1):
		v = data[i] if i in data else 0
		er = float(v) / float(nr)
		sum += v
		print i,v,er
		plot_x[i] = i
		plot_y[i] = er

elif reportType == ReportType.QUAL:
	# Sort the dicttion
	s_keys = data.keys()
	s_keys.sort()
	plot_x = [0] * len(s_keys)
	plot_y = [0] * len(s_keys)

	for idx,i in enumerate(s_keys):
		t = data[i]
		if t[1] > 0:
			score = sangerQual2Score(i)
			prop = float(t[1]) / float(t[0])
			prob = score2prob(score)
			lprop = math.log(prop)
			lprob = math.log(prob)
			print i, score, prob, prop, lprob, lprop
			plot_x[idx] = lprop
			plot_y[idx] = lprob
	
if drawFile != None:
	pylab.scatter(plot_x, plot_y)
	if drawFile != '-':
		pylab.savefig(drawFile)
	else:
		pylab.show()

if reportType == ReportType.POS and bOutputSummary:
	ter = float(sum) / float(nb)
	print "Total reads:", nr
	print "Total bases:", nb
	print "Total error rate:", ter

