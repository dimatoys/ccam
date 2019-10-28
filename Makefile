ACCOUNT1=pi@10.0.0.232
#ACCOUNT=pi@10.0.0.117
ACCOUNT2=pi@10.0.0.147

PPATH=projects/ccam
PICS=

LDFLAGS =  -g -Wall -lstdc++ -L/opt/vc/lib -L/usr/local/lib -lmmal -lmmal_components -lmmal_util -lmmal_core -lbcm2835 -ljpeg

%.o : %.C
	g++ -g -Wall -std=c++11 -I/opt/vc/include -c $< -o $@

shutdown1:
	ssh ${ACCOUNT1} "sudo shutdown -h now"

shutdown2:
	ssh ${ACCOUNT2} "sudo shutdown -h now"

run2:
	rsync -r . ${ACCOUNT2}:${PPATH}
	ssh ${ACCOUNT2} "cd ${PPATH} ; make local_run"

master:
	rsync -r . ${ACCOUNT2}:${PPATH}
	ssh ${ACCOUNT2} "cd ${PPATH} ; make local_master"	

slave:
	rsync -r . ${ACCOUNT1}:${PPATH}
	ssh ${ACCOUNT1} "cd ${PPATH} ; make local_slave"	


build1:
	rsync -r . ${ACCOUNT1}:${PPATH}
	ssh ${ACCOUNT1} "cd ${PPATH} ; make ccam"

build2:
	rsync -r . ${ACCOUNT2}:${PPATH}
	ssh ${ACCOUNT2} "cd ${PPATH} ; make ccam"

build: build1 build2

ccam: ccam.o jpeg.o
	g++ $^ $(LDFLAGS) -o $@

local_run: ccam
	sudo ./ccam c

local_master: ccam
	sudo ./ccam m

local_slave: ccam
	sudo ./ccam s`

local_cleanpics:
	rm ~/${PPATH}/../ccampic/* 

test:
	ssh ${ACCOUNT2} "/opt/vc/bin/raspistill -o firstpic.jpg"

