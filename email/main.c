//*****************************************************************************
// Copyright (C) 2014 Texas Instruments Incorporated
//
// All rights reserved. Property of Texas Instruments Incorporated.
// Restricted rights to use, duplicate or disclose this code are
// granted through contract.
// The program may not be used without the written permission of
// Texas Instruments Incorporated or against the terms and conditions
// stipulated in the agreement under which this program has been supplied,
// and under no circumstances can it be used with non-TI connectivity device.
//
//*****************************************************************************
//*****************************************************************************
//
// Application Name     -   Email
// Application Overview -   The email application on the CC3200 sends emails
//							using SMTP (Simple Mail Transfer Protocol). The email
//							application sends a preconfigured email at the push of
//							a button or a user-configured email through the
//							CLI (Command Line Interface)
//
// Application Details  -
// http://processors.wiki.ti.com/index.php/CC32xx_Email_Demo_Application
// or
// docs\examples\CC32xx_Email_Demo_Application.pdf
//
//*****************************************************************************

#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "socket.h"
#include "MQTTPacket.h"

#include "hw_types.h"
#include "hw_memmap.h"
#include "osi.h"
#include "aes.h"
#include "interrupt.h"
#include "prcm.h"
#include "timer.h"
#include "i2c_if.h"
#include "tmp006drv.h"
#include "wdt.h"
#include "wdt_if.h"

// common interface includes

#include "network_if.h"
#include "timer_if.h"
#include "gpio.h"
#include "gpio_if.h"
#include "adc_func.h"
#include "utils.h"
#ifndef NOTERM
#include "uart_if.h"
#endif


#include "pinmux.h"


#define SPAWN_TASK_PRIORITY             (9)
#define OSI_STACK_SIZE					(4000)
///////////////////////////////////////////////////////////////////////////////

#ifndef NOTERM
#define UART_PRINT                              Report
#define DispatcherUartSendPacket                Report
#else
#define UART_PRINT(x,...)                                    
#define DispatcherUartSendPacket(x,...)                          
#endif

#define APP_NAME					"MQTT wolkabout.com"
#define TASK_PRIORITY					(1)

// Default SSID Settings
#define SSID_NAME               "ecguest"      // AP SSID
#define SECURITY_TYPE           SL_SEC_TYPE_WPA  // Security type (OPEN or WEP or WPA)
#define SECURITY_KEY            "execomguest"                // Password of the secured AP
#define MAX_BUFF_SIZE			128

#define COMP_IP				0x0a000008 // computers IP adres (for testing virtual broker)
#define WOLKABOUT_IP		0x25fc7d5a // 37.252.125.90
#define PROBNI_IP			0x68288c3f // 104.40.140.63
#define PORT_NUM            1883	   // port number
#define SUBSCRIBE_TOPIC		"config/016"
#define PUBLISH_TOPIC		"sensors/016"

// AES key and initialization vector
char Key[16] = "no-preshared-key";
unsigned char IV[16] = {0};


static volatile bool g_bContextInIntFlag;
static volatile bool g_bDataInIntFlag;
static volatile bool g_bContextOutIntFlag;
static volatile bool g_bDataOutIntFlag;


// u messageToBeCrypted formira poruka koja se kriptuje, a rezultat se upisuje u cryptedMessage koja se salje na server
char messageToBeCrypted[MAX_BUFF_SIZE] = {0};
char cryptedMessage[MAX_BUFF_SIZE] = {0};
char recievedFromServer[MAX_BUFF_SIZE] = {0};
unsigned char buf[200];
int buflen = sizeof(buf);

char iSocketDesc;
char interupt_flag = 1;
char crypt_flag = 1;
int heartbeat = 10;
long server_rtc = 0;
char messageLength = 0;
int pingSent = 0;
char ResetTimerCount = 0;

int iAddrSize = sizeof(SlSockAddrIn_t);
SlSockAddrIn_t  sAddr;
MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
MQTTString topicString = MQTTString_initializer;

int iSockID;
//*****************************************************************************
// Variable used in Timer Interrupt Handler
//*****************************************************************************
int g_usTimerInts;
//*****************************************************************************
// global variables (UART Buffer, Email Task, Queues Parameters)
//*****************************************************************************
signed char g_cConnectStatus;
//char ucUARTBuffer[200];

/* AP Security Parameters */
SlSecParams_t SecurityParams = {0};

//SlSockAddr_t Server_adress = {0};

//*****************************************************************************
//                      GLOBAL VARIABLES for VECTOR TABLE
//*****************************************************************************
#if defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

//*****************************************************************************
// extern Functions definition
//*****************************************************************************
void conect_to_AP(void);
void PushButtonHandler(void);
void Send_email(void);
void AESCrypt(long direction, char *sourceBuff, char *resultBuff);
void AESIntHandler(void);
signed char Send_MQTT(char *message);
void MQTT_payload_string(char * buff_ptr, int temperature, char battery);
void AESInit(void);
int getdata(unsigned char* buf, int count);
signed char Subscribe_MQTT(void);
signed char receivePublish(void);
void ResetHandler(void);
//static void MainTask(void);


int getdata(unsigned char* buf, int count)
{
	return recv(iSockID, buf, count, 0);
	//return sl_RecvFrom(iSockID, buf, count, 0, (SlSockAddr_t *)&sAddr, (SlSocklen_t*)&iAddrSize);
}

signed char receivePublish(void)
{
	//int iStatus;
	char *ptr = recievedFromServer;
	signed char rc = -1;
	signed char iStatus;

	iStatus = sl_Connect(iSockID, ( SlSockAddr_t *)&sAddr, iAddrSize);
	if( iStatus < 0 )
	{
	// error
		sl_Close(iSockID);
		UART_PRINT("Unable to connect to TCP server\r\n");
		return -1;
	}

	memcpy(buf, 0, buflen);

	if (MQTTPacket_read(buf, buflen, getdata) == PUBLISH)
		{
			//Message("\n\rRecived from server");

			unsigned char dup;
			int qos;
			unsigned char retained;
			unsigned short msgid;
			int payloadlen_in;
			unsigned char* payload_in;
			MQTTString receivedTopic;

			//Message("\n\r..dobio PUBLISH od klijenta, nesto je objavljeno na topiku");

			MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic, &payload_in, &payloadlen_in, buf, buflen);

			//do
			//{
				//Message("\n\r\n\r Decryption in progress....");
				AESCrypt(AES_CFG_DIR_DECRYPT, (char *)payload_in, recievedFromServer);
				messageLength = strlen(recievedFromServer);
				//UART_PRINT("\n\r Decryption done. Recived message lenght: %d\r\n", messageLength);
				UART_PRINT("\n\rRecieved message: %s Message length: %d\n\r", recievedFromServer, messageLength);

				ptr = strtok(recievedFromServer, " ;");
				ptr = strtok(NULL, " ;");
				server_rtc = atoi(ptr);
				ptr = strtok(NULL, " ;");
				ptr = strtok(NULL, " ;");
				heartbeat = atoi(ptr);

				if(heartbeat == 0)
					heartbeat = 1;


			//} while(heartbeat > 600 || server_rtc < 4000000);
			rc = 0;
		}
	crypt_flag = 1;
	return rc;
}

void
AESIntHandler(void)
{
    uint32_t uiIntStatus;

    //
    // Read the AES masked interrupt status.
    //
    uiIntStatus = AESIntStatus(AES_BASE, true);

    //
    // Set Different flags depending on the interrupt source.
    //
    if(uiIntStatus & AES_INT_CONTEXT_IN)
    {
        AESIntDisable(AES_BASE, AES_INT_CONTEXT_IN);
        g_bContextInIntFlag = true;
    }
    if(uiIntStatus & AES_INT_DATA_IN)
    {
        AESIntDisable(AES_BASE, AES_INT_DATA_IN);
        g_bDataInIntFlag = true;
    }
    if(uiIntStatus & AES_INT_CONTEXT_OUT)
    {
        AESIntDisable(AES_BASE, AES_INT_CONTEXT_OUT);
        g_bContextOutIntFlag = true;
    }
    if(uiIntStatus & AES_INT_DATA_OUT)
    {
        AESIntDisable(AES_BASE, AES_INT_DATA_OUT);
        g_bDataOutIntFlag = true;
    }


}
//*****************************************************************************
//
//! AES Crypt Function - function that crypts messageToBeCrypted
//!
//! \param void
//!
//! \return none
//
//*****************************************************************************

void AESCrypt(long direction, char *sourceBuff, char *resultBuff)
{
    //
    // Step1: Reset the Module
    // Step4: Set the Configuration Parameters (Direction,AES Mode and Key Size)
    // Step5: Set the Initialization Vector
    // Step6: Write Key
    // Step7: Start the Crypt Process
    //

	unsigned int uiMsgLen,iSize;
	int iMod=0;

		uiMsgLen = strlen(sourceBuff);
		iSize=uiMsgLen;

		iMod=uiMsgLen%16;
		if(iMod)
		{
			iSize=((uiMsgLen/16)+1)*16;
		}


	g_bContextInIntFlag = false;
	g_bDataInIntFlag = false;
	g_bContextOutIntFlag = false;
	g_bDataOutIntFlag = false;

	//
	// Enable all interrupts.
	//
	AESIntEnable(AES_BASE, AES_INT_CONTEXT_IN | AES_INT_CONTEXT_OUT | AES_INT_DATA_IN | AES_INT_DATA_OUT);

	//
	// Wait for the context in flag, the flag will be set in the Interrupt handler.
	//
/*
	while(!g_bContextInIntFlag)
	{
	}
*/
	PRCMPeripheralReset(PRCM_DTHE);
	AESConfigSet(AES_BASE, (direction | AES_CFG_MODE_CBC | AES_CFG_KEY_SIZE_128BIT));
	AESIVSet(AES_BASE, IV);
	AESKey1Set(AES_BASE,(unsigned char *)Key, AES_CFG_KEY_SIZE_128BIT);

    AESDataProcess(AES_BASE, (unsigned char *)sourceBuff, (unsigned char *)resultBuff, iSize);
}

//*****************************************************************************
//
//! Periodic Timer Interrupt Handler
//!
//! \param None
//!
//! \return None
//
//*****************************************************************************

void
TimerPeriodicIntHandler(void)
{

    unsigned long ulInts;
    ulInts = TimerIntStatus(TIMERA0_BASE, true);
    TimerIntClear(TIMERA0_BASE, ulInts);

   //WatchdogIntClear(WDT_BASE);

    g_usTimerInts++;
    if(g_usTimerInts == (6*heartbeat)-1 )
    	crypt_flag = 1;

    if(g_usTimerInts == (6*heartbeat) )
    {
    	interupt_flag = 1;
    }

    if(g_usTimerInts == (6*heartbeat)+6)
		PRCMSOCReset();
}

//****************************************************************************
//
//! Function to configure and start timer to blink the LED while device is
//! trying to connect to an AP
//!
//! \param none
//!
//! return none
//
//****************************************************************************

void LedTimerConfigNStart()
{
    //
    // Configure Timer for blinking the LED for IP acquisition
    //
    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(TIMERA0_BASE, TIMER_A, TimerPeriodicIntHandler);
    //Timer_IF_Start(TIMERA0_BASE, TIMER_A, (PERIODIC_TEST_CYCLES*3));
}

//****************************************************************************
//
//! Disable the LED blinking Timer as Device is connected to AP
//!
//! \param none
//!
//! return none
//
//****************************************************************************

void LedTimerDeinitStop()
{
    //
    // Disable the LED blinking Timer as Device is connected to AP
    //
    Timer_IF_Stop(TIMERA0_BASE,TIMER_A);
    Timer_IF_DeInit(TIMERA0_BASE,TIMER_A);

}

void ResetHandler(void)
{
	ResetTimerCount++;
	if(ResetTimerCount == 10);
		PRCMSOCReset();
}
//*******************************************************************
//
//	brief 	Push Button Task - postavlja fleg interupt_flag na 1
//
//	\param	pvParameters		-	pointer to the task parameter
//
//	\return void
//	\note
//	\warning
//
//********************************************************************

void PushButtonHandler(void)
{
	if(!interupt_flag)
	{
		unsigned long ulStatus;
		//GPIOIntClear(GPIOA1_BASE, GPIO_INT_PIN_4);
		ulStatus = GPIOIntStatus(GPIOA1_BASE, false);
		GPIOIntClear(GPIOA1_BASE, ulStatus);
		GPIOIntDisable(GPIOA1_BASE, 0x20);
		DBG_PRINT("\nTaster pritisnut\n\r");
		interupt_flag = 1;
		//PRCMSOCReset();
	}
}

//*******************************************************************
//
//	SimpleEmail - glavni task koji se izvrsava, kada se pritisne taster
//				  uzima se temperatura, formira poruka, kriptuje i
//				  salje na server
//
//
//
//*******************************************************************

static
void MainTask(void *pvParameters)
{

	float temp_raw;
	int Temp = 0;
	char battery = 0;
	int brojac = 0;
	char count=0;
	signed char status = 0;

	g_usTimerInts = 0;

	TMP006DrvGetTemp(&temp_raw);
	Temp = (int)(temp_raw*10);
	battery =  batteryVoltage();

	TimerLoadSet(TIMERA0_BASE, TIMER_BOTH, (PERIODIC_TEST_CYCLES * 10));
	TimerEnable(TIMERA0_BASE,TIMER_A);

	// connect to access point
	//do
	//{
		conect_to_AP();
		//status = Subscribe_MQTT();
	//}while(status < 0);

    while(1)
    {
		//Timer_IF_Start(TIMERA0_BASE, TIMER_A, (PERIODIC_TEST_CYCLES*5));

		GPIOIntEnable(GPIOA1_BASE, 0x20);
		while(!interupt_flag)
		{
			if(crypt_flag)
			{
				TMP006DrvGetTemp(&temp_raw);
				Temp = (int)(temp_raw*10);

				battery =  batteryVoltage();
				UART_PRINT("Battery: %d\r\n", battery);

				if(!pingSent)
					MQTT_payload_string(messageToBeCrypted,Temp, battery);

				else
				{
					memset(messageToBeCrypted, 0x00, MAX_BUFF_SIZE);
					strcpy(messageToBeCrypted, "RTC 0000000000;");
				}

				AESCrypt(AES_CFG_DIR_ENCRYPT, messageToBeCrypted, cryptedMessage);

				crypt_flag = 0;
			}
		}

			//Timer_IF_Stop(TIMERA0_BASE, TIMER_A);
			do
			{
				count++;
				if(crypt_flag)
				{
					if(!pingSent)
						MQTT_payload_string(messageToBeCrypted,Temp, battery);

					else
					{
						memset(messageToBeCrypted, 0x00, MAX_BUFF_SIZE);
						strcpy(messageToBeCrypted, "RTC 0000000000;");
					}

					UART_PRINT("Usao u do while\r\n");
					AESCrypt(AES_CFG_DIR_ENCRYPT, messageToBeCrypted, cryptedMessage);
				}

				if(heartbeat > 1 && !crypt_flag)
				{
					status = Subscribe_MQTT();
					while(status < 0)
					{
						sl_Close(iSockID);
						conect_to_AP();
						MQTT_payload_string(messageToBeCrypted,Temp, battery);
						AESCrypt(AES_CFG_DIR_ENCRYPT, messageToBeCrypted, cryptedMessage);
						status = Subscribe_MQTT();
					}
				}

				status = Send_MQTT(cryptedMessage);
				if(status < 0 || count > 5)
				{
					//TimerDisable(TIMERA0_BASE, TIMER_A);
//					TimerLoadSet(TIMERA0_BASE, TIMER_A, (PERIODIC_TEST_CYCLES * 10));
//					TimerEnable(TIMERA0_BASE,TIMER_A);
					//conect_to_AP();
					status = Subscribe_MQTT();
					while(status < 0)
					{
						sl_Close(iSockID);
						conect_to_AP();
						MQTT_payload_string(messageToBeCrypted,Temp, battery);
						AESCrypt(AES_CFG_DIR_ENCRYPT, messageToBeCrypted, cryptedMessage);
						status = Subscribe_MQTT();
					}
					status = Send_MQTT(cryptedMessage);
					count = 0;
					pingSent = 0;
				}
				else
					UtilsDelay((PERIODIC_TEST_CYCLES/4));
					status = receivePublish();
			} while(status < 0 || messageLength < 27 || messageLength > 29);
    		//} while(status < 0 || messageLength < 13 || messageLength > 17);

			count = 0;
			messageLength = 0;
			brojac+=heartbeat;
			UART_PRINT("%d) Message <%s> sent successfuly\r\n", brojac, messageToBeCrypted);
			UART_PRINT("Server RTC: %d\r\nServer HEARTBEAT: %d\r\n", server_rtc, heartbeat);

			if(!pingSent)
				pingSent = heartbeat;

			//pingSent--;
			pingSent = 0;
			interupt_flag = 0;
			crypt_flag = 0;
	    	g_usTimerInts = 0;
			GPIOIntEnable(GPIOA1_BASE, 0x20);
	}
}

//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************

static void
DisplayBanner(char * AppName)
{
    UART_PRINT("\n\n\n\r");
    UART_PRINT("\t\t *************************************************\n\r");
    UART_PRINT("\t\t		CC3200 %s Application       \n\r", AppName);
    UART_PRINT("\t\t *************************************************\n\r");
    UART_PRINT("\n\n\n\r");
}

//*****************************************************************************
//
//! Subscribe_MQTT
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************

signed char Subscribe_MQTT(void)
{
	//unsigned char buf[200];
	//int buflen = sizeof(buf);
	int len = 0;
	int msgid = 1;
	int iStatus;
	int req_qos = 0;

	//filling the data for needed for MQTT
	data.clientID.cstring = "PKrzaID";
	data.keepAliveInterval = 60;
	data.cleansession = 1;
	data.MQTTVersion = 3;

		// creating a TCP socket
		iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, IPPROTO_TCP);
		if( iSockID < 0 )
		{
		// error
			UART_PRINT("Unable to create a TCP socket\r\n");
			return -1;
		}

		SlSockKeepalive_t KAenableOption;// = 1UL;
		KAenableOption.KeepaliveEnabled = 1;
		len = sl_SetSockOpt(iSockID, SOL_SOCKET,SL_SO_KEEPALIVE, &KAenableOption, sizeof(KAenableOption));

		struct SlTimeval_t tv;
		tv.tv_sec = 0;  // 1 second Timeout
		tv.tv_usec = 250000; // half a second
		sl_SetSockOpt(iSockID, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

		SlSockNonblocking_t NBenableOption;
		NBenableOption.NonblockingEnabled = 0;
		sl_SetSockOpt(iSockID,SOL_SOCKET,SL_SO_NONBLOCKING, &NBenableOption,sizeof(NBenableOption));

		// connecting to TCP server
		iStatus = sl_Connect(iSockID, ( SlSockAddr_t *)&sAddr, iAddrSize);
		if( iStatus < 0 )
		{
		// error
			sl_Close(iSockID);
			UART_PRINT("Unable to connect to TCP server\r\n");
			return -1;
		}


	// form a connect message
	len = MQTTSerialize_connect(buf, buflen, &data);
	iStatus = sl_Send(iSockID, buf, len, 0);
	  if( iStatus < 0 )
	  {
		// error
	   return -1;
	  }

	 // recive CONNACK message
	if (MQTTPacket_read(buf, buflen, getdata) == CONNACK)
	{
		unsigned char sessionPresent, connack_rc;

		if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
		{
			Report("Unable to connect, return code %d\n", connack_rc);
			sl_Close(iSockID);
			return -1;
		}
		Message("\n\r..dobio CONNACK od klijenta");
	}
	else
	{
		 sl_Close(iSockID);
		 Message("\n\rERROR:nisam uspeo da se konektujem");
	}

	topicString.cstring = SUBSCRIBE_TOPIC;

	len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);

	iStatus = sl_Send(iSockID, buf, len, 0);
	if (MQTTPacket_read(buf, buflen, getdata) == SUBACK) 	/* wait for suback */
	{
		unsigned short submsgid;
		int subcount;
		int granted_qos;

		Message("\n\r..dobio SUBACK od klijenta");

		iStatus = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, buf, buflen);
		if (granted_qos != 0)
		{
			Report("...granted QoS != 0, %d", granted_qos);
			sl_Close(iSockID);
		}
		Report("\n\r...granted qos: %d", granted_qos);
	}
	else
	{
		 sl_Close(iSockID);
		 Message("\n\rERROR:nisam uspeo da se konektujem");
	}

	Message("\n\r..zavrsio subscribe...\n\r");


	topicString.cstring = PUBLISH_TOPIC;

	return 0;
}
//*****************************************************************************
//
//! MQTT - salje sifrovanu poruku na wolkabout.com pod temom sensors/16
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************

signed char Send_MQTT(char *message)
{
	//unsigned char buf[200];
	//int buflen = sizeof(buf);
	int payloadlen = strlen(message);
	int len = 0;
	int iStatus;


	// form a publish message
	len = MQTTSerialize_publish(buf, buflen, 0, 0, 0, 0, topicString, (unsigned char *)message, payloadlen);
/*
	SlSockKeepalive_t KAenableOption;// = 1UL;
	KAenableOption.KeepaliveEnabled = 1;
	len = sl_SetSockOpt(iSockID, SOL_SOCKET,SL_SO_KEEPALIVE, &KAenableOption, sizeof(KAenableOption));

	struct SlTimeval_t tv;
	tv.tv_sec = 0;   //1 second Timeout
	tv.tv_usec = 500000; // half a second
	sl_SetSockOpt(iSockID, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

	SlSockNonblocking_t NBenableOption;
	NBenableOption.NonblockingEnabled = 0;
	sl_SetSockOpt(iSockID,SOL_SOCKET,SL_SO_NONBLOCKING, &NBenableOption,sizeof(NBenableOption));
*/
	// connecting to TCP server
	iStatus = sl_Connect(iSockID, ( SlSockAddr_t *)&sAddr, iAddrSize);
	if( iStatus < 0 )
	{
	// error
		sl_Close(iSockID);
		UART_PRINT("Unable to connect to TCP server\r\n");
		return -1;
	}

	iStatus = sl_Send(iSockID, buf, len, 0);
	//iStatus =  sl_SendTo(iSockID, buf, len, 0, ( SlSockAddr_t *)&sAddr, iAddrSize);
	  if( iStatus < 0 )
	  {
		// error
	   return -1;
	  }

	  //Message("\n\rPublished");


	  return 0;
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************

static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
   //
   // Set vector table base
   //
#if defined(ccs)
   MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
   MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif

    // Enable Processor
    //
    IntMasterEnable();
    IntEnable(15);

    PRCMCC3200MCUInit();
}


void conect_to_AP(void)
{
	int status;

	//filling the TCP server socket address
	sAddr.sin_family = SL_AF_INET;
	sAddr.sin_port = sl_Htons((unsigned short)1883);
	sAddr.sin_addr.s_addr = sl_Htonl((unsigned int)WOLKABOUT_IP);

	Network_IF_ResetMCUStateMachine();
	//
	// Start the driver
	//
	Network_IF_InitDriver(ROLE_STA);
	GPIO_IF_LedOff(MCU_GREEN_LED_GPIO);

	//
	// Configure Timer for blinking the LED for IP acquisition
	//

	//
	// Connect to the Access Point
	//
	do
	{
		SecurityParams.Key = SECURITY_KEY;
		SecurityParams.KeyLen = strlen(SECURITY_KEY);
		SecurityParams.Type = SECURITY_TYPE;

		status = Network_IF_ConnectAP(SSID_NAME,SecurityParams);
		if(status < 0)
		{
			UART_PRINT("\r\nFailed to connect to AP\r\n");
		}
	}
	while(status < 0);
	//
	// Disable the LED blinking Timer as Device is connected to AP
	//
	//LedTimerDeinitStop();

	GPIO_IF_LedOff(MCU_IP_ALLOC_IND);
}

//*****************************************************************************
//
//! AESInit - initializes AES module
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************

void AESInit(void)
{
    //
    // Reset the Module
    // Set the Configuration Parameters (Direction,AES Mode and Key Size)
    // Set the Initialization Vector
    // Write Key
    //
	PRCMPeripheralReset(PRCM_DTHE);
	AESConfigSet(AES_BASE, (AES_CFG_DIR_ENCRYPT | AES_CFG_KEY_SIZE_128BIT | AES_CFG_MODE_CBC));
	AESIVSet(AES_BASE,IV);
	AESKey1Set(AES_BASE, (unsigned char *)Key, AES_CFG_KEY_SIZE_128BIT);
}

//*****************************************************************************
//
//! MQTT_payload_string
//!
//! \param  pointer to buffer, mesured temperature*10
//!
//! \return None
//
//  string format:
//
// 	RTC <unix timestamp>;CHARGING <0/1>;BATTERY <0-100>;BUFFER U:<unix timestamp>:<temperature*10>;
//*****************************************************************************

void MQTT_payload_string(char * buff_ptr, int temperature, char battery)
{
	char *pointer = buff_ptr;
	unsigned int timestamp = time(NULL);
	char charging = 0;
	//char battery = rand() % 100;
	unsigned int number = timestamp;
	char count = 0;

	strcpy(pointer, "RTC ");
	pointer += strlen("RTC ");

	while(number)
	{
		number /= 10;
		pointer++;
		count++;
	}

	while(timestamp)
	{
		number = timestamp % 10;
		timestamp /= 10;
		--pointer;
		*pointer = number+48; // +48 to get the ASCII value for number
	}
	pointer += count;

	strcpy(pointer, ";CHARGING ");
	pointer += strlen(";CHARGING ");

	*pointer = charging+48;
	pointer++;

	strcpy(pointer, ";BATTERY ");
	pointer += strlen(";BATTERY ");

	if(battery == 100)
	{
		*pointer = 49;
		pointer++;
		*pointer = 48;
		pointer++;
		*pointer = 48;
		pointer++;
	}
	else if(battery < 100 && battery >9)
	{
		pointer ++;
		*pointer = (battery % 10)+48;
		pointer--;
		battery /= 10;
		*pointer = (battery % 10)+48;
		pointer += 2;
	}
	else if(battery < 10)
	{
		*pointer = battery;
		pointer++;
	}

	strcpy(pointer, ";BUFFER U:");
	pointer += strlen(";BUFFER U:");

	count = 0;
	timestamp = time(NULL);
	number = timestamp;
	while(number)
		{
			number /= 10;
			pointer++;
			count++;
		}

		while(timestamp)
		{
			number = timestamp % 10;
			timestamp /= 10;
			--pointer;
			*pointer = number+48;
		}
		pointer += count;
		strcpy(pointer, ":");
		pointer+=3;

		*pointer = (temperature % 10)+48;
		pointer--;
		temperature /= 10;
		*pointer = (temperature % 10)+48;
		pointer--;
		temperature /= 10;
		*pointer = (temperature % 10)+48;
		pointer+=3;
		strcpy(pointer, ";");
}

//*****************************************************************************
//
//! Main function to start execution 
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************

void main()
{
	//tBoolean bRetcode;

	BoardInit();

	//
	// Pinmuxing for GPIO,UART
	//
	PinMuxConfig();
	GPIOIntRegister(GPIOA1_BASE, PushButtonHandler);


	// Set up the timer
	PRCMPeripheralReset(PRCM_TIMERA0);
	TimerConfigure(TIMERA0_BASE, TIMER_CFG_PERIODIC);
	TimerPrescaleSet(TIMERA0_BASE, TIMER_A, 0);
	TimerIntRegister(TIMERA0_BASE, TIMER_A, TimerPeriodicIntHandler);


/*
	PRCMPeripheralClkEnable(PRCM_WDT, PRCM_RUN_MODE_CLK);
	//WatchdogUnlock(WDT_BASE);
	WatchdogUnlock(WDT_BASE);
	WatchdogIntRegister(WDT_BASE,TimerPeriodicIntHandler);
	WatchdogIntEnable(WDT_BASE);
	WatchdogStallEnable(WDT_BASE);
	WatchdogReloadSet(WDT_BASE, (PERIODIC_TEST_CYCLES * 10));
	WatchdogEnable(WDT_BASE);
	//bRetcode = WatchdogRunning(WDT_BASE);
*/

/*
	PRCMPeripheralReset(PRCM_TIMERA1);
	TimerConfigure(TIMERA1_BASE, TIMER_CFG_PERIODIC);
	TimerPrescaleSet(TIMERA1_BASE, TIMER_A, 0);
	TimerIntRegister(TIMERA1_BASE, TIMER_A, ResetHandler);
	TimerLoadSet(TIMERA1_BASE, TIMER_A, (PERIODIC_TEST_CYCLES * 10));
	TimerEnable(TIMERA1_BASE,TIMER_A);
*/
	//AESInit();


	LedTimerConfigNStart();


	//UDMAInit();
	//
	// configure LEDs
	//
	//GPIO_IF_LedConfigure(LED1|LED2|LED3);

	//GPIO_IF_LedOff(MCU_ALL_LED_IND);

	AESIntRegister(AES_BASE, AESIntHandler);


#ifndef NOTERM  
	//
	// Configuring UART
	//
	InitTerm();
#endif

	// Initialize I2C drivers & temperature sensor

	I2C_IF_Open(I2C_MASTER_MODE_FST);
	TMP006DrvOpen();
	initADC();
	//
	// Display Welcome Message
	//
	DisplayBanner(APP_NAME);
	//conect_to_AP();
	// Initialize AP security params
	//SecurityParams.Key = SECURITY_KEY;
	//SecurityParams.KeyLen = strlen(SECURITY_KEY);
	//SecurityParams.Type = SECURITY_TYPE;

	//
	// Simplelinkspawntask
	//
	VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);

	osi_TaskCreate(MainTask, (signed char*)"Main Task", OSI_STACK_SIZE, NULL, 1, NULL );

	osi_start();
	//conect_to_AP();
	//MainTask();

	while(1)
	{

	}

}
