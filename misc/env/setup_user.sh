#!/usr/bin/env bash
declare -A user2core
declare -A cmd2args
declare -A cmd2help
user2core['ubuntu']='0,0'
cmd2args['ubuntu']='0,0'
#l1d args info...
cmd2args['isca-cc-pp-l1d-recv']="(no args)"
cmd2args['isca-cc-pp-l1d-send']="(no args)"
cmd2args['isca-cc-graph-l1d-contention']="(no args but dont forget to use \e[92mpin_send\e[96m and \e[92mpin_recv\e[96m)"
cmd2args['isca-cc-measure-l1d-contention']="(no args but dont forget to use \e[92mpin_send\e[96m and \e[92mpin_recv\e[96m)"
cmd2args['isca-cc-create-l1d-contention']="(Sets to generate contention on. Dont forget to use \e[92mpin_send\e[96m and \e[92mpin_recv\e[96m)"
#fr args info...
cmd2args['isca-cc-fr-recv']="(no args required) Flags exist to interface with the binary:\n\t-f,\tspecify a shared file.\n\t-o,\tspecify a offset into the shared file\n\t-i,\tset the send bit interval rate\n"
cmd2args['isca-cc-fr-send']="(no args required) Flags exist to interface with the binary:\n\t-f,\tspecify a shared file.\n\t-o,\tspecify a offset into the shared file\n\t-i,\tset the send bit interval rate\n"
#llc args info...
cmd2args['isca-cc-pp-llc-recv']="(no args required)\n\t-r,\tspecify a cache set to contend on (default is 0).\n\t-i,\tspecify the channel bit interval (in cycles).\n\t-p,\tspecify cycles receiver spends priming the cache.\n\t-a,\tspecify cycles sender spends accessing the cache.\n\t-b,\trun channel in benchmark mode."
cmd2args['isca-cc-pp-llc-send']="(no args required)\n\t-r,\tspecify a cache set to contend on (default is 0).\n\t-i,\tspecify the channel bit interval (in cycles).\n\t-p,\tspecify cycles receiver spends priming the cache.\n\t-a,\tspecify cycles sender spends accessing the cache.\n\t-b,\trun channel in benchmark mode."
cmd2args['isca-cc-pp-llc-benchmark.py']="Please run with -h to see details."

#l1d help string
cmd2help['isca-cc-pp-l1d-recv']="Covert receiver chat client. Built using prime+probe on L1D cache (Must share core with sender!)"
cmd2help['isca-cc-pp-l1d-send']="Covert transmitter chat client. Built using prime+probe on L1D cache. (Must share core with receiver!)"
cmd2help['isca-cc-graph-l1d-contention']="Visualize the contention on the L1D.\n\tRun with \"isca-cc-create-l1d-contention\" or \"isca-cc-pp-l1d-send\" to visualize  L1D cache-contention!"
cmd2help['isca-cc-measure-l1d-contention']="Measures timings on the L1D sets."
cmd2help['isca-cc-create-l1d-contention']="Generate contention on specific sets."
#llc help string
cmd2help['isca-cc-pp-llc-recv']="Prime+Probe LLC channel receiver."
cmd2help['isca-cc-pp-llc-send']="Prime+Probe LLC channel sender."
cmd2help['isca-cc-pp-llc-benchmark.py']="Run the Prime+Probe LLC covert-channel in benchmark mode and calculate its capacity and bandwidth for different configurations."
#flush-reload help info
cmd2help['isca-cc-fr-recv']="Flush+Reload Receiver Chat Client!\n\tRun the binary as is and you should be all good to go.\n\tFor extra information, check the args info!\n"
cmd2help['isca-cc-fr-send']="Flush+Reload Sender Chat Client!\n\tRun the binary as is and you should be all good to go.\n\tFor extra information, check the args info!\n"
CC_BINS_DIR=/isca19/bins
function _cc_setup_assign_threads() {
	USER_NAME=$(whoami)
	USER_CORE=${user2core[${USER_NAME}]}
	export TSEND=$(echo ${USER_CORE} | cut -d, -f1)
	export TRECV=$(echo ${USER_CORE} | cut -d, -f2)
}
function _cc_welcome_user() {
	echo ""
	echo -e "Welcome to the ISCA 2019 Covert-Channel tutorial hands-on machine!"
	echo -e  "\e[5m \e[96m PLEASE READ THESE INSTRUCTIONS!!!! \e[0m"
	echo ""
	echo "REMEMBER: these covert-channels communicate through physical resources."
	echo -e "\e[31m   !!!! interference across processes is possible and will add"
	echo -e "\e[31m   !!!! noise to your runs and disrupt other students!"
	echo -e "\e[31m   !!!! So please, please please --- pin your sender/receiver!"
	echo -e "\e[0m"
	echo -e "You have been assigned \e[91mThread-${TSEND}\e[0m and \e[91mThread-${TRECV}\e[0m."
	echo -e "You can use these to pin your sender/receiver to these threads!"
	echo -e "Other threads are for other students -- do \e[91mNOT\e[0m pin anywhere else!!"
	echo ""
	echo "For your convenience we created shortcuts to pin your runs"
	echo -e "To pin to your sender core use \e[92m pin_send [program args ...]\e[0m"
	echo -e "To pin to your recevier core use \e[92m pin_recv [program args ...]\e[0m"
	echo -e ""
	TUT_BINS=$(ls /isca19/bins)
	echo "Pre-built tutorial binaries are available for your use:"
	for prog in $(ls /isca19/bins); do
		echo -e "    \e[92m${prog}\e[0m"
	done
	echo -e ""
	echo -e "To see them again, type \e[92misca-cc-\e[0m and hit tab!"
	echo -e ""
	echo -e "For help on individual commands use \"\e[92misca-cc-help\e[0m\""
	echo -e ""
	echo -e "For the sources, git clone https://github.com/yshalabi/covert-channel-tutorial"
	echo -e "Have fun, and remember -- only pin using \e[92mpin_send\e[0m and \e[92mpin_recv\e[0m!!"
}
function _cc_setup_map() {
	NODE=$1
	USER=$2
	for core in $(cat /sys/devices/system/node/node${NODE}/cpu*/topology/thread_siblings_list | cut -d',' -f1,2 |  sort | uniq); do	
		LOGIN=user${USER}
		USER=$((${USER} + 2))
		user2core[${LOGIN}]="${core}"
		#echo "user ${LOGIN} -> ${core}"
	done
}
function pin_send () {
	#echo -e "Running \e[96m$@\e[0m on \e[92Thread-${TSEND}\e[0m"
	taskset -c ${TSEND} "$@"
}
function pin_recv () {
	#echo -e "Running \e[96m$@\e[0m on \e[92Thread-${TRECV}\e[0m"
	taskset -c ${TRECV} "$@"
}

function isca-cc-help() {
	N=$#
	if [[ "$N" -eq "0" ]]; then
		TUT_BINS=$(ls /isca19/bins)
		for prog in $(ls /isca19/bins); do
			echo -e "Cmnd: \e[92m${prog}\e[0m"
			HELP=${cmd2help[${prog}]}
			ARGS=${cmd2args[${prog}]}
			echo -e "Args: \e[96m${ARGS}\e[0m"
			echo -e "Info: \e[96m${HELP}\e[0m"
			echo "------------------------------------------------------------"
			echo ""
		done
		return
	fi
	prog=$1
	HELP=${cmd2help[${prog}]}
	ARGS=${cmd2args[${prog}]}
	echo -e "\e[92m${prog}\e[0m"
	echo -e "Args: \e[92m${ARGS}\e[0m"
	echo -e "Info: \e[92m${HELP}\e[0m"
}
function _cc_setup_user_env() {
	export -f pin_send
	export -f pin_recv
	export -f isca-cc-help
	export PATH=${CC_BINS_DIR}:${PATH}
}
function _cc_setup() {
	_cc_setup_map 0 0
	_cc_setup_map 1 1
	_cc_setup_assign_threads
	_cc_setup_user_env
}

_cc_setup
_cc_welcome_user
