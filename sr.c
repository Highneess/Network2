#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"


/* ******************************************************************
   Go Back N protocol.  Adapted from J.F.Kurose
   ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.2

   Network properties:
   - one way network delay averages five time units (longer if there
   are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
   or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
   (although some can be lost).

   Modifications:
   - removed bidirectional GBN code and other code not used by prac.
   - fixed C style to adhere to current programming style
   - added GBN implementation
**********************************************************************/

#define RTT  16.0       /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packet
                          MUST BE SET TO 6 when submitting assignment */
#define SEQSPACE 12      /* the min sequence space for GBN must be at least windowsize + 1 */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for (i = 0; i < 20; i++)
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}


/********* Sender (A) variables and functions ************/

static struct pkt buffer[WINDOWSIZE]; // Buffer for packets in the window
static int windowfirst, windowlast;     // Indices for the start and end of the window
static int windowcount;                 // Count of packets in the current window
static int A_nextseqnum;                // Next sequence number to send
static bool acked[WINDOWSIZE];          // Array to track ACK status

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_output(struct msg message)
{
    struct pkt sendpkt; // Packet to be sent
    int i;

    // If the window is not full, send a new packet
    if (windowcount < WINDOWSIZE)
    {
        if (TRACE > 1)
            printf("----A: New message arrives, send window is not full, send new message to layer3!\n");
    /* create packet */
    sendpkt.seqnum = A_nextseqnum; // Set the sequence number
        sendpkt.acknum = NOTINUSE;     // Acknowledgment number is not used
        
        // Copy message data into the packet
        for (i = 0; i < 20; i++)
            sendpkt.payload[i] = message.data[i];
        sendpkt.checksum = ComputeChecksum(sendpkt); // Compute the checksum

        windowlast = (windowlast + 1) % WINDOWSIZE; // Update the end index of the window
        buffer[windowlast] = sendpkt; // Place the packet in the buffer
        windowcount++; // Increment the count of packets in the window

        if (TRACE > 0)
            printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
        tolayer3(A, sendpkt); // Send the packet to layer 3

    /* start timer if first packet in window */
    if (windowcount == 1)
            starttimer(A, RTT);

        // Update the next sequence number
        A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
    }
    else
    {
        if (TRACE > 0)
            printf("----A: New message arrives, send window is full\n");
        window_full++; // Count of full window messages
    }
}

// Handle received ACKs
void A_input(struct pkt packet)
{
    // If the ACK is not corrupted
    if (!IsCorrupted(packet))
    {
        if (TRACE > 0)
            printf("----A: uncorrupted ACK %d is received\n", packet.acknum);

        // If the ACK is new
        if (!acked[packet.acknum])
        {
            if (TRACE > 0)
                printf("----A: ACK %d is not a duplicate\n", packet.acknum);
            new_ACKs++;
            acked[packet.acknum] = true;
            if (packet.acknum == buffer[windowfirst].seqnum)
            {
                // Move the window and handle acknowledged packets
                while (windowcount > 0 && acked[buffer[windowfirst].seqnum])
                {
                    windowfirst = (windowfirst + 1) % WINDOWSIZE; 
                    windowcount--; 
                }

                stoptimer(A); 
                if (windowcount > 0)
                    starttimer(A, RTT); // Restart the timer if there are still packets in the window
            }
        }
        else if (TRACE > 0)
            printf("----A: duplicate ACK received, do nothing!\n");
    }
    else if (TRACE > 0)
        printf("----A: corrupted ACK is received, do nothing!\n");
}
/* called when A's timer goes off */
void A_timerinterrupt(void)
{
    if (TRACE > 0)
        printf("----A: time out, resend packets!\n");

    if(TRACE > 0)
        printf("---A: resending packet %d\n", buffer[windowfirst].seqnum);

    tolayer3(A, buffer[windowfirst]); // Resend the first packet in the window
    packets_resent++; // Increment resend count

    if(windowcount > 0)
        starttimer(A, RTT); // Restart the timer
}
// Initialize the sender
void A_init(void)
{
    A_nextseqnum = 0; 
    windowfirst = 0;
    windowlast = -1;
    windowcount = 0; 
}

// Variables related to the receiver
static int expectedseqnum; 
static struct pkt recvpkt[SEQSPACE]; 
static bool received[SEQSPACE]; 

void B_input(struct pkt packet)
{
  struct pkt sendpkt;
  int i;

  /* if not corrupted and received packet is in order */
  if  ( (!IsCorrupted(packet))  && (packet.seqnum == expectedseqnum) ) {
    if (TRACE > 0)
      printf("----B: packet %d is correctly received, send ACK!\n",packet.seqnum);
    packets_received++;

    /* deliver to receiving application */
    tolayer5(B, packet.payload);

    /* send an ACK for the received packet */
    sendpkt.acknum = expectedseqnum;

    /* update state variables */
    expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
  }
  else {
    /* packet is corrupted or out of order resend last ACK */
    if (TRACE > 0)
      printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
    if (expectedseqnum == 0)
      sendpkt.acknum = SEQSPACE - 1;
    else
      sendpkt.acknum = expectedseqnum - 1;
  }

  /* create packet */
  sendpkt.seqnum = B_nextseqnum;
  B_nextseqnum = (B_nextseqnum + 1) % 2;

  /* we don't have any data to send.  fill payload with 0's */
  for ( i=0; i<20 ; i++ )
    sendpkt.payload[i] = '0';

  /* computer checksum */
  sendpkt.checksum = ComputeChecksum(sendpkt);

  /* send out packet */
  tolayer3 (B, sendpkt);
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
  expectedseqnum = 0;
  B_nextseqnum = 1;
}

/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(struct msg message)
{
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
}
