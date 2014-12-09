#!/usr/bin/python3

import sys,os,subprocess

###############################################################################
## Config
###############################################################################

inputDir = "n/a"
statsFile = "numa_motivation.csv"
graphFile = "numa_motivation.png"
numNodes = -1

###############################################################################
## Helpers
###############################################################################

def printHelp():
	print("parse_numa_motivation.py - parse NUMA motivational results & generate heat map")
	print()
	print("Usage: ./parse_numa_motivation.py <input directory> [ OPTIONS ]")
	print("Options:")
	print("\t-h / --help : print help & exit")
	print("\t-o <file>   : stats file (please use .csv suffix), default is " + statsFile)
	print("\t-g <file>   : graph file (please use .png suffix), default is " + graphFile)
	print("\t-n <number> : number of NUMA nodes on machine used for experiments")
	sys.exit(0)

def parseArgs(args):
	global inputDir
	global statsFile
	global graphFile
	global numNodes
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
			numNodes = int(args[i+1])
			skip = True
		else:
			inputDir = args[i]
	if inputDir == "n/a":
		print("Please specify an input directory!")
		printHelp()
	if numNodes == -1:
		print("Please specify the number of NUMA nodes!")
		printHelp()

#def getNumNodes():
#	global numNodes
#	global inputDir
#	print("Found " + str(numNodes) + " NUMA node(s)")

def initialize():
	parseArgs(sys.argv)
#	getNumNodes()

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
		statLine = ""
		for i in range(len(stats)):
			if i == 0:
				continue # This column will always be white
			statLine += str(stats[i]) + ","
		statLine = statLine[0:len(statLine)-1] + "\n"
		statsFile.write(statLine)
	statsFile.close()

def generateHeatMap(statsFile, graphFile):
	global numNodes
	gpNumNodes = float(numNodes-2) + 0.5
	retcode = subprocess.call(["gnuplot", \
														 "-e", "numnodes=" + str(gpNumNodes), \
														 "-e", "infile='" + statsFile + "'", \
														 "-e", "outfile='" + graphFile + "'", \
														 "plot_numa_motivation.gp"])
	if retcode != 0:
		print("could not generate heat map!")

###############################################################################
## Classes
###############################################################################

class Benchmark:
	def __init__(self, name):
		global numNodes

		self.name = name
		self.results = []
		# TODO add these via generators
		for cpu in range(numNodes):
			self.results.append([])
			for mem in range(numNodes):
				self.results[cpu].append([])

	def addResult(self, logDir, logFile):
		def parseFile(logFile):
			parts = logFile[logFile.rfind("/")+1:].split("-")
			infile = open(logFile, "r")
			for line in infile:
				if "Time in seconds" in line:
					time = float(line.split()[4])
			infile.close()
			assert time in locals()
			return (int(parts[1]), int(parts[2]), time)

		cpuNode, memNode, time = parseFile(os.path.join(logDir, logFile))
		self.results[cpuNode][memNode].append(time)

	def getStats(self):
		global numNodes

		stats = [ [] for i in range(numNodes) ]
		baseline = 0
		for time in self.results[0][0]:
			baseline += time
		baseline /= len(self.results[0][0])

		# For now, we only care about CPU execution on node 0, otherwise there's
		# no sensible way to display all the data
		for i in range(len(self.results[0])):
			average = 0
			for time in self.results[0][i]:
				average += time
			average /= len(self.results[0][i])
			stats[i] = average / baseline

		return stats

###############################################################################
## Driver
###############################################################################

initialize()
benches = parseDir(inputDir)
saveResults(benches, statsFile)
generateHeatMap(statsFile, graphFile)

