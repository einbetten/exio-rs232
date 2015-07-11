#include <msp430f233.h>

void fWrite();
void fRead();
void systemInit();
void pinSetLastState();
void pioSetModeSwitch();
void pioSetModePioDir();
void pioSetMode();
void pioSetValue();
void sendAnswer();

unsigned int i = 0;
char* Flash_ptr = (char *) 0x1000;
char pioData[10];

//volatile int aDone = 1;
volatile int aSize = 0;
volatile int aSend = 0;
volatile char answer[13];
volatile int action = 0;
volatile char toSet[10];
volatile int toSetSize = 0;
volatile int readerState = 0;
volatile int bTx = 0;

void systemInit() {
    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
    if (CALBC1_1MHZ==0xFF) 		              // If calibration constant erased
    {											
        while(1);                             // do not load, trap CPU!!	
    } 
    DCOCTL = 0;                               // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;                    // Set DCO
    DCOCTL = CALDCO_1MHZ;
    P3SEL = 0x30;                             // P3.4,5 = USCI_A0 TXD/RXD
    UCA0CTL1 |= UCSSEL_2;                     // SMCLK
    UCA0BR0 = 104;                            // 1MHz 9600; (104)decimal = 0x068h
    UCA0BR1 = 0;                              // 1MHz 9600
    UCA0MCTL = UCBRS0;                        // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
	IE2 |= UCA0RXIE;                          // Enable USCI_A0 RX interrupt
     
    FCTL2 = FWKEY + FSSEL0 + FN1;             // MCLK/3 for Flash Timing Generator

    __bis_SR_register(GIE);       	         //  interrupts enabled
     
 //   IE2 &= ~UCA0TXIE;

    //GPIO settings
    //AA1 - AA10 (set IN signal in 734 BRU switch, always as output pins)
    P6DIR  = (1<<0) | (1<<1) | (1<<2) | (1<<3) | (1<<4) | (1<<5) | (1<<6) | (1<<7);
    P5DIR  = (1<<0) | (1<<1) ; 
}

int main(void) {

    int offset = 0;
    unsigned char pioIn4;
	unsigned char pioIn5;
	unsigned char pioBit;

    systemInit();
    pinSetLastState();

    for (;;) {
		
		//while ((aDone == 0) || (readerState != 0)){
		//	nop();
		//}
		
		while (readerState != 0){
			nop();
		}

        switch (action){
            case 1: //set modes
                aSize = 0;
                answer[0] = (char)0xFF;
                answer[1] = (char)0x81;
                for (i=0;i<toSetSize;i++){
                    offset = ((toSet[i] & 0xFF )>>4)-1;
                    if (offset < 10){
                        pioData[offset] = (toSet[i] & 0xA);
                        pioSetMode(offset);
                        if ((toSet[i]>>3) & 1){
                            pioSetValue(offset);
                            answer[2+aSize] = toSet[i];
                            aSize++;
                       }
                   }
                }
				fWrite();
                answer[2+aSize] = (char)0xFE;
                aSize+= 3;
                //aSend = 0;
                //aDone = 0;
				action = 0;
                //IE2 |= UCA0TXIE;
                //UCA0TXBUF = answer[aSend];
				sendAnswer();
                break;

            case 2: //set values
                aSize = 0;
                answer[0] = (char)0xFF;
                answer[1] = (char)0x82;
                for (i=0;i<toSetSize;i++){
                    offset = ((toSet[i] & 0xFF )>>4)-1;
                    if (offset < 10){
                        if (((pioData[offset]>>3) & 1) == 0){
							pioData[offset] = (toSet[i] & 0x1);
                            pioSetValue(offset);
                            answer[2+aSize] = toSet[i];
                            aSize++;
                        } else {
                            answer[2+aSize] = toSet[i] | (1<<2);
                            aSize++;
                        }
                    }
                }
				fWrite();
                answer[2+aSize] = (char)0xFE;
                aSize+= 3;
                //aSend = 0;
                //aDone = 0;
				action = 0;
                //IE2 |= UCA0TXIE;
                //UCA0TXBUF = answer[aSend];
				sendAnswer();
                break;

            case 3: //get modes-values
                aSize = 0;
                answer[0] = (char)0xFF;
                answer[1] = (char)0x83;
                for (i=0;i<10;i++){
                    answer[2+aSize] = ((i+1)<<4) | (pioData[i] & 8);
					if (pioData[i] & 8){
						if (!(pioData[i] & 1)){
							answer[2+aSize]+=1;
						}
					} else {
						answer[2+aSize]+=(pioData[i] & 1);
					}
                    aSize++;
                }
                answer[2+aSize] = (char)0xFE;
                aSize+= 3;
                //aSend = 0;
                //aDone = 0;
				action = 0;
                //IE2 |= UCA0TXIE;
                //UCA0TXBUF = answer[aSend];
				sendAnswer();
                break;

            default:
                //read pio and signal if needed
				aSize = 0;
				pioIn4 = P4IN;
				pioIn5 = P5IN;
                for (i=0;i<10;i++){
                    if (((pioData[i]>>3) & 1)){
						//get bit
                        if (i<8){
							pioBit = (pioIn4 >> i) & 1;
                        } else {
							pioBit = (pioIn5 >> (i-6)) & 1;
                        }
						//check
						if (pioBit != (pioData[i] & 1)){
							if (pioBit){
								pioData[i] |= 1; 
							} else {
								pioData[i] -= 1;
							}
							answer[2+aSize] = ((i+1)<<4) + (pioData[i] & 8);
							if (!pioBit){
								answer[2+aSize] += 1;
							}
							aSize++;
						}
                    }
                }
				
				//if was some changes
				if (aSize > 0){
					answer[0] = (char)0xFF;
					answer[1] = (char)0x8E;
					answer[2+aSize] = (char)0xFE;
					aSize+= 3;
					
					//IE2 |= UCA0TXIE;
					//UCA0TXBUF = answer[aSend];
					sendAnswer();
				}
                break;
        }        

    }
}

void sendAnswer(){
	aSend = 0;
	bTx = 0;
	IE2 |= UCA0TXIE;
	while (bTx == 0){
		nop();
	}
	IE2 |= UCA0RXIE;
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void) {
	UCA0TXBUF = answer[aSend];
	aSend++;
	if (aSend == aSize){
		IE2 &= ~UCA0TXIE;
		bTx = 1;
	}
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {
    char recvByte = UCA0RXBUF;
	
	//while(!(IFG2 & UCA0TXIFG));
	
    switch (readerState){
        case 0:
            if (recvByte == (char)0xFF) {
                readerState = 1;
                toSetSize = 0;
            }
            break;

        case 1:
            action = recvByte;
            readerState = 2;
            break;

        case 2:
            if (recvByte != (char)0xFE){
                toSet[toSetSize] = recvByte;
                toSetSize++;
            } else {
                IE2 &= ~UCA0RXIE;
				readerState = 0;
            }
    }
}

void fRead(){
    if (*(Flash_ptr+10) == (char)0xEE) {
        for (i = 0; i < 10; i++)
            pioData[i] = *(Flash_ptr+i);
    } else {
        for (i = 0; i < 10; i++)
            pioData[i] = 0x0;
    }
}

void fWrite(){
    FCTL3 = FWKEY;                            // Clear Lock bit
    FCTL1 = FWKEY + ERASE;                    // Set Erase bit
    *Flash_ptr = 0;                           // Dummy write to erase Flash seg
    FCTL1 = FWKEY + WRT;                      // Set WRT bit for write operation
    for (i = 0; i < 10; i++){
        *(Flash_ptr+i) = (pioData[i] & 0xB);  // Write value to flash
    }
    *(Flash_ptr+i) = 0xEE;
    FCTL1 = FWKEY;                            // Clear WRT bit
    FCTL3 = FWKEY + LOCK;                     // Set LOCK bit
}

void pinSetLastState() {
    fRead();
    for (i=0; i<10;i++){
        pioSetMode(i);
        pioSetValue(i);
    }
}

void pioSetModeSwitch(int index){   
    unsigned char port6Out = P6OUT;
    unsigned char port5Out = P5OUT;
    unsigned char portBit = ((pioData[index] >> 3) & 1);
    switch (index){
        case 0: 
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
			if (portBit == 0){
				port6Out &=~((1<<index));  
			} else {
				port6Out |=((1<<index));
			}
			P6OUT = port6Out;
            break;

        case 8:
        case 9:
			if (portBit == 0){
				port5Out &=~(1<<(index-8));  
			} else {
				port5Out |=(1<<(index-8));
			}
			P5OUT = port5Out;
            break;
    }
}

void pioSetModePioDir(int index){
    unsigned char port4Dir = P4DIR;
    unsigned char port5Dir = P5DIR;
    unsigned char portBit = ((pioData[index] >> 3) & 1);
    switch (index){
        case 0: 
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
			if (portBit == 0){
				port4Dir |= (1<<index);
			} else {
				port4Dir &=~(1<<index); 
			}
			P4DIR = port4Dir;
            break;

        case 8:
        case 9:
			if (portBit == 0){
				port5Dir |= (1<<(index-6));
			} else {
				port5Dir &=~(1<<(index-6)); 
			}
			P5DIR = port5Dir;
            break;
    }
}

void pioSetMode(int index){
    pioSetModeSwitch(index);  
    pioSetModePioDir(index);
}

void pioSetValue(index){
    unsigned char port4Out = P4OUT;
    unsigned char port5Out = P5OUT;
    unsigned char portBit = (pioData[index] & 1);
    switch (index){
        case 0: 
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
			if (portBit == 0){
				port4Out &=~(1<<index);  
			} else {
				port4Out |=(1<<index);
			}
			P4OUT = port4Out;
            break;

        case 8:
        case 9:
			if (portBit == 0){
				port5Out &=~(1<<(index-6));  
			} else {
				port5Out |=(1<<(index-6));
			}
			P5OUT = port5Out;
            break;
    } 
}
