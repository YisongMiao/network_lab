all: master worker

master: master.c
	gcc -Wall -g master.c -o master -lm

worker: worker.c
	gcc -Wall -g worker.c -o worker -lm

clean:
	@rm master worker
