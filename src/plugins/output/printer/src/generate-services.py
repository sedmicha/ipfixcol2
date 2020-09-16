#!/usr/bin/env python3
import re
from collections import defaultdict

def readlines(filename, pattern):
	r = re.compile(pattern)
	for l in open(filename):
		m = r.search(l)
		if m:
			yield m.groups()

def writeout(filename, data):
	with open(filename, 'w') as f:
		for a, b in data:
			f.write(f'{{ {a}, "{b}" }},\n')

services = defaultdict(list)
for name, port, proto in readlines('/etc/services', '^(\w+)\s+(\d+)/(\w+)'):
	services[port].append(name)
services = [(port, names[0]) for port, names in services.items() if len(set(names)) == 1]
writeout('services.inc', services)

protocols = []
for name, number in readlines('/etc/protocols', '^(\w+)\s+(\d+)'):
	protocols.append((number, name))
writeout('protocols.inc', protocols)
