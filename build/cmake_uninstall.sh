#!/bin/bash

CYAN='\e[36m'
PURPLE='\e[35m'
NC='\e[0m' # No Color

uninstall () {
    NAME=$1
    NAME=${NAME##*_}
    NAME=${NAME%.*}
    NAME=${NAME/manifest/the project}
    if [ -e $1 ]
    then
        echo -e "${CYAN}Uninstall " $NAME"...${NC}"
        xargs rm -v < $1
        rm -f $1
    fi
}

if [[ $EUID -ne 0 ]]; then
   echo -e "${PURPLE}Permission denied${NC}" 1>&2
   exit 100
fi

for manifest in "$search_dir"${PWD}/install_manifest*
do
   uninstall $manifest
done
