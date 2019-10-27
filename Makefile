#ACCOUNT=pi@10.0.0.232
#ACCOUNT=pi@10.0.0.117
ACCOUNT=pi@10.0.0.147

PPATH=projects/ccam
PICS=

LDFLAGS =  -g -Wall -lstdc++ -L/opt/vc/lib -L/usr/local/lib -lmmal -lmmal_components -lmmal_util -lmmal_core -lbcm2835 -ljpeg

%.o : %.C
	g++ -g -Wall -std=c++11 -I/opt/vc/include -c $< -o $@

shutdown:
	ssh ${ACCOUNT} "sudo shutdown -h now"

run:
	rsync -r . ${ACCOUNT}:${PPATH}
	ssh ${ACCOUNT} "cd ${PPATH} ; make local_run"

build:
	rsync -r . ${ACCOUNT}:${PPATH}
	ssh ${ACCOUNT} "cd ${PPATH} ; make ccam"

ccam: ccam.o jpeg.o
	g++ $^ $(LDFLAGS) -o $@

local_run: ccam
	sudo ./ccam

local_cleanpics:
	rm ~/${PPATH}/../ccampic/* 

test:
	ssh ${ACCOUNT} "/opt/vc/bin/raspistill -o firstpic.jpg"

