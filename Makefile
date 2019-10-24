ACCOUNT=pi@10.0.0.232
PPATH=projects/ccam

LDFLAGS =  -g -Wall -lstdc++ -L/opt/vc/lib -L/usr/local/lib -lmmal -lmmal_components -lmmal_util -lmmal_core -lbcm2835

%.o : %.C
	gcc -g -Wall -std=c++11 -I/opt/vc/include -c $< -o $@

shutdown:
	ssh ${ACCOUNT} "sudo shutdown -h now"

rccam:
	rsync -r . ${ACCOUNT}:${PPATH}
	ssh ${ACCOUNT} "cd ${PPATH} ; make eccam"

ccam: ccam.o 
	gcc $(LDFLAGS) $^ -o $@

eccam: ccam
	./ccam

test:
	ssh ${ACCOUNT} "/opt/vc/bin/raspistill -o firstpic.jpg"

