#!/usr/bin/python3

import sys,os,subprocess,math

###############################################################################
## Config
###############################################################################

baselineDir = "n/a"
threadAdjDir = "n/a"
numaDir = "n/a"
statsFile = "results.csv"
stdDevsFile = "standard_deviations.csv"
graphFile = "results.png"
stdDevsGraphFile = "standard_deviations.png"
justGraph = False

###############################################################################
## Helpers
###############################################################################

def printHelp():
	print("parse_results.py - parse experimental results")
	print()
	print("Usage: ./parse_results.py [ OPTIONS ]")
	print("Options:")
	print("\t-h / --help    : print help & exit")
	print("\t-b <directory> : directory containing baseline results")
	print("\t-t <directory> : directory containing dynamic thread adjustment results")
	print("\t-c <directory> : directory containing NUMA-aware dynamic thread adjustment results")
	print("\t-o <file>      : stats file (please use .csv suffix), default is " + statsFile)
	print("\t-s <file>      : standard deviations file (please use .csv suffix), default is" + stdDevsFile)
	print("\t-g <file>      : graph file (please use .png suffix), default is " + graphFile)
	print("\t-v <file>      : graph file for standard deviations (please use .png suffix), default is " + stdDevsGraphFile)
	print("\t-n             : just graph an existing file")
	sys.exit(0)

def parseArgs(args):
	global baselineDir
	global threadAdjDir
	global numaDir
	global statsFile
	global stdDevsFile
	global graphFile
	global stdDevsGraphFile
	global justGraph
	skip = True
	for i in range(len(args)):
		if skip is True:
			skip = False
			continue
		elif args[i] == "-h" or args[i] == "--help":
			printHelp()
		elif args[i] == "-b":
			baselineDir = args[i+1]
			skip = True
		elif args[i] == "-t":
			threadAdjDir = args[i+1]
			skip = True
		elif args[i] == "-c":
			numaDir = args[i+1]
			skip = True
		elif args[i] == "-o":
			statsFile = args[i+1]
			skip = True
		elif args[i] == "-s":
			stdDevsFile = args[i+1]
			skip = True
		elif args[i] == "-g":
			graphFile = args[i+1]
			skip = True
		elif args[i] == "-v":
			stdDevsGraphFile = args[i+1]
			skip = True
		elif args[i] == "-n":
			justGraph = True
	if baselineDir == "n/a" or threadAdjDir == "n/a" or numaDir == "n/a":
		print("Please specify the three directories!")
		printHelp()

def initialize():
	parseArgs(sys.argv)

###############################################################################
## Parsing & saving
###############################################################################

def parseDirs(inDirs):
	benches = {}
	for inDir in inDirs:
		for f in sorted(os.listdir(inDir)):
			if "lu" in f:
				continue
			else:
				benchName = f.split("-")[0]
				if benchName not in benches.keys():
					benches[benchName] = Benchmark(benchName)
				benches[benchName].addResult(inDir, f)
	return benches

def saveResults(benches, outF, outSDF):
	statsFile = open(outF, "w")
	stdDevsFile = open(outSDF, "w")

	header = "No workload,1 external app,3 external apps,7 external apps,No workload (NUMA),1 external app (NUMA),3 external apps (NUMA),7 external apps (NUMA),Average,Average (NUMA)\n"
	statsFile.write(header)

	header = "Baseline,Baseline w/ 1 app,Baseline w/ 3 apps,Baseline w/ 7 apps,DynThreadAdj,DynThreadAdj w/ 1 app,DynThreadAdj w/ 3 apps,DynThreadAdj w/ 7 apps,NUMA-aware,NUMA-aware w/ 1 app,NUMA-aware w/ 3 apps,NUMA-aware w/ 7 apps\n"
	stdDevsFile.write(header)

	overallAverages = [ 0 for i in range(8) ]

	for bench in sorted(benches.keys()):
		stats, stdDevs = benches[bench].getStats()
		statLine = ""
		stdDevsLine = ""

		# Dynamic thread adjustment
		curDir = benches[bench].getThreadAdjDir()
		for extWorkload in range(len(stats[curDir])):
			overallAverages[extWorkload] += stats[curDir][extWorkload]
			statLine += str(stats[curDir][extWorkload]) + ","

		# NUMA-aware dynamic thread adjustment
		curDir = benches[bench].getNumaAwareDir()
		for extWorkload in range(len(stats[curDir])):
			overallAverages[extWorkload+4] += stats[curDir][extWorkload]
			statLine += str(stats[curDir][extWorkload]) + ","

		statLine = statLine[:len(statLine)-1] + "\n"
		statsFile.write(statLine)

		# Standard deviations
		curDir = benches[bench].getBaselineDir()
		for extWorkload in range(len(stdDevs[curDir])):
			stdDevsLine += str(stdDevs[curDir][extWorkload]) + ","

		curDir = benches[bench].getThreadAdjDir()
		for extWorkload in range(len(stdDevs[curDir])):
			stdDevsLine += str(stdDevs[curDir][extWorkload]) + ","

		curDir = benches[bench].getNumaAwareDir()
		for extWorkload in range(len(stdDevs[curDir])):
			stdDevsLine += str(stdDevs[curDir][extWorkload]) + ","

		stdDevsLine = stdDevsLine[:len(stdDevsLine)-1] + "\n"
		stdDevsFile.write(stdDevsLine)

	for i in range(len(overallAverages)):
		overallAverages[i] /= 9
	statLine = ""
	for avg in overallAverages:
		statLine += str(avg) + ","

	statLine = statLine[:len(statLine)-1] + "\n"
	statsFile.write(statLine)

	statsFile.close()
	stdDevsFile.close()

def generateGraph(statsFile, stdFile, graphFile, stdGraphFile):
	retcode = subprocess.call(["gnuplot", \
														 "-e", "infile='" + statsFile + "'", \
														 "-e", "outfile='" + graphFile + "'", \
														 "plot_results.gp"])
	if retcode != 0:
		print("could not generate heat map!")

	retcode = subprocess.call(["gnuplot", \
														 "-e", "infile='" + stdFile + "'", \
														 "-e", "outfile='" + stdGraphFile + "'", \
														 "plot_std_devs.gp"])
	if retcode != 0:
		print("could not generate heat map!")

###############################################################################
## Classes
###############################################################################

class Benchmark:
	def __init__(self, name):
		self.name = name
		self.results = {}

	def addResult(self, logDir, logFile):
		def parseFile(logFile):
			parts = logFile[logFile.rfind("/")+1:].split("-")
			infile = open(logFile, "r")
			for line in infile:
				if "Time in seconds" in line:
					time = float(line.split()[4])
			infile.close()
			assert "time" in locals()
			return (parts[1], int(parts[2].split(".")[0]), time)

		if logDir not in self.results.keys():
			self.results[logDir] = {}
		extWorkload, iteration, time = parseFile(os.path.join(logDir, logFile))

		if iteration < 5: # Throw out first 5 iterations
			return
		else:
			if extWorkload not in self.results[logDir].keys():
				self.results[logDir][extWorkload] = []
			self.results[logDir][extWorkload].append(time)

	def getBaselineDir(self):
		for f in self.results.keys():
			if "external_workload_baseline" in f:
				return f
		print("Didn't find baseline!")
		return ""

	def getThreadAdjDir(self):
		for f in self.results.keys():
			if "baseline" not in f and "numa" not in f:
				return f
		return ""

	def getNumaAwareDir(self):
		for f in self.results.keys():
			if "external_workload_numa" in f:
				return f
		return ""

	def getStats(self):
		self.numExtWorkloads = 4
		baseline = [ 0 for n in range(self.numExtWorkloads) ]
		stats = {
			self.getThreadAdjDir():[ 0 for n in range(self.numExtWorkloads) ],
			self.getNumaAwareDir():[ 0 for n in range(self.numExtWorkloads) ]
		}
		stdDevs = {
			self.getBaselineDir():[ 0 for n in range(self.numExtWorkloads) ],
			self.getThreadAdjDir():[ 0 for n in range(self.numExtWorkloads) ],
			self.getNumaAwareDir():[ 0 for n in range(self.numExtWorkloads) ]
		}

		# Calculate baseline
		curDir = self.getBaselineDir()
		i = 0
		for extWorkload in sorted(self.results[curDir].keys()):
			# Average
			for time in self.results[curDir][extWorkload]:
				baseline[i] += time
			baseline[i] /= len(self.results[curDir][extWorkload])

			# Standard deviation
			for time in self.results[curDir][extWorkload]:
				stdDevs[curDir][i] += ((baseline[i] - time)**2)
			stdDevs[curDir][i] = math.sqrt(stdDevs[curDir][i] / \
																		 len(self.results[curDir][extWorkload]))
			i += 1

		# Calculate speedup with only dynamic thread adjustment
		curDir = self.getThreadAdjDir()
		i = 0
		for extWorkload in sorted(self.results[curDir].keys()):
			for time in self.results[curDir][extWorkload]:
				stats[curDir][i] += time
			stats[curDir][i] /= len(self.results[curDir][extWorkload])
			stats[curDir][i] = baseline[i] / stats[curDir][i]

			for time in self.results[curDir][extWorkload]:
				stdDevs[curDir][i] += ((stats[curDir][i] - time)**2)
			stdDevs[curDir][i] = math.sqrt(stdDevs[curDir][i] / \
																		 len(self.results[curDir][extWorkload]))
			i += 1

		# Calculate speedup with dynamic thread adjustment + NUMA-aware mapping
		curDir = self.getNumaAwareDir()
		i = 0
		for extWorkload in sorted(self.results[curDir].keys()):
			for time in self.results[curDir][extWorkload]:
				stats[curDir][i] += time
			stats[curDir][i] /= len(self.results[curDir][extWorkload])
			stats[curDir][i] = baseline[i] / stats[curDir][i]

			for time in self.results[curDir][extWorkload]:
				stdDevs[curDir][i] += ((stats[curDir][i] - time)**2)
			stdDevs[curDir][i] = math.sqrt(stdDevs[curDir][i] / \
																		 len(self.results[curDir][extWorkload]))
			i += 1

		return (stats, stdDevs)

###############################################################################
## Driver
###############################################################################

initialize()
if justGraph is False:
	benches = parseDirs([baselineDir, threadAdjDir, numaDir])
	saveResults(benches, statsFile, stdDevsFile)
generateGraph(statsFile, stdDevsFile, graphFile, stdDevsGraphFile)

