/* 
   Start routine for the client thread.
 */
void* client_action(void *arg);

/* 
   Start routine for the proposer thread.
 */
void* proposer_action(void *arg);

/* 
   Start routine for the acceptor thread.
 */
void* acceptor_action(void *arg);

/* 
   Start routine for the learner thread.
 */
void* learner_action(void *arg);

/* 
   Start routine for the receiver thread.
 */
void* receiver_action(void *arg);

/*
  Start routine for listening socket.
*/
void* conn_accept(void *arg);
