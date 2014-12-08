#!/usr/bin/python3

import sys,os,subprocess

###############################################################################
## Config
###############################################################################

inputDir = "n/a"
statsFile = "thread_adj_motivation.csv"
graphFile = "thread_adj_motivation.png"
numProcs = -1

###############################################################################
## Helpers
###############################################################################

def printHelp():
	print("parse_thread_adj_motivation.py - parse NUMA motivational results & generate heat map")
	print()
	print("Usage: ./parse_thread_adj_motivation.py <input directory> [ OPTIONS ]")
	print("Options:")
	print("\t-h / --help : print help & exit")
	print("\t-o <file>   : stats file (please use .csv suffix), default is " + statsFile)
	print("\t-g <file>   : graph file (please use .png suffix), default is " + graphFile)
	print("\t-n <num>    : number of processors in the machine used to conduct the experiments")
	sys.exit(0)

def parseArgs(args):
	global inputDir
	global statsFile
	global graphFile
	global numProcs
	skip = True
	for i in range(len(args)):
		if skip is True:
			skip = False
			continue
		elif args[i] == "-h" or args[i] == "--help":
			printHelp()
		elif args[i] == "-o":
			statsFile = args[i+1]
			skip = True
		elif args[i] == "-g":
			graphFile = args[i+1]
			skip = True
		elif args[i] == "-n":
			numProcs = int(args[i+1])
			skip = True
		else:
			inputDir = args[i]
	if inputDir == "n/a":
		print("Please specify an input directory!")
		printHelp()
	if numProcs == -1:
		print("Please specify the number of processrs!")
		printHelp()

def initialize():
	parseArgs(sys.argv)

###############################################################################
## Parsing & saving
###############################################################################

def parseDir(inDir):
	benches = {}
	for f in os.listdir(inDir):
		if "lu" in f:
			continue
		else:
			benchName = f.split("-")[0]
			if benchName not in benches.keys():
				benches[benchName] = Benchmark(benchName)
			benches[benchName].addResult(inDir, f)
	return benches

def saveResults(benches, outF):
	statsFile = open(outF, "w")
	for bench in sorted(benches.keys(), reverse=True):
		stats = benches[bench].getStats()
		print(bench + ": " + str(len(stats)))
		statLine = ""
		for otherBench in sorted(stats.keys()):
			statLine += str(stats[otherBench]) + ","
		statLine = statLine[0:len(statLine)-1] + "\n"
		statsFile.write(statLine)
	statsFile.close()

def generateHeatMap(statsFile, graphFile):
	retcode = subprocess.call(["gnuplot", \
														 "-e", "infile='" + statsFile + "'", \
														 "-e", "outfile='" + graphFile + "'", \
														 "plot_thread_adj_motivation.gp"])
	if retcode != 0:
		print("could not generate heat map!")

###############################################################################
## Classes
###############################################################################

class Benchmark:
	def __init__(self, name):
		global numProcs
		self.name = name
		self.results = {}
		self.maxThreads = str(int(numProcs))
		self.halfThreads = str(int(int(numProcs) / 2))

	def addResult(self, logDir, logFile):
		def parseFile(logFile):
			parts = logFile[logFile.rfind("/")+1:].split("-")
			infile = open(logFile, "r")
			for line in infile:
				if "Time in seconds" in line:
					time = float(line.split()[4])
			infile.close()
			assert "time" in locals()
			return (parts[1], parts[2], time)

		otherBench, numThreads, time = parseFile(os.path.join(logDir, logFile))
		if otherBench not in self.results.keys():
			self.results[otherBench] = {}
		if numThreads not in self.results[otherBench].keys():
			self.results[otherBench][numThreads] = []
		self.results[otherBench][numThreads].append(time)

	def getStats(self):
		baseline = { key:0 for key in self.results.keys() }
		stats = { key:0 for key in self.results.keys() }
		for bench in self.results.keys():
			for time in self.results[bench][self.maxThreads]:
				baseline[bench] += time
			baseline[bench] /= len(self.results[bench][self.maxThreads])

		# For now, we only care about corunning applications with # procs / 2
		for bench in self.results.keys():
			for time in self.results[bench][self.halfThreads]:
				stats[bench] += time
			stats[bench] /= len(self.results[bench][self.halfThreads])
			stats[bench] = baseline[bench] / stats[bench]

		if self.name == "dc.W.x":
			stats["dc.W.x"] = 0

		return stats

###############################################################################
## Driver
###############################################################################

initialize()
benches = parseDir(inputDir)
saveResults(benches, statsFile)
generateHeatMap(statsFile, graphFile)

