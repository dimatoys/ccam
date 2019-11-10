#ACCOUNTS=pi@10.0.0.232
ACCOUNTS=pi@10.0.0.184
#ACCOUNT=pi@10.0.0.117
ACCOUNTM=pi@10.0.0.147

PPATH=projects/ccam
PICS=

LDFLAGS =  -g -Wall -lstdc++ -L/opt/vc/lib -L/usr/local/lib -lmmal -lmmal_components -lmmal_util -lmmal_core -lbcm2835 -ljpeg -lpigpio

%.o : %.C
	g++ -g -Wall -std=c++11 -I/opt/vc/include -c $< -o $@

shutdowns:
	ssh ${ACCOUNTS} "sudo shutdown -h now"

shutdownm:
	ssh ${ACCOUNTM} "sudo shutdown -h now"

builda:
	rsync -r . ${ACCOUNTM}:${PPATH}
	ssh ${ACCOUNTM} "cd ${PPATH} ; make ccam"

runb:
	rsync -r . ${ACCOUNTS}:${PPATH}
	ssh ${ACCOUNTS} "cd ${PPATH} ; make ccam ; make local_cleanpics ; make cleana"
	ssh ${ACCOUNTS} "cd ${PPATH} ; sudo ./ccam b ; cp ../ccampic/* ../ccampics/b ; rsync ${ACCOUNTM}:${PPATH}/../ccampic/* ../ccampics/a"
	rsync -r ${ACCOUNTS}:${PPATH}/../ccampics ../ccampics

master:
	rsync -r . ${ACCOUNTM}:${PPATH}
	ssh ${ACCOUNTM} "cd ${PPATH} ; make local_master"

slave:
	rsync -r . ${ACCOUNTS}:${PPATH}
	ssh ${ACCOUNTS} "cd ${PPATH} ; make local_slave"


builds:
	rsync -r . ${ACCOUNTS}:${PPATH}
	ssh ${ACCOUNTS} "cd ${PPATH} ; make ccam"

build: build1 build2

ccam: ccam.o jpeg.o
	g++ $^ $(LDFLAGS) -o $@

local_run: ccam
	rm ~/${PPATH}/../ccampic/*
	sudo ./ccam c

local_master: ccam
	sudo ./ccam m

local_slave: ccam
	sudo ./ccam s`

local_cleanpics:
	rm -f ~/${PPATH}/../ccampic/* 

cleana:
	ssh ${ACCOUNTM} "cd ${PPATH} ; make local_cleanpics"

test:
	ssh ${ACCOUNTM} "/opt/vc/bin/raspistill -o firstpic.jpg"

