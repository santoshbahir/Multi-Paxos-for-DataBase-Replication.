#ifndef __ERRORCODE__
#define __ERRORCODE__
/*
  Assign error code to each known bug or erroneous behaviour. If I could ever
  be able to revisit this and fix any of the known issue, timestamp the error
  code and comment it out.
 */

#define EGARBAGE 	1		/* Garbage message received */
#define ENOTYPE		2		/* No type set for received message */
#define EEMPTYPE	3		/* Empty type set for received message */
#define ESEND		4		/* Error in send() : Mostly connection reset
							   by peers */
#define EPARTSEND	5		/* Partial message send over socket */
#define ECONNCLOSE	6		/* Socket is orderly shutdown by peer */

#endif
