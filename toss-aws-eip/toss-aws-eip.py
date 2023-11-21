#!/bin/env python3

# Copyright (c) 2023 David Timber <dxdt@dev.snart.me>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import getopt
import ipaddress
import math
import signal
import sys
import time
from decimal import Decimal
from enum import Enum
from typing import Callable

import boto3

VER_STR = "0.0.0 (Nov 2023)"

HELP_STR = '''Usage: {prog} [options] <SPEC> [SPEC ...]
Repeat allocating Elastic IP address until an address in desired range is
acquired. The multiple specs will be OR'd.

SPEC: <RANGE | NET>
  RANGE: <ADDR>-<ADDR>
  ADDR:  an IPv4 address
  NET:   <ADDR>/<CIDR>
  CIDR:  integer [0,32]
Options:
  -r <string>   aws region name. Used if the default is not set in profile
  -p <string>   aws profile to use. Use the default profile if unspecified
  -l <string>   set the name for the allocated EIP. Defaults to an empty string
  -x <string>   the tag spec in "name=value" format
  -n <int>      limit number of attempts. Default: -1
  -t <decimal>  limit run time in seconds. Default: 'inf'
  -d            do a dry run
  -h            print this message and exit gracefully
  -v            increase verbosity
  -q            shut up
  -V            print other info and exit gracefully

WARNING: check the pricing policy of your region prior to using this tool!'''
V_STR = '''Version: {ver}
by David Timber <dxdt@dev.snart.me> (c) 2023'''

# Global classes

class Verbosity (Enum):
	'''Verbosity Level Enum'''
	Q = ERR = 0
	DEFAULT = WARN = 1
	INFO = 2
	DBG0 = 3
	DBG1 = 4

class AddrSpec:
	'''The class works in two modes - A to B range mode and CIDR mode. `range()`
	and `net()` factory methods can be used to instantiate a working instance.
	The default constructor instantiates a unusable dummy instance.'''
	def _range_in_op (self, addr: ipaddress.IPv4Address) -> bool:
		return self.a <= addr and addr <= self.b

	def _net_in_op (self, addr: ipaddress.IPv4Address) -> bool:
		return addr in self.net

	def _range_str_op (self) -> str:
		return '''{a}-{b}'''.format(a = str(self.a), b = str(self.b))

	def _net_str_op (self) -> str:
		return str(self.net)

	def range (a: ipaddress.IPv4Address, b: ipaddress.IPv4Address):
		ret = AddrSpec()
		ret.a = a
		ret.b = b
		ret.net = None
		ret._contains_f = ret._range_in_op
		ret._str_f = ret._range_str_op
		return ret

	def net (n: ipaddress.IPv4Network):
		ret = AddrSpec()
		ret.a = ret.b = None
		ret.net = n
		ret._contains_f = ret._net_in_op
		ret._str_f = ret._net_str_op
		return ret

	def __init__(self):
		self.a = None
		self.b = None
		self.net = None

	def __contains__ (self, addr: ipaddress.IPv4Address) -> bool:
		return self._contains_f(addr)

	def __str__ (self) -> str:
		return self._str_f()

class ProgConf:
	'''The program configuration class. The members represent the parametres
	from the command line arguments.'''
	def __init__(self):
		self.range_specs = list[AddrSpec]()
		self.profile = None
		self.tag_spec = []
		self.nb_runs = math.inf
		self.runtime = math.inf
		self.verbose = Verbosity.DEFAULT.value
		self.help = False
		self.dryrun = False
		self.region = None
		self.ver = False

	def CompVerbosity (self, v: Verbosity) -> bool:
		return self.verbose >= v.value

class EIP:
	'''Class for holding the EIP and the allocation id for a successful
	iteration. The `str()` operator should return a string that can be more or
	less parsed by a YAML parser.'''
	def __init__(self, addr: ipaddress.IPv4Address, alloc_id: str):
		self.addr = addr
		self.alloc_id = alloc_id

	def __str__(self) -> str:
		return '''PublicIp: {addr}
AllocationId: {alloc_id}'''.format(
	addr = str(self.addr),
	alloc_id = self.alloc_id)

# Exceptions
class FormatError (Exception):
	'''Used for cmd line args parse error'''
	...


def ParseAddrSpec (x: str) -> AddrSpec:
	'''If the string contains a hyphen, try to extract the two addresses. If the
	string contains a slash, treat the characters before it as an IP address and
	the ones after it a CIDR length.'''
	sep = x.find("-")
	if sep > 0: # range
		a = x[:sep].strip()
		b = x[sep + 1:].strip()
		return AddrSpec.range(ipaddress.IPv4Address(a), ipaddress.IPv4Address(b))

	sep = x.find("/")
	if sep > 0: # network
		return AddrSpec.net(ipaddress.IPv4Network(x))

	raise FormatError("Invalid address spec: " + x)

def ParseParam (argv: list[str]) -> ProgConf:
	'''The cmd line args parser'''
	ret = ProgConf()
	opt, args = getopt.getopt(argv, "p:l:x:n:t:hdvqr:V")

	for v in args:
		ret.range_specs.append(ParseAddrSpec(v))

	for k, v in opt:
		match k:
			case "-r": ret.region = v
			case "-p": ret.profile = v
			case "-l": ret.tag_spec.append({ 'Key': 'Name', 'Value': v })
			case "-x":
				i = v.find("=")
				if i < 0:
					raise FormatError("Invalid format for option '-x': " + v)
				tname = v[:i]
				tvalue = v[i + 1:]
				ret.tag_spec.append({ 'Key': tname, 'Value': tvalue })
			case '-n':
				ret.nb_runs = int(v)
				if ret.nb_runs <= 0: ret.nb_runs = math.inf
			case '-t': ret.runtime = Decimal(v) * 1000000000
			case '-h': ret.help = True
			case '-v': ret.verbose += 1
			case '-q': ret.verbose = Verbosity.Q.value
			case '-d': ret.dryrun = True
			case '-V': ret.ver = True

	return ret

def GetRefClock () -> int:
	'''A wrapper function for retrieving the monotonic clock tick value used to
	measure the process run time.'''
	return time.monotonic_ns()

def DoPrint (s: str, v: Verbosity):
	'''Check verbosity level and print the string to stderr.'''
	if conf.CompVerbosity(v):
		return sys.stderr.write(s)

try:
	conf = ParseParam(sys.argv[1:])
except (FormatError, ipaddress.AddressValueError) as e:
	sys.stderr.write(str(e) + "\n")
	sys.exit(2)
run_cnt = 0 # the number of iteration performed
run_start = GetRefClock() # time the process started
it_ret = None # acquired EIP information returned after a successful iteration
flag = True # flag used to stop the main loop to serve exit signals(TERM, INT)

if conf.help:
	print(HELP_STR.format(prog = sys.argv[0]))
	sys.exit(0)
if conf.ver:
	print(V_STR.format(ver = VER_STR))
	sys.exit(0)

if not conf.range_specs:
	DoPrint("No SPEC specified. Run with '-h' option for help.\n", Verbosity.ERR)
	sys.exit(2)

# Init Boto3
session = boto3.Session(
	region_name = conf.region,
	profile_name = conf.profile)
client = session.client("ec2")

def PrintedCall (func: Callable, fname: str, v: Verbosity, **kwargs):
	DoPrint('''CALL {fname}({kwargs})\n'''.format(fname = fname, kwargs = kwargs), v)
	return func(**kwargs)

def PrintReturn (fname: str, ret, v: Verbosity):
	return DoPrint('''RET {fname}(): {ret}'''.format(fname = fname, ret = str(ret)), v)

def ShouldIterate () -> bool:
	'''Check if the main loop should continue'''
	global run_cnt, conf, flag

	run_elapsed = GetRefClock() - run_start
	ret = flag and run_cnt < conf.nb_runs and run_elapsed < conf.runtime
	run_cnt += 1

	return ret

def OptInSignalHandler (sname: str, handler: Callable):
	'''Install the signal handler if the signal with the name exists on the
	platform.'''
	if hasattr(signal, sname):
		return signal.signal(signal.Signals(sname), handler)

def HandleSignal (sn, sf):
	'''Exit signal handler'''
	global flag

	flag = False
	# Deregister the handler so that the subsequent signals kill the process
	signal.signal(sn, signal.SIG_DFL)

	# Signal names are not supported by all platforms
	try:
		signame = signal.Signals(sn).name
	except:
		signame = "?"
	DoPrint('''CAUGHT {signame}({sn})\n'''.format(
		signame = signame,
		sn = sn), Verbosity.WARN)

def ExtractBotoError (e: Exception) -> str:
	'''Not many types of Boto3 exceptions are defined for errors. Use the "duck
	typing technique" to extract the error code returned from the AWS
	endpoint.'''
	if (hasattr(e, "response") and
		type(e.response) == dict and
		type(e.response.get("Error")) == dict):
		return e.response["Error"].get("Code")

def IsDryRunError (e: Exception) -> bool:
	return ExtractBotoError(e) == "DryRunOperation"

def DoIteration () -> EIP | None:
	'''Returns the EIP in the desired range if successful. None otherwise.'''
	global conf
	# Pre-construct the tag spec.
	tag_spec = [ { 'ResourceType':  'elastic-ip' , 'Tags': conf.tag_spec } ] if conf.tag_spec else None

	try:
		# Send the allocation request!
		r = PrintedCall(
			client.allocate_address,
			"client.allocate_address",
			Verbosity.DBG0,
			TagSpecifications = tag_spec,
			DryRun = conf.dryrun)
		PrintReturn("client.allocate_address", r, Verbosity.DBG0)
		# The method will return the response object if successful
		ip = ipaddress.IPv4Address(r['PublicIp'])
		alloc_id = r['AllocationId']

		DoPrint('''Got {ip}\n'''.format(ip = ip), Verbosity.INFO)
	except Exception as e:
		if conf.dryrun and IsDryRunError(e):
			# This is expected for dry run. Carry on with mock data.
			ip = None
			alloc_id = "DryIce"
		else:
			# Propagate other errors
			# Could be bad internet connection or insufficient privileges
			raise e

	if ip:
		# Check if the address allocated is within the desired range
		for spec in conf.range_specs:
			DoPrint("IS {ip} in {range}?: ".format(
				ip = ip,
				range = spec
			), Verbosity.DBG1)
			ret = ip in spec # this calls `_net_in_op()` or `_range_in_op()`
			DoPrint("{verdict}\n".format(
				verdict = "yes" if ret else "no"), Verbosity.DBG1)
			if ret:
				# Instantiate and return the result!
				return EIP(ip, alloc_id)

	# Reached because the allocated EIP is not in the desired range
	# Release the EIP.
	try:
		r = PrintedCall(
			client.release_address,
			"client.release_address",
			Verbosity.DBG0,
			AllocationId = alloc_id,
			DryRun = conf.dryrun)
		PrintReturn("client.release_address", r, Verbosity.DBG0)
	except Exception as e:
		if conf.dryrun and IsDryRunError(e):
			# This is expected for dry run. Let the function return. It is
			# possible that the user has allocate rights but not release rights.
			# If that's the case, this is where the user will find out.
			pass
		else:
			# Propagate other errors
			# Could be bad internet connection or insufficient privileges
			raise e

# Catch these normal case signals so that the current iteration can release the
# EIP before coming out of the main loop.
OptInSignalHandler("SIGINT", HandleSignal)
OptInSignalHandler("SIGTERM", HandleSignal)
OptInSignalHandler("SIGHUP", HandleSignal) # multitoss support

while ShouldIterate():
	DoPrint('''Iteration #{nr_run} ...\n'''.format(nr_run = run_cnt), Verbosity.INFO)
	it_ret = DoIteration()
	if it_ret:
		DoPrint("Got EIP in target range!\n", Verbosity.INFO)
		print(it_ret)
		break

run_elapsed = GetRefClock() - run_start
DoPrint(
	'''Run complete after {nb_runs} run(s) in {run_elapsed:.3f}\n'''.format(
		nb_runs = run_cnt,
		run_elapsed = run_elapsed / 1000000000.0
	), Verbosity.INFO)

sys.exit(0 if it_ret else 3)
