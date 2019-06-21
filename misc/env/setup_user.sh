#!/usr/bin/env bash
declare -A user2core
user2core['ubuntu']='0,0'
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
	echo ""
	echo "Because our covert-channels communicate through physical resources interference across processes is possible!!!"
	echo "This interference will add noise to your runs and disrupt other students!"
	echo "So please	pin your sender/receiver!"
	echo "You have been assigned T${TSEND} and T${TRECV} -- use them, they are all yours!"
	echo "please do not pin your channels elsewhere to avoid interfering with other students!!"
	echo "for your convenience we created shortcuts to pin your runs"
	echo -e "To pin to your sender core use \e[92m pin_send [program args ...]\e[0m"
	echo -e "To pin to your recevier core use \e[92m pin_recv [program args ...]\e[0m"
	echo -e ""
	echo ""
	echo "Pre-built tutorial binaries are available for your use."
	echo -e "\e[96mprime+probe L1D commands:\e[0m"
	echo -e "\e[96m	pp_l1d_send\e[0m"
	echo -e "\e[96m	pp_l1d_recv\e[0m"
	echo -e "\e[96m  viz_l1d_cont\e[0m"
	echo -e "\e[96m  make_l1d_cont\e[0m"
	echo -e ""
	echo -e "the sources for these binaries is at: https://github.com/yshalabi/covert-channel-tutorial"
	echo "Have fun!"
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
	echo "pin_send -c ${TSEND} $@"
}
function pin_recv () {
	echo "pin_recv -c ${TRECV} $@"
}
function _cc_setup_user_env() {
	export -f pin_send
	export -f pin_recv
}
function _cc_setup() {
	_cc_setup_map 0 0
	_cc_setup_map 1 1
	_cc_setup_assign_threads
	_cc_setup_user_env
}
function isca_help() {
	echo "CC HELP MSG"
	ls ${CC_BINS_DIR}
}

_cc_setup
_cc_welcome_user
