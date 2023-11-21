#!/bin/bash
declare nb_proc
declare nb_runs=1
declare cmdline
declare flag_help=false

## Func defs

print_help () {
	cat << EOF
Run the command in pararrel ensuring the number of sucessful exits.
Usage: $1 <OPTIONS> <CMDLINE>
Options:
  -h  print this message and exit gracefully
  -p  number of processes to spawn (required)
  -n  number of successful run to count (default: 1)
EOF
}

parse_params () {
	local name
	local delta

	while getopts "hp:n:" name
	do
		case "$name" in
		h) flag_help=true ;;
		p) let "nb_proc=$OPTARG" ;;
		n) let "nb_runs=$OPTARG" ;;
		'?') exit 2;;
		esac
	done

	let "delta = OPTIND - 1"
	shift $delta

	cmdline="$@"
}

spwan_one () {
	$cmdline &
}

main () {
	local procs
	local ec
	local good_runs=0
	local children

	# spwan initial processes
	for (( procs = 0; procs < nb_proc; procs += 1 ))
	do
		spwan_one
	done

	while true
	do
		wait -n
		ec=$?
		echo $ec

		if [ $ec -eq 0 ]; then
			let "good_runs += 1"
			if [ $good_runs -ge $nb_runs ]; then
				break
			else
				spwan_one
			fi
		elif [ $ec -ne 3 ]; then
			# error occurred or no more child left. do not continue
			break
		fi
	done

	children="$(jobs -p)"
	[ ! -z "$children" ] && kill -TERM $children 2> /dev/null > /dev/null
	while wait -n; do : ; done

	return $ec
}

## Init script
parse_params $@

## Parametre check
if $flag_help; then
	print_help
	exit 0
fi
if [ -z "$nb_proc" ]; then
	cat << EOF >&2
-p option not set. Run with -h option for help.
EOF
	exit 2
fi
if [ -z "$cmdline" ]; then
	cat << EOF >&2
CMDLINE not set. Run with -h option for help.
EOF
fi

## Main start
main
