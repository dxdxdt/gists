#!/bin/env python3
import hashlib
import random
import string
import sys
import os
import getopt
import socket

ARGV0 = 'holepunch-tcp'
target_addr = ( "", 0 )
af = socket.AF_UNSPEC
flags = {
	'4': False,
	'6': False,
	'h': False,
}
tstun_host = None
tstun_port = 3477
key = None

def prefixed_err (msg: str):
	sys.stderr.write(ARGV0 + ': ' + msg + os.linesep)

def die (ec: int, msg: str):
	prefixed_err(msg)
	sys.exit(ec)

def my_getsockname (s: socket.socket) -> tuple:
	sa = list(s.getsockname())
	if s.family == socket.AF_INET6 and hasattr(socket, 'IPV6_V6ONLY'):
		v = s.getsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY)

		if sa[0] == '::' and not v:
			sa[0] = ''

	return tuple(sa)

def parse_args ():
	global target_addr, af, flags, tstun_host, tstun_port, key

	argv = sys.argv[1:]
	if len(argv) == 0:
		die(2, 'Use -h option for help')

	short_opts = 'T:P:H:' + ''.join(list(flags.keys()))
	opts, args = getopt.getopt(argv, short_opts)

	for p in opts:
		k = p[0][1:]

		if k in flags:
			flags[k] = True
		elif k == 'T':
			tstun_host = p[1]
		elif k == 'P':
			tstun_port = int(p[1])
		elif k == 'H':
			target_addr = ( p[1], 0 )
		else:
			raise KeyError(p)

	if flags['h']:
		return

	if flags['4'] and flags['6']:
		die(2, '-4 and -6 options are mutually exclusive')
	if not (flags['4'] or flags['6']):
		die(2, '-4 or -6 option required')

	if len(args) < 1:
		die(2, 'KEY required')
	elif len(args) > 1:
		die(2, 'Too many args')
	key = hashlib.sha256(args[0].encode()).digest()

	if flags['4']: af = socket.AF_INET
	if flags['6']: af = socket.AF_INET6

def do_tstun (s: socket.socket) -> tuple[str, int]:
	global tstun_host, tstun_port, key
	BUFSIZE = 46 + 1 + 5 # INET6_ADDRSTRLEN(46) + ' '(1) + port(5)

	sa = (tstun_host, tstun_port,)
	prefixed_err('Doing TSTUN via ' + str(sa) + ' ...')

	s.connect(sa)

	s.send(key)
	s.shutdown(socket.SHUT_WR)

	msg = str(s.recv(BUFSIZE), 'ascii')

	ret = msg.split()[:2]
	return (ret[0], int(ret[1]),)

def set_common_sockopt (s: socket.socket):
	s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

def scrub_unprint (data: bytes) -> str:
	allowed = set(bytes(string.printable, 'ascii'))
	return str(bytes(b for b in data if b in allowed), 'ascii')

def run_peer_mode ():
	global target_addr
	global af
	global flags

	with (
			socket.socket(af, socket.SOCK_STREAM, socket.IPPROTO_TCP) as s_main,
			socket.socket(af, socket.SOCK_STREAM, socket.IPPROTO_TCP) as s_tstun):
		set_common_sockopt(s_main)
		set_common_sockopt(s_tstun)

		s_main.bind(target_addr)
		bound = my_getsockname(s_main)
		s_tstun.bind(bound)
		prefixed_err('Server socket bound to: ' + str(bound))

		peer = do_tstun(s_tstun)
		prefixed_err('TSTUN result: ' + str(peer))

		s_main.connect(peer)
		r = str(random.random())
		prefixed_err('Connected! rnd: ' + r)

		s_main.send(('Hello, there! ' + r).encode('ascii'))
		s_main.shutdown(socket.SHUT_WR)

		msg_raw = s_main.recv(255)
		prefixed_err('The other peer says:')
		print(scrub_unprint(msg_raw))

def print_help (out = sys.stdout):
	print(
'''Usage: %s <-46> [-T TSTUN_HOST] [-P TSTUN_PORT] [-H BIND_ADDR] [-h] KEY
KEY: user-supplied paring key for NAT traversal via "T-STUN" server (basically
     a password or session key)
Options:
  -h             print this message and exit normally
  -T TSTUN_HOST  T-STUN server host
  -P TSTUN_PORT  T-STUN server port
  -H BIND_ADDR   bind to local address
''' % (ARGV0,))

parse_args()

if flags['h']:
	print_help()
	sys.exit(0)

if tstun_host is None:
	die(2, 'TSTUN_HOST env not set')

run_peer_mode()
