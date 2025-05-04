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

// Variables related to the sliding window
static struct pkt packetBuffer[WINDOWSIZE]; // Buffer for packets in the window
static int windowStart, windowEnd;           // Indices for the start and end of the window
static int currentWindowCount;                // Count of packets in the current window
static int nextSeqNum;                        // Next sequence number to send
static bool ackStatus[WINDOWSIZE];           // Array to track ACK status

// Handle a new message and send a packet
void A_output(struct msg message)
{
    struct pkt outgoingPacket; // Packet to be sent
    int i;

    // If the window is not full, send a new packet
    if (currentWindowCount < WINDOWSIZE)
    {
        if (TRACE > 1)
            printf("----A: New message arrives, send window is not full, send new message to layer3!\n");

        outgoingPacket.seqnum = nextSeqNum; // Set the sequence number
        outgoingPacket.acknum = NOTINUSE;   // Acknowledgment number is not used
        
        // Copy message data into the packet
        for (i = 0; i < 20; i++)
            outgoingPacket.payload[i] = message.data[i];
        outgoingPacket.checksum = ComputeChecksum(outgoingPacket); // Compute the checksum

        windowEnd = (windowEnd + 1) % WINDOWSIZE; // Update the end index of the window
        packetBuffer[windowEnd] = outgoingPacket; // Place the packet in the buffer
        currentWindowCount++; // Increment the count of packets in the window

        if (TRACE > 0)
            printf("Sending packet %d to layer 3\n", outgoingPacket.seqnum);
        tolayer3(A, outgoingPacket); // Send the packet to layer 3

        // Start the timer if this is the first packet in the window
        if (currentWindowCount == 1)
            starttimer(A, RTT);

        // Update the next sequence number
        nextSeqNum = (nextSeqNum + 1) % SEQSPACE;
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
void A_timerinterrupt(void)
{
    if (TRACE > 0)
        printf("----A: time out, resend packets!\n");

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

// Handle received packets
void B_input(struct pkt packet)
{
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
    }
}

// Initialize the receiver
void B_init(void)
{
    expectedSeqNum = 0; // Set the expected sequence number to 0
}

// Output function for the receiver (not implemented)
void B_output(struct msg message)
{
}

// Timer interrupt function for the receiver (not implemented)
void B_timerinterrupt(void)
{
}