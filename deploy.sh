#!/bin/bash
MINIX=130.245.156.19
SRC=$(ls *.c *.h Makefile README.md)
scp $SRC cse533-$1@$MINIX:/home/courses/cse533/students/cse533-$1/cse533/
ssh cse533-$1@$MINIX "cd cse533 && make clean && make USER=cse533-$1 && ./deploy_app tour_cse533-$1 arp_cse533-$1"
