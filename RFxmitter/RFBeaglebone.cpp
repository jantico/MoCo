/*
RFBeaglebone.cpp

BBB side.  Transmitter
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>


// Maurice Ribble 
// 8-30-2009
// http://www.glacialwanderer.com/hobbyrobotics
// Used Arduino 0017
// This is a simple test app for some cheap RF transmitter and receiver hardware.
// RF Transmitter: http://www.sparkfun.com/commerce/product_info.php?products_id=8945
// RF Receiver: http://www.sparkfun.com/commerce/product_info.php?products_id=8948

// This says whether you are building the transmistor or reciever.
// Only one of these should be defined.

// Arduino digital pins
#define BUTTON_PIN  2
#define LED_PIN     13

// Button hardware is setup so the button goes LOW when pressed
#define BUTTON_PRESSED LOW
#define BUTTON_NOT_PRESSED HIGH


// Maurice Ribble 
// 8-30-2009
// http://www.glacialwanderer.com/hobbyrobotics
// Used Arduino 0017
// This does does some error checking to try to make sure the receiver on this one way RF 
//  serial link doesn't repond to garbage

#define NETWORK_SIG_SIZE 3

#define VAL_SIZE         2
#define CHECKSUM_SIZE    1
#define PACKET_SIZE      (NETWORK_SIG_SIZE + VAL_SIZE + CHECKSUM_SIZE)

// The network address byte and can be change if you want to run different devices in proximity to each other without interfearance
#define NET_ADDR 5

const char g_network_sig[NETWORK_SIG_SIZE] = {0x8F, 0xAA, NET_ADDR};  // Few bytes used to initiate a transfer


/////////////////////////////////////////////////////////////////////////////////////


// Sends an unsigned int over the RF network
void writeUInt(int file, unsigned int val)
{
  char checksum = (val/256) ^ (val&0xFF);
  int count,num;
  num = 0;
  char syncVal = 0xF0;
  char perr[80];

  count = write(file, &syncVal, sizeof(syncVal));  // This gets reciever in sync with transmitter
  if (count < 0) num = num + 1;
  count = write(file, g_network_sig, NETWORK_SIG_SIZE);
  if (count < 0) num = num + 2;
  count = write(file, (unsigned char*)&val, VAL_SIZE);
  if (count < 0) num = num + 3;
  count = write(file, &checksum,sizeof(checksum)); //CHECKSUM_SIZE
  if (count < 0) num = num + 4;
  if (num > 0) {
  	sprintf(perr,"Failed to write to the output: %d \n",num);
  	perror(perr);
  }
}




int main(int argc, char *argv[]) {
   int file, count;
   

   if ((file = open("/dev/tty4", O_RDWR | O_NOCTTY | O_NDELAY))<0){
      perror("UART: Failed to open the file.\n");
      return -1;
   }
   struct termios options;
   tcgetattr(file, &options);
   options.c_cflag = B9600 | CS8 | CLOCAL;
   options.c_iflag = IGNPAR | ICRNL;
   tcflush(file, TCIFLUSH);
   tcsetattr(file, TCSANOW, &options);

   for (unsigned int i = 1; i<=200; i++) {
	   // send the string plus the null character
	   writeUInt(file,i);
	   usleep(10000);	
   }
   
   
   close(file);
   return 0;
}
