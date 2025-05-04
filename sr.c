#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"


#define RTT 16.0
#define WINDOWSIZE 6
#define SEQSPACE 12
#define NOTINUSE (-1)

int ComputeChecksum(struct pkt packet)
{
    int checksum = 0;
    int i;

    checksum = packet.seqnum;  
    checksum += packet.acknum;
    /* Add each byte of the payload*/
    for (i = 0; i < 20; i++)
        checksum += (int)(packet.payload[i]);

    return checksum; /* Return the computed checksum */
}

/*Check if a packet is corrupted */

bool IsCorrupted(struct pkt packet)
{
    return (packet.checksum != ComputeChecksum(packet));
}

static struct pkt buffer[WINDOWSIZE];
static int windowfirst, windowlast;
static int windowcount;
static int A_nextseqnum;
static bool acked[WINDOWSIZE];

/*Handle a new message and send a packet*/
void A_output(struct msg message)
{
  struct pkt sendpkt;
  int i;

  if (windowcount < WINDOWSIZE)
  {
    if (TRACE > 1)
      printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for (i = 0; i < 20; i++)
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    windowlast = (windowlast + 1) % WINDOWSIZE;
    buffer[windowlast] = sendpkt;
    windowcount++;

    if (TRACE > 0)
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3(A, sendpkt);

    if (windowcount == 1)
      starttimer(A, RTT);

    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
  }
  else
  {
    if (TRACE > 0)
      printf("----A: New message arrives, send window is full\n");
    window_full++;
  }
}
/*Handle received ACKs*/
void A_input(struct pkt packet)
{
  if (!IsCorrupted(packet))
  {
    if (TRACE > 0)
      printf("----A: uncorrupted ACK %d is received\n", packet.acknum);

    if (!acked[packet.acknum])
    {
      if (TRACE > 0)
        printf("----A: ACK %d is not a duplicate\n", packet.acknum);
      new_ACKs++;
      acked[packet.acknum] = true;

      if (packet.acknum == buffer[windowfirst].seqnum)
      {
        while (windowcount > 0 && acked[buffer[windowfirst].seqnum])
        {
          windowfirst = (windowfirst + 1) % WINDOWSIZE;
          windowcount--;
        }

        stoptimer(A);
        if (windowcount > 0)
          starttimer(A, RTT);
      }
    }
    else if (TRACE > 0)
      printf("----A: duplicate ACK received, do nothing!\n");
  }
  else if (TRACE > 0)
    printf("----A: corrupted ACK is received, do nothing!\n");
}
/*Handle timer interrupts to resend packets*/
void A_timerinterrupt(void)
{
  if (TRACE > 0)
    printf("----A: time out,resend packets!\n");

  if(TRACE > 0)
    printf("---A: resending packet %d\n", buffer[windowfirst].seqnum);

  tolayer3(A, buffer[windowfirst]);
  packets_resent++;

  if(windowcount > 0)
    starttimer(A, RTT);
}
/*Initialize the sender*/
void A_init(void)
{
  A_nextseqnum = 0;
  windowfirst = 0;
  windowlast = -1;
  windowcount = 0;
}

static int expectedseqnum;
static struct pkt recvpkt[SEQSPACE];
static bool received[SEQSPACE];
/*Handle received packets*/
void B_input(struct pkt packet)
{
  struct pkt sendpkt;
  int i;

  if ((!IsCorrupted(packet)))
  {
    if (TRACE > 0)
      printf("----B: packet %d is correctly received, send ACK!\n", packet.seqnum);
    packets_received++;

    if(received[packet.seqnum] == false)
    {
      received[packet.seqnum] = true;
      for(i = 0; i < 20; i++)
        recvpkt[packet.seqnum].payload[i] = packet.payload[i];
    }

    while(received[expectedseqnum] == true)
    {
      tolayer5(B, packet.payload);
      received[expectedseqnum] = false;
      expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
    }

    sendpkt.acknum = packet.seqnum;
    sendpkt.seqnum = NOTINUSE;
    for(i = 0; i < 20; i++)
      sendpkt.payload[i] = '0';
    
    sendpkt.checksum = ComputeChecksum(sendpkt);

    tolayer3(B, sendpkt);
  }
}
/*Initialize the receiver*/
void B_init(void)
{
  expectedseqnum = 0;
}

void B_output(struct msg message)
{
}

void B_timerinterrupt(void)
{
}