#! /usr/bin/python
# -*- coding: utf-8-unix -*-

# åäö

#import os

from __future__ import print_function

from time import sleep
import cPickle
import sys
import re
import subprocess

def execute(cmd):
	fail = None
	stdoutdata = ""
	stderrdata = ""
	try:
		p = subprocess.Popen(cmd, shell=False, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		stdoutdata, stderrdata = p.communicate()
	except Exception, fail:
		pass
	
	if fail or p.returncode != 0:
		print("command failed: {:s}".format(cmd))
		if fail:
			print("exception: {:s}".format(fail))
		elif p.returncode != 0:
			print("return code: {:d}".format(p.returncode))

		if stderrdata != "":
			print("contents of stderr:")
			for line in stderrdata.split("\n"):
				if len(line) > 0:
					print("  {:s}".format(line))
		return None
		
	return stdoutdata

def in_quotes(text):
	eat = 0
	quoted = []
	for c in text:
		if c == '"':
			eat = 1 - eat
		else:
			if eat:
				quoted.append(c)
	if len(quoted):
		return "".join(quoted)
	else:
		return None

def process_entry(entry):
	substrings = {}
	if entry is None:
		pass
	else:
		l=re.split("[^a-z0-9A-Z]", entry)
		l2 = [ v.lower() for v in l if len(v) ]
		n = len(l2)
		sl2 = []
		for r in range(1,n+1):
			for s in range(n-r+1):
				sl2.append(l2[s:s+r])

		for s in sl2:
			substrings["".join(s)] = True
			substrings[" ".join(s)] = True

		l=re.split("[^a-zA-Z]", entry)
		l2 = [ v.lower() for v in l if len(v) ]
		n = len(l2)
		sl2 = []
		for r in range(1,n+1):
			for s in range(n-r+1):
				sl2.append(l2[s:s+r])
				
		for s in sl2:
			substrings["".join(s)] = True
			substrings[" ".join(s)] = True
			
	return substrings

def update_index(index, string, n):
	if string in index:
		index[string].append(n)
	else:
		index[string] = [ n ]

def rebuild_index():
	c64_filetypes = [ "d64" ]
	find_program     = "/usr/bin/find"
	find_dir         = "."
	find_opts        = [
		"-regextype", "posix-egrep",
		"-type", "f",
		"-iregex", ".*\.({:s})".format("|".join(c64_filetypes)),
		"-print0"
	]

	stdoutdata = execute([ find_program, find_dir ] + find_opts)

	c1541_program = "/usr/local/bin/c1541"
	c1541_command = "-dir"

	lines = stdoutdata.split("\0")
	nlines = len(lines)
	spaces = " " * 30

	item_map={}
	error_map={}
	item_entries={}
	index={}
	items = 0
	
	database = [ items, item_map, error_map, item_entries, index ]

	for l in range(nlines):
		if len(lines[l]) > 0:
			text = item = lines[l][2:]
			if len(text) > 40:
				text = "..{:s}".format(text[-38:])
			print("processing {:d}/{:d}: {:s}{:s}".format(l, nlines, text, spaces), end="\r")
			sys.stdout.flush()
			data = execute([ c1541_program, item, c1541_command ])
			item_map[items] = item
			
			if data == None:
				error_map[items] = True
			else:
				item_entries[items] = data
				for entry in data.split("\n"):
					ss = process_entry(item)
					for s in ss:
						if len(s) > 3:
							update_index(index, s, items)
					ss = process_entry(in_quotes(entry))
					for s in ss:
						if len(s) > 3:
							update_index(index, s, items)
				items += 1

	print("\npickling database...")
	FILE=open('dir64.dat', 'w')
	cPickle.dump(database, FILE)
	FILE.close()

if __name__ == "__main__":
	if len(sys.argv) > 1:
		if (sys.argv[1] == 'update'):
			rebuild_index()
			exit(0)

	print("loading database...");
	FILE=open('dir64.dat', 'r')
	database=cPickle.load(FILE)
	items, item_map, error_map, item_entries, index = database
	FILE.close()

	while 1:
		a = raw_input("query> ")
		ss = process_entry(a)

		hits={}
	
		for s in ss:
			if s in index:
				for n in index[s]:
					try:
						hits[n][1] += 1
					except:
						hits[n] = [s, 1]
	#				print("match: {:s} {:s}".format(s, index[s]))
			else:
				pass
	#				print("no match for {:s}".format(s))

		if len(hits) == 0:
			print("no match")
	
		decorated = [ [hits[n][1], hits[n][0], n] for n in hits ]
		decorated.sort()
		for s, h, n in decorated:
			item = item_map[n]
			print("--\n{:s}: \"{:s}\" ({:d})\n--".format(item, h, s))
			ie = item_entries[n].split("\n")
			for en in range(min(10, len(ie))):
				print("   {:s}".format(ie[en]))
			if len(ie) > 10:
				print("   ...")


