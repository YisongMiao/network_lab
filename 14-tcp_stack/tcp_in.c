#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>

// handling incoming packet for TCP_LISTEN state
//
// 1. malloc a child tcp sock to serve this connection request; 
// 2. send TCP_SYN | TCP_ACK by child tcp sock;
// 3. hash the child tcp sock into established_table (because the 4-tuple 
//    is determined).
void tcp_state_listen(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement this function please listen.\n");
	printf("-----Under tcp_state_listen\n");
	
	tcp_set_state(tsk, TCP_SYN_RECV);

	struct tcp_sock *new_tsk = malloc(sizeof(struct tcp_sock));
	memcpy(new_tsk, tsk, (sizeof(struct tcp_sock)));
	new_tsk->parent = tsk;
	new_tsk->sk_dip = cb->saddr;
	new_tsk->sk_sip = cb->daddr;
	new_tsk->sk_dport = cb->sport;
	new_tsk->sk_sport = cb->dport;

	new_tsk->rcv_nxt = cb->seq + 1;  //ack = x + 1, see in PPT Chapter5-2, Page41

	//put into its parent's listen_queue
	list_add_tail(&new_tsk->list, &new_tsk->parent->listen_queue);

	tcp_hash(new_tsk);
	printf("Hashed child tsk!\n");

	tcp_send_control_packet(new_tsk, TCP_SYN|TCP_ACK);
	printf("*****Sent a TCP packet: TCP_SYN|TCP_ACK\n");
}

// handling incoming packet for TCP_CLOSED state, by replying TCP_RST
void tcp_state_closed(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	tcp_send_reset(cb);
}

// handling incoming packet for TCP_SYN_SENT state
//
// If everything goes well (the incoming packet is TCP_SYN|TCP_ACK), reply with 
// TCP_ACK, and enter TCP_ESTABLISHED state, notify tcp_sock_connect; otherwise, 
// reply with TCP_RST.
void tcp_state_syn_sent(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement this function please.syn_sent\n");
	printf("-----Under tcp_state_syn_sent\n");

	struct tcphdr *the_tcphdr = packet_to_tcp_hdr(packet);
	if(the_tcphdr->flags == (TCP_SYN + TCP_ACK)){
		printf("Receive TCP_SYN|TCP_ACK\n");
		tsk->rcv_nxt = cb->seq + 1;  //ack = y + 1
		tcp_send_control_packet(tsk, TCP_ACK);
		printf("*****Sent a TCP packet: TCP_ACK\n");
		tcp_set_state(tsk, TCP_ESTABLISHED);
		wake_up(tsk->wait_connect);
	}
}

// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0)
		wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (tsk->snd_una <= cb->ack && cb->ack <= tsk->snd_nxt)
		tcp_update_window(tsk, cb);
}

// handling incoming ack packet for tcp sock in TCP_SYN_RECV state
//
// 1. remove itself from parent's listen queue;
// 2. add itself to parent's accept queue;
// 3. wake up parent (wait_accept) since there is established connection in the
//    queue.
void tcp_state_syn_recv(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	fprintf(stdout, "TODO: implement this function please.syn_recv\n");
	tcp_set_state(tsk, TCP_ESTABLISHED);
	list_delete_entry(&tsk->list);
	if(tsk->parent){
		printf("parent exist\n");
	}
	else{
		printf("parent not exist\n");
	}
	tcp_sock_accept_enqueue(tsk);
	wake_up(tsk->parent->wait_accept);
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (cb->seq < rcv_end && tsk->rcv_nxt <= cb->seq_end) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

// Process an incoming packet as follows:
// 	 1. if the state is TCP_CLOSED, hand the packet over to tcp_state_closed;
// 	 2. if the state is TCP_LISTEN, hand it over to tcp_state_listen;
// 	 3. if the state is TCP_SYN_SENT, hand it to tcp_state_syn_sent;
// 	 4. check whether the sequence number of the packet is valid, if not, drop
// 	    it;
// 	 5. if the TCP_RST bit of the packet is set, close this connection, and
// 	    release the resources of this tcp sock;
// 	 6. if the TCP_SYN bit is set, reply with TCP_RST and close this connection,
// 	    as valid TCP_SYN has been processed in step 2 & 3;
// 	 7. check if the TCP_ACK bit is set, since every packet (except the first 
//      SYN) should set this bit;
//   8. process the ack of the packet: if it ACKs the outgoing SYN packet, 
//      establish the connection; (if it ACKs new data, update the window;)
//      if it ACKs the outgoing FIN packet, switch to correpsonding state;
//   9. (process the payload of the packet: call tcp_recv_data to receive data;)
//  10. if the TCP_FIN bit is set, update the TCP_STATE accordingly;
//  11. at last, do not forget to reply with TCP_ACK if the connection is alive.
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement this function please. tcp_process\n");
	switch(tsk->state){
		case TCP_CLOSED:
			tcp_state_closed(tsk, cb, packet);
			break;
		case TCP_LISTEN:
			tcp_state_listen(tsk, cb, packet);
			break;
		case TCP_SYN_SENT:
			tcp_state_syn_sent(tsk, cb, packet);
			break;
		case TCP_SYN_RECV:
			tcp_state_syn_recv(tsk, cb, packet);
			break;
		case TCP_ESTABLISHED:
			//it is only for the server side.
			if(cb->flags & TCP_FIN){
				tsk->rcv_nxt = cb->seq + 1;
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_set_state(tsk, TCP_CLOSE_WAIT);
			}
			break;
		case TCP_FIN_WAIT_1:
			if(cb->flags & TCP_ACK){
				tcp_set_state(tsk, TCP_FIN_WAIT_2);
			}
			break;
		case TCP_FIN_WAIT_2:
			if(cb->flags & TCP_FIN){
				tsk->rcv_nxt = cb->seq + 1;
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_set_state(tsk, TCP_TIME_WAIT);
				tcp_set_timewait_timer(tsk);
			}
			break;
		case TCP_LAST_ACK:
			if(cb->flags & TCP_ACK){
				tcp_set_state(tsk, TCP_CLOSED);
			}
			break;
	}
	if(cb->flags & TCP_RST){  //check RST
		tcp_sock_close(tsk);
		free_tcp_sock(tsk);
	}
	//if(cb->flags & TCP_SYN){  //check SYN
	//	if(tsk->state != TCP_CLOSED && tsk->state != TCP_SYN_SENT){
	//		tcp_send_control_packet(tsk, TCP_RST);
	//		tcp_sock_close(tsk);
	//	}
	//}
}
