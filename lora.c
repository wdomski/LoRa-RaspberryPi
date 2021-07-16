/*******************************************************************************
 *
 * Author: Wojciech Domski
 * Email: Wojciech.Domski@gmail.com
 *
 * WWW: domski.pl
 *
 * Available as Python module
 * Fixed retaining of bandwidth, code ratio and spread factor settings 
 *
 * originally based on: https://github.com/dragino/rpi-lora-tranceiver
 *
 *
 *******************************************************************************/

#ifdef PYTHONMODULE
#include <Python.h>
#endif

#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/ioctl.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>


// #############################################
// #############################################

#define REG_FIFO                    0x00
#define REG_OPMODE                  0x01
#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_AD         0x0E
#define REG_FIFO_RX_BASE_AD         0x0F
#define REG_RX_NB_BYTES             0x13
#define REG_FIFO_RX_CURRENT_ADDR    0x10
#define REG_IRQ_FLAGS               0x12
#define REG_DIO_MAPPING_1           0x40
#define REG_DIO_MAPPING_2           0x41
#define REG_MODEM_CONFIG            0x1D
#define REG_MODEM_CONFIG2           0x1E
#define REG_MODEM_CONFIG3           0x26
#define REG_SYMB_TIMEOUT_LSB  		0x1F
#define REG_PKT_SNR_VALUE			0x19
#define REG_PAYLOAD_LENGTH          0x22
#define REG_IRQ_FLAGS_MASK          0x11
#define REG_MAX_PAYLOAD_LENGTH 		0x23
#define REG_HOP_PERIOD              0x24
#define REG_SYNC_WORD				0x39
#define REG_VERSION	  				0x42

#define PAYLOAD_LENGTH              0x40

// LOW NOISE AMPLIFIER
#define REG_LNA                     0x0C
#define LNA_MAX_GAIN                0x23
#define LNA_OFF_GAIN                0x00
#define LNA_LOW_GAIN		    	0x20

#define RegDioMapping1                             0x40 // common
#define RegDioMapping2                             0x41 // common

#define RegPaConfig                                0x09 // common
#define RegPaRamp                                  0x0A // common
#define RegPaDac                                   0x5A // common

#define SX72_MC2_FSK                0x00
#define SX72_MC2_SF7                0x70
#define SX72_MC2_SF8                0x80
#define SX72_MC2_SF9                0x90
#define SX72_MC2_SF10               0xA0
#define SX72_MC2_SF11               0xB0
#define SX72_MC2_SF12               0xC0

#define SX72_MC1_LOW_DATA_RATE_OPTIMIZE  0x01 // mandated for SF11 and SF12

// sx1276 RegModemConfig1
#define SX1276_MC1_BW_125                0x70
#define SX1276_MC1_BW_250                0x80
#define SX1276_MC1_BW_500                0x90
#define SX1276_MC1_CR_4_5            0x02
#define SX1276_MC1_CR_4_6            0x04
#define SX1276_MC1_CR_4_7            0x06
#define SX1276_MC1_CR_4_8            0x08

#define SX1276_MC1_IMPLICIT_HEADER_MODE_ON    0x01

// sx1276 RegModemConfig2
#define SX1276_MC2_RX_PAYLOAD_CRCON        0x04

// sx1276 RegModemConfig3
#define SX1276_MC3_LOW_DATA_RATE_OPTIMIZE  0x08
#define SX1276_MC3_AGCAUTO                 0x04

// preamble for lora networks (nibbles swapped)
#define LORA_MAC_PREAMBLE                  0x34

#define RXLORA_RXMODE_RSSI_REG_MODEM_CONFIG1 0x0A
#ifdef LMIC_SX1276
#define RXLORA_RXMODE_RSSI_REG_MODEM_CONFIG2 0x70
#elif LMIC_SX1272
#define RXLORA_RXMODE_RSSI_REG_MODEM_CONFIG2 0x74
#endif

// FRF
#define        REG_FRF_MSB              0x06
#define        REG_FRF_MID              0x07
#define        REG_FRF_LSB              0x08

#define        FRF_MSB                  0xD9 // 868.1 Mhz
#define        FRF_MID                  0x06
#define        FRF_LSB                  0x66

// ----------------------------------------
// Constants for radio registers
#define OPMODE_LORA      0x80
#define OPMODE_MASK      0x07
#define OPMODE_SLEEP     0x00
#define OPMODE_STANDBY   0x01
#define OPMODE_FSTX      0x02
#define OPMODE_TX        0x03
#define OPMODE_FSRX      0x04
#define OPMODE_RX        0x05
#define OPMODE_RX_SINGLE 0x06
#define OPMODE_CAD       0x07

// ----------------------------------------
// Bits masking the corresponding IRQs from the radio
#define IRQ_LORA_RXTOUT_MASK 0x80
#define IRQ_LORA_RXDONE_MASK 0x40
#define IRQ_LORA_CRCERR_MASK 0x20
#define IRQ_LORA_HEADER_MASK 0x10
#define IRQ_LORA_TXDONE_MASK 0x08
#define IRQ_LORA_CDDONE_MASK 0x04
#define IRQ_LORA_FHSSCH_MASK 0x02
#define IRQ_LORA_CDDETD_MASK 0x01

// DIO function mappings                D0D1D2D3
#define MAP_DIO0_LORA_RXDONE   0x00  // 00------
#define MAP_DIO0_LORA_TXDONE   0x40  // 01------
#define MAP_DIO1_LORA_RXTOUT   0x00  // --00----
#define MAP_DIO1_LORA_NOP      0x30  // --11----
#define MAP_DIO2_LORA_NOP      0xC0  // ----11--

// #############################################
// #############################################
//
typedef bool boolean;
typedef unsigned char byte;

static const int CHANNEL = 0;

char message[256];

bool sx1272 = true;

byte receivedbytes;

enum sf_t { SF7=7, SF8, SF9, SF10, SF11, SF12 };

/*******************************************************************************
 *
 * Configure these values!
 *
 *******************************************************************************/

// SX1272 - Raspberry connections
int ssPin = 6;
int dio0  = 7;
int RST   = 0;

// Set spreading factor (SF7 - SF12)
sf_t lora_sf = SF7;

// Set center frequency
uint32_t  lora_freq = 869000000; // in Mhz! (868.1)

#define LORA_TRANSMITTER		0
#define LORA_RECEIVER			1
byte lora_mode = LORA_RECEIVER;

byte hello[32] = "HELLO";

void die(const char *s)
{
	perror(s);
	exit(1);
}

void selectreceiver()
{
	digitalWrite(ssPin, LOW);
}

void unselectreceiver()
{
	digitalWrite(ssPin, HIGH);
}

byte readReg(byte addr)
{
	unsigned char spibuf[2];

	selectreceiver();
	spibuf[0] = addr & 0x7F;
	spibuf[1] = 0x00;
	wiringPiSPIDataRW(CHANNEL, spibuf, 2);
	unselectreceiver();

	return spibuf[1];
}

void writeReg(byte addr, byte value)
{
	unsigned char spibuf[2];

	spibuf[0] = addr | 0x80;
	spibuf[1] = value;
	selectreceiver();
	wiringPiSPIDataRW(CHANNEL, spibuf, 2);

	unselectreceiver();
}

static void opmode (uint8_t mode) {
	writeReg(REG_OPMODE, (readReg(REG_OPMODE) & ~OPMODE_MASK) | mode);
}

static void opmodeLora() {
	uint8_t u = OPMODE_LORA;
	if (sx1272 == false)
		u |= 0x8;   // TBD: sx1276 high freq
	writeReg(REG_OPMODE, u);
}


void SetupLoRa(int freq, int sf)
{

	digitalWrite(RST, HIGH);
	delay(100);
	digitalWrite(RST, LOW);
	delay(100);

	byte version = readReg(REG_VERSION);

	if (version == 0x22) {
		// sx1272
		// printf("SX1272 detected, starting.\n");
		sx1272 = true;
	} else {
		// sx1276?
		digitalWrite(RST, LOW);
		delay(100);
		digitalWrite(RST, HIGH);
		delay(100);
		version = readReg(REG_VERSION);
		if (version == 0x12) {
			// sx1276
			// printf("SX1276 detected, starting.\n");
			sx1272 = false;
		} else {
			// printf("Unrecognized transceiver.\n");
			//printf("Version: 0x%x\n",version);
			exit(1);
		}
	}

	opmode(OPMODE_SLEEP);
	delay(15);
	// entry LoRa mode Required to Bandwidth, Coding Rate, Spread Factor
	opmodeLora();  

	// set frequency
	uint64_t frf = ((uint64_t)freq << 19) / 32000000;
	writeReg(REG_FRF_MSB, (uint8_t)(frf>>16) );
	writeReg(REG_FRF_MID, (uint8_t)(frf>> 8) );
	writeReg(REG_FRF_LSB, (uint8_t)(frf>> 0) );

	writeReg(REG_SYNC_WORD, 0x34); // LoRaWAN public sync word

	if (sx1272) {
		if (sf == SF11 || sf == SF12) {
			writeReg(REG_MODEM_CONFIG,0x0B);
		} else {
			writeReg(REG_MODEM_CONFIG,0x0A);
		}
		writeReg(REG_MODEM_CONFIG2,(sf<<4) | 0x04);
	} else {
		if (sf == SF11 || sf == SF12) {
			writeReg(REG_MODEM_CONFIG3,0x0C);
		} else {
			writeReg(REG_MODEM_CONFIG3,0x04);
		}
		writeReg(REG_MODEM_CONFIG,0x72);
		writeReg(REG_MODEM_CONFIG2,(sf<<4) | 0x04);
	}

	if (sf == SF10 || sf == SF11 || sf == SF12) {
		writeReg(REG_SYMB_TIMEOUT_LSB,0x05);
	} else {
		writeReg(REG_SYMB_TIMEOUT_LSB,0x08);
	}
	writeReg(REG_MAX_PAYLOAD_LENGTH,0x80);
	writeReg(REG_PAYLOAD_LENGTH,PAYLOAD_LENGTH);
	writeReg(REG_HOP_PERIOD,0xFF);
	writeReg(REG_FIFO_ADDR_PTR, readReg(REG_FIFO_RX_BASE_AD));

	writeReg(REG_LNA, LNA_MAX_GAIN);
}

boolean receive(char *payload) {
	// clear rxDone
	writeReg(REG_IRQ_FLAGS, 0x40);

	int irqflags = readReg(REG_IRQ_FLAGS);

	//  payload crc: 0x20
	if((irqflags & 0x20) == 0x20)
	{
		//printf("CRC error\n");
		writeReg(REG_IRQ_FLAGS, 0x20);
		return false;
	} else {

		byte currentAddr = readReg(REG_FIFO_RX_CURRENT_ADDR);
		byte receivedCount = readReg(REG_RX_NB_BYTES);
		receivedbytes = receivedCount;

		writeReg(REG_FIFO_ADDR_PTR, currentAddr);

		for(int i = 0; i < receivedCount; i++)
		{
			payload[i] = (char)readReg(REG_FIFO);
		}
	}
	return true;
}

int receive2(char *payload, int * receivedbytes) {
	// clear rxDone
	writeReg(REG_IRQ_FLAGS, 0x40);

	int irqflags = readReg(REG_IRQ_FLAGS);

	//  payload crc: 0x20
	if((irqflags & 0x20) == 0x20)
	{
		printf("CRC error\n");
		writeReg(REG_IRQ_FLAGS, 0x20);
		*receivedbytes = 0;
		return -1;
	} else {

		byte currentAddr = readReg(REG_FIFO_RX_CURRENT_ADDR);
		byte receivedCount = readReg(REG_RX_NB_BYTES);
		*receivedbytes = receivedCount;

		writeReg(REG_FIFO_ADDR_PTR, currentAddr);

		for(int i = 0; i < receivedCount; i++)
		{
			payload[i] = (char)readReg(REG_FIFO);
		}
	}
	return 0;
}

void receivepacket() {

	long int SNR;
	int rssicorr;

	if(digitalRead(dio0) == 1)
	{
		if(receive(message)) {
			byte value = readReg(REG_PKT_SNR_VALUE);
			if( value & 0x80 ) // The SNR sign bit is 1
			{
				// Invert and divide by 4
				value = ( ( ~value + 1 ) & 0xFF ) >> 2;
				SNR = -value;
			}
			else
			{
				// Divide by 4
				SNR = ( value & 0xFF ) >> 2;
			}

			if (sx1272) {
				rssicorr = 139;
			} else {
				rssicorr = 157;
			}

			printf("Packet RSSI: %d, ", readReg(0x1A)-rssicorr);
			printf("RSSI: %d, ", readReg(0x1B)-rssicorr);
			printf("SNR: %li, ", SNR);
			printf("Length: %i", (int)receivedbytes);
			printf("\n");
			printf("Payload: %s\n", message);

		}
	}
}

void receivepacket2(char * buffer, int * receivedbytes, int * prssi, int * rssi, long int *snr, int *error) {

	int rssicorr;
	int ret;

	*error = 0;

	if(digitalRead(dio0) == 1)
	{
		ret = receive2(buffer, receivedbytes);
		if(ret == 0){
			byte value = readReg(REG_PKT_SNR_VALUE);
			if( value & 0x80 ) // The SNR sign bit is 1
			{
				// Invert and divide by 4
				value = ( ( ~value + 1 ) & 0xFF ) >> 2;
				*snr = -value;
			}
			else
			{
				// Divide by 4
				*snr = ( value & 0xFF ) >> 2;
			}

			if (sx1272) {
				rssicorr = 139;
			} else {
				rssicorr = 157;
			}

			*prssi =  readReg(0x1A)-rssicorr;
			*rssi = readReg(0x1B)-rssicorr;
		}
		else {
			//CRC error
			*error = ret;
		}
	}
	else{
		*receivedbytes = 0;
		*prssi = 0;
		*rssi = 0;
		*snr = 0;
	}
}

static void configPower (int8_t pw) {
	if (sx1272 == false) {
		// no boost used for now
		if(pw >= 17) {
			pw = 15;
		} else if(pw < 2) {
			pw = 2;
		}
		// check board type for BOOST pin
		writeReg(RegPaConfig, (uint8_t)(0x80|(pw&0xf)));
		writeReg(RegPaDac, readReg(RegPaDac)|0x4);

	} else {
		// set PA config (2-17 dBm using PA_BOOST)
		if(pw > 17) {
			pw = 17;
		} else if(pw < 2) {
			pw = 2;
		}
		writeReg(RegPaConfig, (uint8_t)(0x80|(pw-2)));
	}
}


static void writeBuf(byte addr, byte *value, byte len) {                                                       
	unsigned char spibuf[256];                                                                          
	spibuf[0] = addr | 0x80;                                                                            
	for (int i = 0; i < len; i++) {                                                                         
		spibuf[i + 1] = value[i];                                                                       
	}                                                                                                   
	selectreceiver();                                                                                   
	wiringPiSPIDataRW(CHANNEL, spibuf, len + 1);                                                        
	unselectreceiver();                                                                                 
}

void txlora(byte *frame, byte datalen) {
	writeReg(REG_HOP_PERIOD,0x00);
	// set the IRQ mapping DIO0=TxDone DIO1=NOP DIO2=NOP
	writeReg(RegDioMapping1, MAP_DIO0_LORA_TXDONE|MAP_DIO1_LORA_NOP|MAP_DIO2_LORA_NOP);
	// clear all radio IRQ flags
	writeReg(REG_IRQ_FLAGS, 0xFF);
	// mask all IRQs but TxDone
	writeReg(REG_IRQ_FLAGS_MASK, ~IRQ_LORA_TXDONE_MASK);

	// initialize the payload size and address pointers
	writeReg(REG_FIFO_TX_BASE_AD, 0x00);
	writeReg(REG_FIFO_ADDR_PTR, 0x00);
	writeReg(REG_PAYLOAD_LENGTH, datalen);

	// download buffer to the radio FIFO
	writeBuf(REG_FIFO, frame, datalen);
	// now we actually start the transmission
	opmode(OPMODE_TX);
}

int main (int argc, char *argv[]) {
	if (argc < 2) {
		printf ("Usage: argv[0] sender|rec [message]\n");
		exit(1);
	}

	wiringPiSetup () ;
	pinMode(ssPin, OUTPUT);
	pinMode(dio0, INPUT);
	pinMode(RST, OUTPUT);

	wiringPiSPISetup(CHANNEL, 500000);

	SetupLoRa(lora_freq, lora_sf);

	if (!strcmp("sender", argv[1])) {
		opmodeLora();
		// enter standby mode (required for FIFO loading))
		opmode(OPMODE_STANDBY);

		writeReg(RegPaRamp, (readReg(RegPaRamp) & 0xF0) | 0x08); // set PA ramp-up time 50 uSec

		configPower(23);

		printf("Send packets at SF%i on %.6lf Mhz.\n", lora_sf,(double)lora_freq/1000000);
		printf("------------------\n");

		if (argc > 2)
			strncpy((char *)hello, argv[2], sizeof(hello));

		while(1) {
			txlora(hello, strlen((char *)hello));
			delay(5000);
		}
	} else {
		// radio init
		opmodeLora();
		opmode(OPMODE_STANDBY);
		opmode(OPMODE_RX);
		printf("Listening at SF%i on %.6lf Mhz.\n", lora_sf,(double)lora_freq/1000000);
		printf("------------------\n");
		while(1) {
			receivepacket(); 
			delay(1);
		}
	}

	return (0);
}

#ifdef PYTHONMODULE

static PyObject * LoraError;

static PyObject* send(PyObject* self, PyObject* args)
{
	char * buffer;
	int size;

	if (!PyArg_ParseTuple(args, "y#", &buffer, &size)){
		return NULL;
	}

	txlora((byte *)buffer, size);

	return PyLong_FromLong(0);
}


static PyObject* recv(PyObject* self, PyObject* args)
{
	char buffer[256];
	int receivedbytes = 0;
	int prssi = 0;
	int rssi = 0;
	long int snr = 0;
	int error = 0;

	receivepacket2(buffer, &receivedbytes, &prssi, &rssi, &snr, &error);

	return Py_BuildValue("y#iiiii", buffer, receivedbytes, receivedbytes, prssi, rssi, snr, error);
}


static PyObject* init(PyObject* self, PyObject* args)
{
	int mode;
	int freq;
	int sf;

	if (!PyArg_ParseTuple(args, "iii", &mode, &freq, &sf))
		return NULL;


	if(!(mode == 0 || mode == 1))
	{
		printf("Bad mode. Should be 0 (sender) or 1 (receiver) \r\n");
		PyErr_SetString(LoraError, "Bad mode. Should be 0 (sender) or 1 (receiver)");
		return NULL;
	}

	if((sf < 7) || (sf > 12))
	{
		printf("Bad spread factor. Should be 7, 8, 9, 10, 11 or 12\r\n");
		PyErr_SetString(LoraError, "Bad spread factor. Should be 7, 8, 9, 10, 11 or 12");
		return NULL;
	}

	//setup GPIO
	wiringPiSetup();
	pinMode(ssPin, OUTPUT);
	pinMode(dio0, INPUT);
	pinMode(RST, OUTPUT);

	//set up SPI
	wiringPiSPISetup(CHANNEL, 500000);
	SetupLoRa(freq, sf);

	//sender
	if (mode == 0) {
		opmodeLora();
		// enter standby mode (required for FIFO loading))
		opmode(OPMODE_STANDBY);

		writeReg(RegPaRamp, (readReg(RegPaRamp) & 0xF0) | 0x08); // set PA ramp-up time 50 uSec

		configPower(23);

	} else {
		// radio init
		opmodeLora();
		opmode(OPMODE_STANDBY);
		opmode(OPMODE_RX);
	}

	lora_freq = freq;
	lora_sf = (sf_t)sf;
	lora_mode = mode;

	return PyLong_FromLong(0);
}

static PyObject* mode(PyObject* self, PyObject* args)
{
	int mode;

	if (!PyArg_ParseTuple(args, "i", &mode))
		return NULL;


	if(!(mode == 0 || mode == 1))
	{
		printf("Bad mode. Should be 0 (sender) or 1 (receiver) \r\n");
		PyErr_SetString(LoraError, "Bad mode. Should be 0 (sender) or 1 (receiver)");
		return NULL;
	}

	SetupLoRa(lora_freq, lora_sf);

	//sender
	if (mode == 0) {
		opmodeLora();
		// enter standby mode (required for FIFO loading))
		opmode(OPMODE_STANDBY);

		writeReg(RegPaRamp, (readReg(RegPaRamp) & 0xF0) | 0x08); // set PA ramp-up time 50 uSec

		configPower(23);

	} else {
		// radio init
		opmodeLora();
		opmode(OPMODE_STANDBY);
		opmode(OPMODE_RX);
	}

	lora_mode = mode;

	return PyLong_FromLong(0);
}


static PyMethodDef LoraMethods[] = {
	{"init",  init, METH_VARARGS, "Initialization"},
	{"send",  send, METH_VARARGS, "Send data"},
	{"recv",  recv, METH_VARARGS, "Receive data"},
	{"mode",  mode, METH_VARARGS, "Set mode"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef loramodule = {
	PyModuleDef_HEAD_INIT,
	"lora",   /* name of module */
	NULL, /* module documentation, may be NULL */
	-1,       /* size of per-interpreter state of the module,
				 or -1 if the module keeps state in global variables. */
	LoraMethods
};

	PyMODINIT_FUNC
PyInit_loralib(void)
{
	PyObject *m;

	m = PyModule_Create(&loramodule);

	LoraError = PyErr_NewException("lora.error", NULL, NULL);
	Py_INCREF(LoraError);
	PyModule_AddObject(m, "error", LoraError);

	return m;
}

#endif
