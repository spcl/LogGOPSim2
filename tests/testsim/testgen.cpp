//gcc testgen.cpp -o testgen -lm

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define RANK2VRANK(rank, vrank, root) \
{ \
  vrank = rank; \
  if (rank == 0) vrank = root; \
  if (rank == root) vrank = 0; \
}
#define VRANK2RANK(rank, vrank, root) \
{ \
  rank = vrank; \
  if (vrank == 0) rank = root; \
  if (vrank == root) rank = 0; \
}

void create_scan_bintree_rank(int rank, int size, int datasize, int optime){

    int dmax = (int) log2(size/2);
    int root = (int) size / 2;

    //calc level
    int tmp = root;
    int d = dmax;
    int op=0;
    int dst;
    int parent = -1;
    int maxct = 0;
    while (tmp!=rank){
        parent = tmp;
        if (rank<tmp) tmp = tmp - pow(2, d);
        else tmp = tmp + pow(2, d);
        d--;
    }

    //end calc level
    //printf("rank: %i; d. %i; dmax: %i\n", rank, d, dmax);
    int left = rank - pow(2, d);
    int right = rank + pow(2, d);
    //up-phase
    if (d>=0){ //d<0 is a leaf
        //receive from left l..j-1
        printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag 10\n", op, datasize, left, 0);
        if (op>0) printf("l%i requires l%i\n", op, 0);
        op++;

        //add j to compute l..j (todown) USED IN THE DOWN-PHASE

        //receive from right j+1..r
        printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag 10\n", op, datasize, right, 0);
        if (op>0) printf("l%i requires l%i\n", op, 0);
        op++;
        //add j to comput j..r
        //sum to comput l..r in myval

    }

    if (d!=dmax){ // d==max is the root
        //send myval to parent
        if (d>=0) printf("l%i: tput %ib to %i when %i reaches 2 ct %i tag 10\n", op++, datasize, parent, 0, 1);
        else printf("l%i: put %ib to %i ct %i tag 10\n", op++, datasize, parent, 1);
    }


    //down-phase
    if (d!=dmax){
        //receive 0..l-1 from the parent //store it in
        printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag 10\n", op, datasize, parent, 2);
        if (op>0) printf("l%i requires l%i\n", op, 0);
        op++;

        if (d>=0){
            //send 0..l-1 to the left child
            printf("l%i: tput %ib to %i when %i reaches 1 ct %i tag 10\n", op++, datasize, left, 2, 3);

            //add myval (l..j) to compute (0..j)

            //send (0..j) to the right child
            printf("l%i: tput %ib to %i when %i reaches 1 ct %i tag 10\n", op++, datasize, right, 2, 4);
        }

        printf("l%i: test %i for 1\n", op, 2);
        printf("l%i: calc 1\n", op+1);
        printf("l%i requires l%i\n", op+1, op);
        if (op>0) printf("l%i requires l%i\n", op, op-1);

    }else if (d==dmax){

        printf("l%i: tput %ib to %i when %i reaches 2 ct %i tag 10\n", op++, datasize, left, 0, 3);

        //send it to the right child
        printf("l%i: tput %ib to %i when %i reaches 2 ct %i tag 10\n", op++, datasize, right, 0, 4);

        printf("l%i: test %i for 2\n", op, 0);
        printf("l%i: calc 1\n", op+1);
        printf("l%i requires l%i\n", op+1, op);
        if (op>0) printf("l%i requires l%i\n", op, op-1);
    }

}

void create_scan_sim_rank(int rank, int size, int datasize, int optime){

    int dmax = (int) log2(size);
    int ctcount=0;
    int op=0;
    int stopsend=0, stoprcv=0;
    int d=0;
    int lastappend=-1;
    while (!stopsend || !stoprcv){

        int dst = rank + pow(2, d);
        if (dst<size){
            if (ctcount==0) printf("l%i: put %ib to %i ct %i tag 10\n", op++, datasize, dst, ctcount);
            else {
                printf("l%i: tput %ib to %i when %i reaches 1 ct %i tag 10\n", op, datasize, dst, ctcount-1, ctcount);
                printf("l%i requires l%i\n", op, op-1);
                op++;
            }
            ctcount++;
        }else stopsend=1;


        dst = rank - pow(2, d);
        if (dst>=0) {
            printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag 10\n", op, datasize, dst, ctcount);
            if (op>0) printf("l%i requires l%i\n", op, op-1);
            op++;
            lastappend = ctcount++;
        }else stoprcv=1;

        d++;

    }

    if (lastappend>=0){
        printf("l%i: test %i for 1\n", op, lastappend);
        printf("l%i: calc 1\n", op+1);
        printf("l%i requires l%i\n", op+1, op);
        if (op>0) printf("l%i requires l%i\n", op, op-1);
    }
}

void create_scan_sweep_rank(int rank, int size, int datasize, int optime){
    int op = 0;
    int lastappend=-1;
    int ctcount = 0;
    int dmax = (int) log2(size);
    //dmax=2;
    for (int d=0; d<dmax; d++){
        if ((rank+1) % (int) pow(2, d+1) == 0){
            int dst = rank - pow(2, d);
            if (dst>=0) {
                printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag 10\n", op, datasize, dst, ctcount);
                lastappend = ctcount;
                ctcount++;
                if (op>0)printf("l%i requires l%i\n", op, op-1);
                op++;

            }
        }

        int dst = rank + pow(2, d);
        if ((dst+1) % (int) pow(2, d+1) == 0 && dst<size){
            if (lastappend==-1) printf("l%i: put %ib to %i ct %i tag 10\n", op, datasize, dst, ctcount++);
            else {
                printf("l%i: tput %ib to %i when %i reaches 1 ct %i tag 10\n", op, datasize, dst, lastappend, ctcount++);
                printf("l%i requires l%i\n", op, op-1);
            }
            op++;
        }
    }

    for (int d=dmax-1; d>=0; d--){


        int dst = rank - pow(2, d);
        if (dst >= 0 && (dst+1) % (int) pow(2, d+1) == 0){
            printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag 10\n", op, datasize, dst, ctcount);
            lastappend = ctcount;
            ctcount++;
            if (op>0) printf("l%i requires l%i\n", op, op-1);
            op++;
        }

        dst = rank + pow(2, d);

        if ((rank+1) % (int) pow(2, d+1) == 0 && dst<size){
            //if (appendcount==0) printf("l%i: put %ib to %i ct %i tag 10\n", op++, datasize, dst, putcount++);
            //else
            printf("l%i: tput %ib to %i when %i reaches 1 ct %i tag 10\n", op, datasize, dst, lastappend, ctcount++);
            if (op>0) printf("l%i requires l%i\n", op, op-1);
            op++;
        }

    }

    if (lastappend!=-1){
        printf("l%i: test %i for 1\n", op, lastappend);
        printf("l%i: calc 1\n", op+1);
        printf("l%i requires l%i\n", op+1, op);
        if (op>0) printf("l%i requires l%i\n", op, op-1);
    }
}



void create_scan_double_rank(int rank, int size, int datasize, int optime){

	int mask = 0x1;
	int dst;
	int op = 0;
	int ctcount = 0;
	while (mask < size) {
	  dst = rank^mask;
	  if (dst < size) {
		 //send partial_scan to dst;
		 if (ctcount!=0) {
            printf("l%i: tput %ib to %i when %i reaches 1 ct %i  tag 10\n", op, datasize, dst, ctcount-1, ctcount);
            printf("l%i requires l%i\n", op, op-1);
		 }else printf("l%i: put %ib to %i ct %i tag 10\n", op, datasize, dst, ctcount);
		 ctcount++;
		 op++;
		 //recv from dst into tmp_buf;
		 printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag 10\n", op, datasize, dst, ctcount++);
		 printf("l%i requires l%i\n", op, op-1);
		 op++;
		 //printf("l%i: test %i for 1\n", op, ctcount);
		 //printf("l%i: calc %i\n", op+1, optime);
		 //ctcount++;
		 //printf("l%i requires l%i\n", op+1, op);
		 //op = op + 2;
	  }
	  mask <<= 1;
	}

    printf("l%i: test %i for 1\n", op, ctcount-1);
    printf("l%i: calc 1\n", op+1);
    printf("l%i requires l%i\n", op+1, op);
    if (op>0) printf("l%i requires l%i\n", op, op-1);
}

void create_scan(int size, int datasize, int optime, int mode){
	printf("num_ranks %i\n", size);
	for (int rank=0; rank<size; rank++){
		printf("rank %i {\n", rank);
		if (mode==0) create_scan_sweep_rank(rank, size, datasize, optime);
		else if (mode==1) create_scan_double_rank(rank, size, datasize, optime);
		else if (mode==2) create_scan_sim_rank(rank, size, datasize, optime);
		else create_scan_bintree_rank(rank, size, datasize, optime);
		printf("}\n");
	}
}


void create_gather(int comm_size, int datasize, int root) {


    printf("num_ranks %i\n", comm_size);

	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
        printf("rank %i{\n", comm_rank);
		int vrank;
		RANK2VRANK(comm_rank, vrank, root);

		if (vrank == 0) {
            printf("l0: append %ib to priority_list allowed -1 ct 0 tag 10\n", datasize*(comm_size-1));
            printf("l1: test 0 for %i\n", comm_size-1);
            printf("l1 requires l0\n");
            printf("l2: calc 1\n");
            printf("l2 requires l1\n");

		}
		else {
			int sendpeer = 0;
			int vpeer = sendpeer;
			VRANK2RANK(sendpeer, vpeer, root);
			printf("l0: put %ib to 0 ct 0 tag 10\n", datasize);
		}
		printf("}\n");
	}
}


void create_binomial_tree_bcast_rank(int root, int comm_rank, int comm_size, int datasize, int wait) {
	int vrank;
	RANK2VRANK(comm_rank, vrank, root);
    int ct=0;

	int recv = -1, send = -1;
	for (int r = 0; r < ceil(log2(comm_size)); r++) {
		int vpeer =  vrank+(int)pow(2,r);
		int peer;
		VRANK2RANK(peer, vpeer, root);
		if ((vrank+pow(2,r) < comm_size) and (vrank < pow(2,r))) {
			//send = goal->Send(datasize, peer);
            send++;
            if (comm_rank==root){
                 printf("l%i: put %ib to %i ct %i tag 10\n", send, datasize, peer, ct++);
                 if (send>0) printf("l%i requires l%i\n", send, send-1);
            }else{
                printf("l%i: tput %ib to %i when %i reaches 1 ct %i tag 10\n", send, datasize, peer, ct-1, ct);
                ct++;
                if (send>0) printf("l%i requires l%i\n", send, 0);
            }

		}

		vpeer = vrank-(int)pow(2, r);
		VRANK2RANK(peer, vpeer, root);
		if ((vrank >= pow(2,r)) and (vrank < pow(2, r+1))) {
			//recv = goal->Recv(datasize, peer);
			send++;
			printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag 10\n", send, datasize, peer, ct++);
		}
	}

	if (comm_rank==comm_size-1 && wait){
        send++;
        printf("l%i: test %i for %i\n", send, ct-1, 1);
        printf("l%i requires l%i\n", send, send-1);
        send++;
        printf("l%i: calc 1\n", send);
        printf("l%i requires l%i\n", send, send-1);
	}
}

void create_binomial_tree_bcast(int comm_size, int datasize, int root) {


    printf("num_ranks %i\n", comm_size);
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
	  printf("rank %i{\n", comm_rank);
      create_binomial_tree_bcast_rank(root, comm_rank, comm_size, datasize, 1);
	  printf("}\n");
	}

}






void create_allreduce_rank(int comm_size, int rank, int size){

  int r, maxr, vpeer, peer, root, p;


  int vrank;
  int blocksize = size;

  p = comm_size;
  int tag=10;

  root = 0; /* this makes the code for ireduce and iallreduce nearly identical - could be changed to improve performance */
  RANK2VRANK(rank, vrank, root);
  maxr = (int)ceil((log2(p)));


  int send=0;
  int recv=0;
  int op=0;
  int hookct = 1;
  int nextct = 2;
  
  int childs=0;
  for(r=1; r<=maxr; r++) {
    if((vrank % (1<<r)) == 0) {
      /* we have to receive this round */
      vpeer = vrank + (1<<(r-1));
      VRANK2RANK(peer, vpeer, root)
      if(peer<p) {
        //printf("up: recv from %i\n", peer);

        printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag %i\n", op++, blocksize, peer, nextct++, tag);
        printf("l%i: tput 1b to %i when %i reaches 1 ct %i tag -1\n", op++, rank, nextct-1, hookct);
        childs++;
      }
    }
  }

  for(r=1; r<=maxr; r++) {
    if((vrank % (1<<r)) != 0) {
      /* we have to send this round */
      vpeer = vrank - (1<<(r-1));
      VRANK2RANK(peer, vpeer, root)
      if (childs>0) printf("l%i: tput %ib to %i when %i reaches %i ct %i tag %i\n", op++, blocksize, peer, hookct, childs, nextct++, tag);
      else printf("l%i: put %ib to %i ct %i tag %i\n", op++, blocksize, peer, nextct++, tag);
      break;
    }
  }

  /* this is the Bcast part - copied with minor changes from nbc_ibcast.c
   * changed: buffer -> recvbuf  */
  RANK2VRANK(rank, vrank, root);

  int recvct = -1;
  /* receive from the right hosts  */
  if(vrank != 0) {
    for(r=0; r<maxr; r++) {
      if((vrank >= (1<<r)) && (vrank < (1<<(r+1)))) {
        VRANK2RANK(peer, vrank-(1<<r), root);
        recvct = nextct;
        printf("l%i: append %ib to priority_list allowed %i ct %i use_once tag %i\n", op++, blocksize, peer, nextct++, tag);

      }
    }
  }

  if (recvct==-1) recvct=hookct;

  /* now send to the right hosts */
  for(r=0; r<maxr; r++) {
    if(((vrank + (1<<r) < p) && (vrank < (1<<r))) || (vrank == 0)) {
      VRANK2RANK(peer, vrank+(1<<r), root);
      printf("l%i: tput %ib to %i when %i reaches %i ct %i tag %i\n", op++, blocksize, peer, recvct, 1, nextct++, tag);
    }
  }
  /* end of the bcast */

}


void create_binomial_allreduce(int comm_size, int datasize) {


    printf("num_ranks %i\n", comm_size);
	for (int comm_rank = 0; comm_rank < comm_size; comm_rank++) {
	  printf("rank %i{\n", comm_rank);
          create_allreduce_rank(comm_size, comm_rank, datasize);
	  printf("}\n");
	}

}



int main(int argc, char * argv[]){


    if (argc!=4 && argc!=5){
        printf("Usage: %s <operation> <comm_size> <message_size> [root=0]\n", argv[0]);
    }

    int root = (argc==5) ? atoi(argv[4]) : 0;

    /* Args: operation, comm_size, datasize, root (if needed) */
    if (!strcmp(argv[1], "binomialtreebcast")) create_binomial_tree_bcast(atoi(argv[2]), atoi(argv[3]), root);
    else if (!strcmp(argv[1], "gather")) create_gather(atoi(argv[2]), atoi(argv[3]), root);
    else if (!strcmp(argv[1], "allreduce")) create_binomial_allreduce(atoi(argv[2]), atoi(argv[3]));
	else if (!strcmp(argv[1], "scan_double")) create_scan(atoi(argv[2]), atoi(argv[3]), root, 1);
	else if (!strcmp(argv[1], "scan_sweep")) create_scan(atoi(argv[2]), atoi(argv[3]), root, 0);
	else if (!strcmp(argv[1], "scan_simbin")) create_scan(atoi(argv[2]), atoi(argv[3]), root, 2);
	else if (!strcmp(argv[1], "scan_bintree")) create_scan(atoi(argv[2]), atoi(argv[3]), root, 3);
    else printf("Invalid operation\n");
    return 0;
}

