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


// simplelink includes
//#include "datatypes.h"
#include "socket.h"
#include "MQTTPacket.h"

// driverlib includes
//#include "hw_aes.h"
//#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "osi.h"
//#include "rom.h"
//#include "rom_map.h"
#include "aes.h"
//#include "pin.h"
//#include "utils.h"
#include "interrupt.h"
#include "prcm.h"
#include "timer.h"
//#include "aes_vector.h"
//#include "aes_userinput.h"
#include "i2c_if.h"
#include "tmp006drv.h"

// common interface includes

#include "network_if.h"
#include "timer_if.h"
#include "gpio.h"
#include "gpio_if.h"
//#include "button_if.h"
#ifndef NOTERM
#include "uart_if.h"
#endif


#include "pinmux.h"
//#include "email.h"
//#include "demo_config.h"
//#include "udma_if.h"



#define SPAWN_TASK_PRIORITY             (9)
#define OSI_STACK_SIZE					(3000)
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

#define COMP_IP				0x0a000008
#define WOLKABOUT_IP		0x25fc7d5a // 37.252.125.90
#define PORT_NUM            1883

// AES key and initialization vector
char Key[16] = "no-preshared-key";
unsigned char IV[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
static volatile bool g_bContextInIntFlag;
static volatile bool g_bDataInIntFlag;
static volatile bool g_bContextOutIntFlag;
static volatile bool g_bDataOutIntFlag;
*/
// u pcEmailmessage formira poruka koja se kriptuje, a rezultat se upisuje u pcRecivemessage koja se salje na server
char pcEmailmessage[MAX_BUFF_SIZE];
char pcRecivemessage[MAX_BUFF_SIZE];

char iSocketDesc;
char interupt_flag = 0;
//*****************************************************************************
// Variable used in Timer Interrupt Handler
//*****************************************************************************
unsigned short g_usTimerInts;
//*****************************************************************************
// global variables (UART Buffer, Email Task, Queues Parameters)
//*****************************************************************************
signed char g_cConnectStatus;
unsigned int uiUartCmd;
//char ucUARTBuffer[200];

/* AP Security Parameters */
SlSecParams_t SecurityParams = {0};


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
void AESCrypt(void);
//void AESIntHandler(void);
signed char Send_MQTT(void);
void MQTT_payload_string(char * buff_ptr, int temperature);
void AESInit(void);

/*
void
AESIntHandler(void)
{
    uint32_t uiIntStatus;

    //
    // Read the AES masked interrupt status.
    //
    uiIntStatus = MAP_AESIntStatus(AES_BASE, true);

    //
    // Set Different flags depending on the interrupt source.
    //
    if(uiIntStatus & AES_INT_CONTEXT_IN)
    {
        MAP_AESIntDisable(AES_BASE, AES_INT_CONTEXT_IN);
        g_bContextInIntFlag = true;
    }
    if(uiIntStatus & AES_INT_DATA_IN)
    {
        MAP_AESIntDisable(AES_BASE, AES_INT_DATA_IN);
        g_bDataInIntFlag = true;
    }
    if(uiIntStatus & AES_INT_CONTEXT_OUT)
    {
        MAP_AESIntDisable(AES_BASE, AES_INT_CONTEXT_OUT);
        g_bContextOutIntFlag = true;
    }
    if(uiIntStatus & AES_INT_DATA_OUT)
    {
        MAP_AESIntDisable(AES_BASE, AES_INT_DATA_OUT);
        g_bDataOutIntFlag = true;
    }


}*/
//*****************************************************************************
//
//! AES Crypt Function
//!
//! This function Configures Key,Mode and carries out Encryption/Decryption in
//!                                                  CPU mode
//! \param uiConfig - Configuration Value
//! \param uiKeySize - KeySize used.(128,192 or 256 bit)
//! \param puiKey1 - Key Used
//! \param puiData - Input Data
//! \param puiResult - Resultant Output Data
//! \param uiDataLength - Input Data Length
//! \param uiIV - Initialization Vector used
//!
//! \return none
//
//*****************************************************************************

void AESCrypt(void)
{
    //
    // Step1: Reset the Module
    // Step2: Enable Interrupts
    // Step3: Wait for Context Ready Inteerupt
    // Step4: Set the Configuration Parameters (Direction,AES Mode and Key Size)
    // Step5: Set the Initialization Vector
    // Step6: Write Key
    // Step7: Start the Crypt Process
    //

	unsigned int uiMsgLen,iSize;
	int iMod=0;

	uiMsgLen = strlen(pcEmailmessage);
	iSize=uiMsgLen;

    iMod=uiMsgLen%16;
    if(iMod)
    {
	    iSize=((uiMsgLen/16)+1)*16;
    }

    PRCMPeripheralReset(PRCM_DTHE);

    AESDataProcess(AES_BASE, (unsigned char *)pcEmailmessage, (unsigned char *)pcRecivemessage, iSize);
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
    //
    // Clear all pending interrupts from the timer we are
    // currently using.
    //
    ulInts = TimerIntStatus(TIMERA0_BASE, true);
    TimerIntClear(TIMERA0_BASE, ulInts);
    //
    // Increment our interrupt counter.
    //
    g_usTimerInts++;
    if(!(g_usTimerInts & 0x1))
    {
        //
        // Off Led
        //
        GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    }
    else
    {
        //
        // On Led
        //
    	GPIO_IF_LedOn(MCU_RED_LED_GPIO);
    }
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
    Timer_IF_Init(PRCM_TIMERA0,TIMERA0_BASE,TIMER_CFG_PERIODIC,TIMER_A,0);
    Timer_IF_IntSetup(TIMERA0_BASE,TIMER_A,TimerPeriodicIntHandler);
    Timer_IF_Start(TIMERA0_BASE,TIMER_A,PERIODIC_TEST_CYCLES / 10);
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

static void SimpleEmail(void *pvParameters)
{
	float tempK;
	int avrgVal = 0;

	// connect to access point
    conect_to_AP();


    // interupt enable
    GPIOIntEnable(GPIOA1_BASE, 0x20);

    while(1)
    {
		while(!interupt_flag)
		{
		}

		TMP006DrvGetTemp(&tempK);
		avrgVal = (int)(tempK*10);
		MQTT_payload_string(pcEmailmessage,avrgVal);
		AESCrypt();
		Send_MQTT();

		UART_PRINT("\r\n");
		UART_PRINT(pcEmailmessage);
		UART_PRINT("\r\n");

		interupt_flag = 0;
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
//! MQTT - salje sifrovanu poruku na wolkabout.com pod temom sensors/16
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************

signed char Send_MQTT(void)
{
	unsigned char buf[200];
	int buflen = sizeof(buf);
	int payloadlen = strlen(pcRecivemessage);
	int len = 0;
	int iAddrSize;
	int iSockID;
	int iStatus;

	SlSockAddrIn_t  sAddr;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

	MQTTString topicString = MQTTString_initializer;

	data.clientID.cstring = "PKrzaID";
	data.keepAliveInterval = 60;
	data.cleansession = 1;
	data.MQTTVersion = 3;
	topicString.cstring = "sensors/016";

	//filling the TCP server socket address
	sAddr.sin_family = SL_AF_INET;
	sAddr.sin_port = sl_Htons((unsigned short)1883);
	sAddr.sin_addr.s_addr = sl_Htonl((unsigned int)WOLKABOUT_IP);

	iAddrSize = sizeof(SlSockAddrIn_t);


   // do
   // {
		// creating a TCP socket
		iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
		if( iSockID < 0 )
		{
		// error
			UART_PRINT("Unable to create a TCP socket\r\n");
		}

		// connecting to TCP server
		iStatus = sl_Connect(iSockID, ( SlSockAddr_t *)&sAddr, iAddrSize);
		if( iStatus < 0 )
		{
		// error
			sl_Close(iSockID);
			UART_PRINT("Unable to connect to TCP server\r\n");
		}
    //} while(iStatus != 0);


	// form a connect message
	len = MQTTSerialize_connect(buf, buflen, &data);
	iStatus = sl_Send(iSockID, buf, len, 0);
	  if( iStatus < 0 )
	  {
		// error
	   return -1;
	  }

	// form a publish message
	len = MQTTSerialize_publish(buf, buflen, 0, 0, 0, 0, topicString, (unsigned char *)pcRecivemessage, payloadlen);
	iStatus = sl_Send(iSockID, buf, len, 0);
	  if( iStatus < 0 )
	  {
		// error
	   return -1;
	  }

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
// In case of TI-RTOS vector table is initialize by OS itself
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
    //
    // Enable Processor
    //
    IntMasterEnable();
    IntEnable(15);

    PRCMCC3200MCUInit();
}


void conect_to_AP(void)
{
	int status;

	Network_IF_ResetMCUStateMachine();
	//
	// Start the driver
	//
	Network_IF_InitDriver(ROLE_STA);
	GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);

	//
	// Configure Timer for blinking the LED for IP acquisition
	//
	LedTimerConfigNStart();

	// Initialize AP security params
	//SecurityParams.Key = SECURITY_KEY;
	//SecurityParams.KeyLen = strlen(SECURITY_KEY);
	//SecurityParams.Type = SECURITY_TYPE;

	//
	// Connect to the Access Point
	//
	do
	{
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
	LedTimerDeinitStop();

	GPIO_IF_LedOn(MCU_IP_ALLOC_IND);
}


void AESInit(void)
{
	PRCMPeripheralReset(PRCM_DTHE);
	AESIVSet(AES_BASE,IV);
	AESConfigSet(AES_BASE, (AES_CFG_DIR_ENCRYPT | AES_CFG_KEY_SIZE_128BIT | AES_CFG_MODE_CBC));
	AESKey1Set(AES_BASE, (unsigned char *)Key, AES_CFG_KEY_SIZE_128BIT);

}

//*****************************************************************************
//
//! MQTT_payload_string
//!
//! \param  pointer to buffer
//!
//! \return None
//
//*****************************************************************************

void MQTT_payload_string(char * buff_ptr, int temperature)
{
	char *pointer = buff_ptr;
	unsigned int timestamp = time(NULL);
	char charging = 1;
	char battery = rand() % 100;
	unsigned int cifra = timestamp;
	char count = 0;
//	char temperature = 225;

	//UART_PRINT( "The current date/time is: %d",timestamp);
	strcpy(pointer, "RTC ");
	pointer += strlen("RTC ");

	while(cifra)
	{
		cifra /= 10;
		pointer++;
		count++;
	}

	while(timestamp)
	{
		//		*pointer++ = 0;pointer--;
		cifra = timestamp % 10;
		timestamp /= 10;
		*--pointer = cifra+48;
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
		*pointer++ = 49;
		*pointer++ = 48;
		*pointer++ = 48;
	}
	else if(battery < 100 && battery >9)
	{
		pointer ++;
		*pointer-- = (battery % 10)+48;
		battery /= 10;
		*pointer = (battery % 10)+48;
		pointer += 2;
	}
	else if(battery < 10)
	{
		*pointer++ = battery;
	}

	strcpy(pointer, ";BUFFER U:");
	pointer += strlen(";BUFFER U:");

	count = 0;
	timestamp = time(NULL);
	cifra = timestamp;
	while(cifra)
		{
			cifra /= 10;
			pointer++;
			count++;
		}

		while(timestamp)
		{
			//		*pointer++ = 0;pointer--;
			cifra = timestamp % 10;
			timestamp /= 10;
			*--pointer = cifra+48;
		}
		pointer += count;
		strcpy(pointer, ":");
		pointer+=3;

		*pointer-- = (temperature % 10)+48;
		temperature /= 10;
		*pointer-- = (temperature % 10)+48;
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

  BoardInit();
  
  //
  // Pinmuxing for GPIO,UART
  //
  PinMuxConfig();
  GPIOIntRegister(GPIOA1_BASE, PushButtonHandler);

  AESInit();

  //UDMAInit();
  //
  // configure LEDs
  //
  GPIO_IF_LedConfigure(LED1|LED2|LED3);

  GPIO_IF_LedOff(MCU_ALL_LED_IND);



#ifndef NOTERM  
  //
  // Configuring UART
  //
  InitTerm();
#endif

  I2C_IF_Open(I2C_MASTER_MODE_FST);
  //MAP_PRCMPeripheralReset(PRCM_I2CA0);
  //MAP_I2CMasterInitExpClk(I2C_BASE,SYS_CLK,true);
  // Init temerature sensor
  TMP006DrvOpen();

  //
  // Display Welcome Message
  //
  DisplayBanner(APP_NAME);

  // Generate Menu Output for Application
  //OutputMenu();

  // Initialize AP security params
  SecurityParams.Key = SECURITY_KEY;
  SecurityParams.KeyLen = strlen(SECURITY_KEY);
  SecurityParams.Type = SECURITY_TYPE;
  uiUartCmd=0;
  
  //
  // Simplelinkspawntask
  //
  VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);

  osi_TaskCreate(SimpleEmail, (signed char*)"SimpleEmail", OSI_STACK_SIZE, NULL, 1, NULL );

  osi_start();

  //conect_to_AP();

  while(1)
  {

  }

}
        
       
