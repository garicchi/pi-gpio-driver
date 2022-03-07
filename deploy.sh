#!/bin/bash -xeu

SCRIPT_PATH=$(cd $(dirname $0); pwd)

cd $SCRIPT_PATH

RASPI_HOST=raspi

rsync -rv --delete . ${RASPI_HOST}:~/pi-gpio-driver
