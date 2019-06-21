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

	echo "Pre-built tutorial binaries are available for your use."
	echo "To see them all type isca-cc- and hit tab!"
	echo ""

	echo -e "\e[96mflush+reload commands:\e[0m"
	echo -e "\e[96m	fr-send		-- flush+reload chat client (sender)\e[0m"
	echo -e "\e[96m	fr-recv		-- flush+reload chat client (receiver)\e[0m"
	echo ""
	echo -e "\e[96mprime+probe llc commands:\e[0m"
	echo -e "\e[96m	pp-llc-send		-- prime+probe (LLC cache) covert chat client (sender)\e[0m"
	echo -e "\e[96m	pp-llc-recv		-- prime+probe (LLC cache) covert chat client (receiver)\e[0m"
	echo -e "\e[96m pp-llc-graph		-- graph channel bandwidth of LLC covert channel\e[0m"
	echo -e ""
	echo -e "\e[96mprime+probe L1D commands:\e[0m"
	echo -e "\e[96m	pp-l1d-send		-- prime+probe (L1D cache) covert chat client (sender)\e[0m"
	echo -e "\e[96m	pp-l1d-recv		-- prime+probe (L1D cache) covert chat client (receiver)\e[0m"
	echo -e "\e[96m measure-l1d-contention	-- measure contention in L1D cache\e[0m"
	echo -e "\e[96m create-l1d-contention	-- graph contention in L1D cache in terminal\e[0m"
	echo ""
	echo -e "get the sources using: git clone https://github.com/yshalabi/covert-channel-tutorial"
	echo "Have fun, and remember -- only pin using pin_send and pin_recv!!"
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

function cc_help() {
	echo "covert-channel-tutorial help"
	ls ${CC_BINS_DIR}
}
function _cc_setup_user_env() {
	export -f pin_send
	export -f pin_recv
	export -f cc_help
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
