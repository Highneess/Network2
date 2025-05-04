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

    checksum = packet.seqnum;  // Add sequence number
    checksum += packet.acknum; // Add acknowledgment number
    
    // Add each byte of the payload
    for (i = 0; i < 20; i++)
        checksum += (int)(packet.payload[i]);

    return checksum; // Return the computed checksum
}

// Check if a packet is corrupted
bool IsCorrupted(struct pkt packet)
{
    // Return true if the checksum does not match
    return (packet.checksum != ComputeChecksum(packet));
}

static struct pkt buffer[WINDOWSIZE];
static int windowfirst, windowlast;
static int windowcount;
static int A_nextseqnum;
static bool acked[WINDOWSIZE];

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

<<<<<<< HEAD
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

=======
        // If the ACK is new
        if (!ackStatus[packet.acknum])
        {
            if (TRACE > 0)
                printf("----A: ACK %d is not a duplicate\n", packet.acknum);
            new_ACKs++; // Count of new ACKs
            ackStatus[packet.acknum] = true; // Mark this ACK as received

            // If the ACK corresponds to the first packet in the window
            if (packet.acknum == packetBuffer[windowStart].seqnum)
            {
                // Move the window and handle acknowledged packets
                while (currentWindowCount > 0 && ackStatus[packetBuffer[windowStart].seqnum])
                {
                    windowStart = (windowStart + 1) % WINDOWSIZE; // Update the start index
                    currentWindowCount--; // Decrement the count of packets in the window
                }

                stoptimer(A); // Stop the timer
                if (currentWindowCount > 0)
                    starttimer(A, RTT); // Restart the timer if there are still packets in the window
            }
        }
        else if (TRACE > 0)
            printf("----A: duplicate ACK received, do nothing!\n"); // Handle duplicate ACK
    }
    else if (TRACE > 0)
        printf("----A: corrupted ACK is received, do nothing!\n"); // Handle corrupted ACK
}

// Handle timer interrupts to resend packets
>>>>>>> a41b488832cc79d99fb214be4eaf4942fbc554af
void A_timerinterrupt(void)
{
  if (TRACE > 0)
    printf("----A: time out,resend packets!\n");

<<<<<<< HEAD
  if(TRACE > 0)
    printf("---A: resending packet %d\n", buffer[windowfirst].seqnum);

  tolayer3(A, buffer[windowfirst]);
  packets_resent++;

  if(windowcount > 0)
    starttimer(A, RTT);
}

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
=======
    if(TRACE > 0)
        printf("---A: resending packet %d\n", packetBuffer[windowStart].seqnum);

    tolayer3(A, packetBuffer[windowStart]); // Resend the first packet in the window
    packets_resent++; // Increment resend count

    if(currentWindowCount > 0)
        starttimer(A, RTT); // Restart the timer
}

// Initialize the sender
void A_init(void)
{
    nextSeqNum = 0; // Set the next sequence number to 0
    windowStart = 0; // Initialize the start index of the window
    windowEnd = -1; // Initialize the end index of the window
    currentWindowCount = 0; // Initialize the count of packets in the window
}

// Variables related to the receiver
static int expectedSeqNum; // Expected sequence number
static struct pkt recvPacket[SEQSPACE]; // Buffer for received packets
static bool receivedStatus[SEQSPACE]; // Array to track received status
>>>>>>> a41b488832cc79d99fb214be4eaf4942fbc554af

void B_input(struct pkt packet)
{
<<<<<<< HEAD
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
=======
    struct pkt ackPacket; // ACK packet to be sent
    int i;

    // If the packet is not corrupted
    if ((!IsCorrupted(packet)))
    {
        if (TRACE > 0)
            printf("----B: packet %d is correctly received, send ACK!\n", packet.seqnum);
        packets_received++; // Increment received packet count

        // If this is a new packet
        if(receivedStatus[packet.seqnum] == false)
        {
            receivedStatus[packet.seqnum] = true; // Mark as received
            // Copy the payload of the packet
            for(i = 0; i < 20; i++)
                recvPacket[packet.seqnum].payload[i] = packet.payload[i];
        }

        // Process consecutively received packets
        while(receivedStatus[expectedSeqNum] == true)
        {
            tolayer5(B, recvPacket[expectedSeqNum].payload); // Deliver the data to the application layer
            receivedStatus[expectedSeqNum] = false; // Reset the received status
            expectedSeqNum = (expectedSeqNum + 1) % SEQSPACE; // Update expected sequence number
        }

        // Prepare to send ACK
        ackPacket.acknum = packet.seqnum; // Set the acknowledgment number
        ackPacket.seqnum = NOTINUSE; // Sequence number is not used
        for(i = 0; i < 20; i++)
            ackPacket.payload[i] = '0'; // Fill the payload

        ackPacket.checksum = ComputeChecksum(ackPacket); // Compute the checksum for the ACK

        tolayer3(B, ackPacket); // Send the ACK to layer 3
>>>>>>> a41b488832cc79d99fb214be4eaf4942fbc554af
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

void B_init(void)
{
<<<<<<< HEAD
  expectedseqnum = 0;
}

=======
    expectedSeqNum = 0; // Set the expected sequence number to 0
}

// Output function for the receiver (not implemented)
>>>>>>> a41b488832cc79d99fb214be4eaf4942fbc554af
void B_output(struct msg message)
{
}

<<<<<<< HEAD
=======
// Timer interrupt function for the receiver (not implemented)
>>>>>>> a41b488832cc79d99fb214be4eaf4942fbc554af
void B_timerinterrupt(void)
{
}