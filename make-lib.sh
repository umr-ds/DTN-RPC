#! /bin/bash
clear
if [ $# -gt 1 ]; then
	printf "\033[0m\033[31mUsage: $0 [<prefix>]\033[0m\n"
	exit 1
fi

PREFIX=$1

printf "\033[0m\033[32mCloning Serval DNA.\033[0m\n"
git clone https://github.com/servalproject/serval-dna.git sdna
printf "\033[0m\033[32mSwitching branch to \"batphone-release-0.93\"\033[0m\n"
cd sdna && git checkout batphone-release-0.93

printf "\033[0m\033[32mRunning \"autoreconf\"\033[0m\n"
autoreconf -i -f -I m4

if [ ! -z $PREFIX ]; then
	printf "\033[0m\033[32mRunning \"./configure --prefix=$PREFIX\"\033[0m\n"
	./configure --prefix=$PREFIX
else
	printf "\033[0m\033[32mRunning \"./configure\"\033[0m\n"
	./configure
fi

printf "\033[0m\033[32mRunning \"make\"\033[0m\n"
make -j9 && cd ..

printf "\033[0m\033[32mBuilding library\033[0m\n"
source lib_objs
ar rcs libservalrpc.a $SERVAL_OBJS $NACL_OBJS && mv libservalrpc.a src/

printf "\033[0m\033[32mCleaning up\033[0m\n"
rm -rf sdna/
