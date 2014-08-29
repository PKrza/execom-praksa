//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
//
//  "C:\Users\sstankovic\code\eclipse.paho.mqtt.embedded"
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

//*****************************************************************************
//
// Application Name     - TCP Socket
// Application Overview - This particular application illustrates how this
//                        device can be used as a client or server for TCP
//                        communication.
// Application Details  -
// http://processors.wiki.ti.com/index.php/CC32xx_TCP_Socket_Application
// or
// docs\examples\CC32xx_TCP_Socket_Application.pdf
//
//*****************************************************************************


//****************************************************************************
//
//! \addtogroup tcp_socket
//! @{
//
//****************************************************************************

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// simplelink includes
#include "simplelink.h"
#include "wlan.h"
#include "socket.h"

// driverlib includes
#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "uart.h"
#include "utils.h"
#include "pinmux.h"
#include "tmp006drv.h"
#include "i2c_if.h"
#include "timer_if.h"
#include "timer.h"
#include <signal.h>

//	MQTT includes
#include "MQTTPacket.h"
#include "MQTTConnect.h"
#include "MQTTPublish.h"
#include "MQTTSubscribe.h"
#include "MQTTUnsubscribe.h"
#include "StackTrace.h"
#include "MQTTConnect.h"
#include "MQTTPacket.h"
#include "adc_func.h"

//  AES includes
#include "aes_vector.h"
#include "aes_userinput.h"
#include "hw_aes.h"
#include "aes.h"

// common interface includes
#include "udma_if.h"
#ifndef NOTERM
#include "uart_if.h"
#endif

#ifdef NOTERM
#define UART_PRINT(x, ...)
#else
#define UART_PRINT Report
#endif

#define APPLICATION_NAME        " ***MQTT Subscriber*** "
#define APPLICATION_VERSION     "1.0.0"
#define SUCCESS                 0

//
// Values for below macros shall be modified as per access-point(AP) properties
// SimpleLink device will connect to following AP when application is executed
//
#define SSID_NAME           "ecguest"    /* AP SSID */
#define SECURITY_TYPE       SL_SEC_TYPE_WPA/* Security type (OPEN or WEP or WPA)*/
#define SECURITY_KEY        "execomguest"              /* Password of the secured AP */
#define SSID_LEN_MAX        (32)
#define BSSID_LEN_MAX       (6)

#define IP_ADDR             0x25FC7D5A//wolkabout.com //0x0a000009//10.0.0.9 - IP racunara//
#define PORT_NUM            1883
#define SL_STOP_TIMEOUT     30
#define BUF_SIZE            15
#define MAX_BUFF_SIZE		128
#define TCP_PACKET_COUNT    1
#define USER_INPUT_ENABLE   1
#define SYS_CLK				80000000

// Loop forever, user can change it as per application's requirement
#define LOOP_FOREVER(line_number) \
            {\
                while(1); \
            }

// check the error code and handle it
#define ASSERT_ON_ERROR(line_number, error_code) \
            {\
                if (error_code < 0) return error_code;\
            }

//Status bits - These are used to set/reset the corresponding bits in
// given variable
typedef enum{
    STATUS_BIT_CONNECTION =  0, // If this bit is Set SimpleLink device is
                                // connected to the AP

    STATUS_BIT_IP_AQUIRED       // If this bit is Set SimpleLink device has
                                 // acquired IP

}e_StatusBits;

// Application specific status/error codes
typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

#define SET_STATUS_BIT(status_variable, bit)  status_variable |= (1<<(bit))
#define CLR_STATUS_BIT(status_variable, bit)  status_variable &= ~(1<<(bit))
#define CLR_STATUS_BIT_ALL(status_variable)   (status_variable = 0)
#define GET_STATUS_BIT(status_variable, bit)  (0 != (status_variable & \
                                                                (1<<(bit))))

#define IS_CONNECTED(status_variable)         GET_STATUS_BIT(status_variable, \
                                                       STATUS_BIT_CONNECTION)
#define IS_IP_ACQUIRED(status_variable)       GET_STATUS_BIT(status_variable, \
                                                       STATUS_BIT_IP_AQUIRED)


//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
signed char Send_MQTT(char *message);
void MQTT_payload_string(char * buff_ptr, int temperature, char battery);         //, int temperature
static INT32 WlanConnect();
static void DisplayBanner();
static void BoardInit();
static void InitializeAppVariables();
signed char receivePublish(void);
static void PerformPRCMDeepSleepGPTWakeup(void);

//
// GLOBAL VARIABLES -- Start
//
unsigned long  g_ulStatus = 0;//SimpleLink Status
unsigned long  g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
unsigned long  g_ulDestinationIp = IP_ADDR;
unsigned int   g_uiPortNum = PORT_NUM;
unsigned long  g_ulPacketCount = TCP_PACKET_COUNT;
unsigned char  g_ucConnectionStatus = 0;
unsigned char  g_ucSimplelinkstarted = 0;
unsigned long  g_ulIpAddr = 0;
char g_cBsdBuf[BUF_SIZE];

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

int iAddrSize = sizeof(SlSockAddrIn_t);
SlSockAddrIn_t  sAddr;
MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
MQTTString topicString = MQTTString_initializer;

char messageToBeCrypted[MAX_BUFF_SIZE] = {0};
char cryptedMessage[MAX_BUFF_SIZE] = {0};
char recievedFromServer[MAX_BUFF_SIZE] = {0};
unsigned char buf[200];
int buflen = sizeof(buf);
char poruka[128];
char rezultat[128];
float temp_raw = 0;
int Temp = 0;
char interupt_flag = 1;
char crypt_flag = 1;
int heartbeat = 1;
long server_rtc = 0;
int iSockID = 0;
int g_usTimerInts;
char messageLength = 0;
int heartbeat_count = 1;
//*****************************************************************************
// Get string - start
//*****************************************************************************
#define UartGetChar()		MAP_UARTCharGet(CONSOLE)
char cCharacter;
int iStringLength = 0;

char *myGetString();



//*****************************************************************************
//
//! The interrupt handler for the first timer interrupt.
//!
//! \param  None
//!
//! \return none
//
//*****************************************************************************
void
TimerBaseIntHandler(void)
{

	unsigned long ulInts;
	ulInts = TimerIntStatus(TIMERA1_BASE, true);
	TimerIntClear(TIMERA1_BASE, ulInts);

	//if(crypt_flag)
	//{
	//UART_PRINT("Prekid TIMERA1\r\n");
		g_usTimerInts++;

		if(g_usTimerInts == 1)
			interupt_flag = 1;

		if(g_usTimerInts == 6)
			PRCMSOCReset();
	//}

}


//*****************************************************************************
//
//		DeepSleep Timer A0 Handler
//
//
//*****************************************************************************
void AppGPTCallBackHandler()
{
    MAP_TimerIntClear(TIMERA0_BASE,TIMER_TIMB_TIMEOUT|TIMER_TIMA_TIMEOUT);

    crypt_flag = 1;
    //start TIMERA1
    PRCMPeripheralClkEnable(PRCM_TIMERA1, PRCM_RUN_MODE_CLK);
	Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
	Timer_IF_IntSetup(TIMERA1_BASE, TIMER_A, TimerBaseIntHandler);
	Timer_IF_Start(TIMERA1_BASE, TIMER_A, (10 * SYS_CLK));

    //UART_PRINT("\n\rGPT TimerA0 Interrupt occured\n\r");
}


//*****************************************************************************
//
//		DeepSleep function
//
//
//*****************************************************************************
void PerformPRCMDeepSleepGPTWakeup(void)
{
    //
    // Power On the GPT
    //
    PRCMPeripheralClkEnable(PRCM_TIMERA0, PRCM_RUN_MODE_CLK);
    //
    // Initialize the GPT as One Shot timer, Clock is halved in DeepSleep
    //
    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_ONE_SHOT, TIMER_BOTH, 0);
    Timer_IF_IntSetup(TIMERA0_BASE, TIMER_BOTH, AppGPTCallBackHandler);
    Timer_IF_Start(TIMERA0_BASE, TIMER_BOTH, (25 * SYS_CLK));//SYS_CLK = 80MHz => 1s, ali kada je u deepsleep onda je to 40MHz => 2s
    //
    // Enable the DeepSleep Clock
    //
    PRCMPeripheralClkEnable(PRCM_TIMERA0, PRCM_DSLP_MODE_CLK);
    //
    // Enter DEEPSLEEP...WaitForInterrupt ARM intrinsic
    //
    //UART_PRINT("\n\rGPT_DEEPSLEEP: Entering Deep Sleep");
    PRCMDeepSleepEnter();// = PRCMDeepSleepEnter();
	//UART_PRINT("\n\rGPT_DEEPSLEEP: Exiting Deep Sleep\r\n");
    //
    // Disable the DeepSleep Clock
    //
    PRCMPeripheralClkDisable(PRCM_TIMERA0, PRCM_DSLP_MODE_CLK);
    //
    // Deinitialize the GPT
    //
    Timer_IF_Stop(TIMERA0_BASE, TIMER_BOTH);
    Timer_IF_DeInit(TIMERA0_BASE, TIMER_BOTH);
    //
    // PowerOff GPT
    //
    PRCMPeripheralClkDisable(PRCM_TIMERA0, PRCM_RUN_MODE_CLK);
    //UART_PRINT("\n\rGPT_DEEPSLEEP: Exiting Deep Sleep");
}

//*****************************************************************************
//
//myGetString Function
//
//! \return string like a series of characters
//*****************************************************************************
char *myGetString()
{
	cCharacter = UartGetChar();
	while(cCharacter != 13 && cCharacter != '\r' && cCharacter != '\n')
	{
		//upisuje u string niz od drugog na dalje
		g_cBsdBuf[iStringLength] = cCharacter;
		cCharacter = UartGetChar();
		iStringLength++;//koliko ih je upisao

		//ako prekoraci duzinu iskoci
		if(iStringLength >= BUF_SIZE -1)
			break;
	}

	g_cBsdBuf[iStringLength] = '\0';
	iStringLength = 0;

	return g_cBsdBuf;
}
//*****************************************************************************
// Get string - end
//*****************************************************************************


//*****************************************************************************
// AES - start
//*****************************************************************************
char Key[16] = "no-preshared-key";
unsigned char IV[16] = {0};

void AESCrypt(long direction, char *sourceBuff, char *resultBuff);
void AESInit(void);

//*****************************************************************************
// AES initialisation
//*****************************************************************************
void AESInit(void)
{
	MAP_PRCMPeripheralClkEnable(PRCM_DTHE, PRCM_RUN_MODE_CLK);
	UART_PRINT("\n\rconfig AES\n\r");
}
//*****************************************************************************
//
//! AES Crypt Function
//!
//! This function Configures Key,Mode and carries out Encryption/Decryption in
//!                                                  CPU mode
//! \param chosenEncrypt - Chose between Encrypt and Decrypt
//! \param puiData - Input Data
//! \param puiResult - Resultant Output Data
//!
//! \return none
//
//*****************************************************************************
void AESCrypt(long direction, char *sourceBuff, char *resultBuff)
{
	unsigned int uiMsgLen,iSize;
	int iMod=0;

	uiMsgLen = strlen(sourceBuff);
	iSize=uiMsgLen;

	iMod=uiMsgLen%16;
	if(iMod)
	{
		iSize=((uiMsgLen/16)+1)*16;
	}

	PRCMPeripheralReset(PRCM_DTHE);
	AESConfigSet(AES_BASE, (direction | AES_CFG_MODE_CBC | AES_CFG_KEY_SIZE_128BIT));
	AESIVSet(AES_BASE, IV);
	AESKey1Set(AES_BASE,(unsigned char *)Key, AES_CFG_KEY_SIZE_128BIT);

	AESDataProcess(AES_BASE, (unsigned char *)sourceBuff, (unsigned char *)resultBuff, iSize);
}
//*****************************************************************************
// AES end
//*****************************************************************************


//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************


//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    switch(pWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'sl_protocol_wlanConnectAsyncResponse_t'-Applications
            // can use it if required
            //
            //  sl_protocol_wlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //
            // Copy new connection SSID and BSSID to global parameters
            memcpy(g_ucConnectionSSID,pWlanEvent->EventData.
                   STAandP2PModeWlanConnected.ssid_name,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
            memcpy(g_ucConnectionBSSID,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                   SL_BSSID_LENGTH);

            UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s ,"
                        " BSSID: %x:%x:%x:%x:%x:%x\n\r",
                      g_ucConnectionSSID,g_ucConnectionBSSID[0],
                      g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                      g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                      g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
            sl_protocol_wlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_USER_INITIATED_DISCONNECTION
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
            {
                UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
                "BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            else
            {
                UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s,"
                            "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
            memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
        }
        break;

        default:
        {
            UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                       pWlanEvent->Event);
        }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    switch(pNetAppEvent->Event)
    {
        case SL_NETAPP_IPV4_ACQUIRED:
        {
            SlIpV4AcquiredAsync_t *pEventData = NULL;

            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            //Ip Acquired Event Data
            pEventData = &pNetAppEvent->EventData.ipAcquiredV4;
            g_ulIpAddr = pEventData->ip;//ovde je ip

            //
            //Gateway IP address
            g_ulGatewayIP = pEventData->gateway;

            UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                        "Gateway=%d.%d.%d.%d\n\r",
							SL_IPV4_BYTE(g_ulIpAddr,3),//SL_IPV4_BYTE pomera za onoliko bajtova u levo za koliko se porsledi treci parametar
							SL_IPV4_BYTE(g_ulIpAddr,2),
							SL_IPV4_BYTE(g_ulIpAddr,1),
							SL_IPV4_BYTE(g_ulIpAddr,0),
							SL_IPV4_BYTE(g_ulGatewayIP,3),
							SL_IPV4_BYTE(g_ulGatewayIP,2),
							SL_IPV4_BYTE(g_ulGatewayIP,1),
							SL_IPV4_BYTE(g_ulGatewayIP,0));
        }
        break;

        default:
        {
            UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                       pNetAppEvent->Event);
        }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
}

//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    //
    // This application doesn't work w/ socket - Events are not expected
    //

}

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************


//*****************************************************************************
//
//! This function initializes the application variables
//!
//! \param[in]    None
//!
//! \return None
//!
//*****************************************************************************
static void InitializeAppVariables()
{
    g_ulStatus = 0;
    g_ulGatewayIP = 0;
    memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
    g_ulDestinationIp = IP_ADDR;
    g_uiPortNum = PORT_NUM;
    g_ulPacketCount = TCP_PACKET_COUNT;
}

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - TBD - Unregister mDNS services
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState()
{
    SlVersionFull   ver = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(__LINE__, lMode);

    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                                &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
    ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
    ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
    ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
    ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                                SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);//brise wlan profil, prosledis mu koji
    ASSERT_ON_ERROR(__LINE__, lRetVal);

//AP MODE

    // If the device is not in station-mode, try putting it in staion-mode
    if (ROLE_STA != lMode)
    {
        if (ROLE_AP == lMode)
        {
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while(!IS_IP_ACQUIRED(g_ulStatus))
            {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
            }
        }

        // Switch to STA role and restart
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(__LINE__, lRetVal);

//stop i start su reset da bi prihvatio novi mod
        lRetVal = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(__LINE__, lRetVal);

        // reset status bits
        CLR_STATUS_BIT_ALL(g_ulStatus);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(__LINE__, lRetVal);

        // Check if the device is in station again
        if (ROLE_STA != lRetVal)
        {
            // We don't want to proceed if the device is not up in STA-mode
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }

//STAION-MODE

    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal)
    {
        // Wait
        while(IS_CONNECTED(g_ulStatus))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Disable scan, Scan policy je vremenski interval nakom se radi porvera ima li konekcije
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(__LINE__, lRetVal);

    InitializeAppVariables();

    return lRetVal; // Success
}

//****************************************************************************
//
//!    \brief Parse the input IP address from the user
//!
//!    \param[in]                     ucCMD (char pointer to input string)
//!
//!    \return                        0 : if correct IP, -1 : incorrect IP
//
//****************************************************************************
int IpAddressParser(char *ucCMD)
{
 int i=0;
    unsigned int uiUserInputData;
    unsigned long ulUserIpAddress = 0;
    char *ucInpString;
    ucInpString = strtok(ucCMD, ".");
    uiUserInputData = (int)strtoul(ucInpString,0,10);
    while(i<4)
    {
        //
       // Check Whether IP is valid
       //
       if((ucInpString != NULL) && (uiUserInputData < 256))
       {
           ulUserIpAddress |= uiUserInputData;
           if(i < 3)
               ulUserIpAddress = ulUserIpAddress << 8;
           ucInpString=strtok(NULL,".");
           uiUserInputData = (int)strtoul(ucInpString,0,10);
           i++;
       }
       else
       {
           return -1;
       }
    }
    g_ulDestinationIp = ulUserIpAddress;
    return 0;
}

//****************************************************************************
//
//!  \brief Connecting to a WLAN Accesspoint
//!
//!   This function connects to the required AP (SSID_NAME) with Security
//!   parameters specified in te form of macros at the top of this file
//!
//!   \param[in]              None
//!
//!   \return     Status value
//!
//!   \warning    If the WLAN connection fails or we don't aquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
static long WlanConnect()
{
    SlSecParams_t secParams = {0};
    INT32 retVal = 0;

    secParams.Key = SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    retVal = sl_WlanConnect(SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(__LINE__, retVal);

    /* Wait */
    while((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus)))
    {
        // Wait for WLAN Event
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask();
#endif
    }

    return SUCCESS;

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

void MQTT_payload_string(char * buff_ptr, int temperature, char battery)           //, int temperature
{
	char *pointer = buff_ptr;
	unsigned int timestamp = time(NULL);
	char charging = 0;
	//char battery = rand() % 100;
	unsigned int number = timestamp;
	char count = 0;
	//int temperature = 911;

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
		*pointer = battery+48;
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
// Connect to wlan station
//*****************************************************************************
//int iSockID = 0;

void conect_to_AP(void)
{

	long retVal = -1;
	//int iStatus;
	//SlSockAddrIn_t  sAddr;
	//int iAddrSize;

	InitializeAppVariables();
	//
	// Following function configure the device to default state by cleaning
	// the persistent settings stored in NVMEM (viz. connection profiles &
	// policies, power policy etc)
	//
	// Applications may choose to skip this step if the developer is sure
	// that the device is in its default state at start of applicaton
	//
	// Note that all profiles and persistent settings that were done on the
	// device will be lost
	//
	retVal = ConfigureSimpleLinkToDefaultState();

	if(retVal < 0)
	{
	  if (DEVICE_NOT_IN_STATION_MODE == retVal)
		  UART_PRINT("Failed to configure the device in its default state \n\r");

	  LOOP_FOREVER(__LINE__);
	}

	UART_PRINT("Device is configured in default state \n\r");

	//
	// Asumption is that the device is configured in station mode already
	// and it is in its default state
	//
	retVal = sl_Start(0, 0, 0);
	if (retVal < 0)
	{
	  UART_PRINT("Failed to start the device \n\r");
	  LOOP_FOREVER(__LINE__);
	}

	UART_PRINT("Device started as STATION \n\r");


	UART_PRINT("Connecting to AP: %s ...\r\n",SSID_NAME);

	// Connecting to WLAN AP - Set with static parameters defined at the top
	// After this call we will be connected and have IP address
	WlanConnect();

	UART_PRINT("Connected to AP: %s \n\r",SSID_NAME);

	UART_PRINT("Device IP: %d.%d.%d.%d\n\r\n\r",
			  SL_IPV4_BYTE(g_ulIpAddr,3),
			  SL_IPV4_BYTE(g_ulIpAddr,2),
			  SL_IPV4_BYTE(g_ulIpAddr,1),
			  SL_IPV4_BYTE(g_ulIpAddr,0));

	Message("\n\r***zavrsio inicjalizaciju***\n\r");

	// creating a TCP socket; oni sve ovo rade sa Socket_new
	//filling the TCP server socket address
	sAddr.sin_family = SL_AF_INET;
	sAddr.sin_port = sl_Htons((unsigned short)g_uiPortNum);//port koji porseldis dodelis clanu strukture
	sAddr.sin_addr.s_addr = sl_Htonl((unsigned int)g_ulDestinationIp);//dodeli mu i IP add

	iAddrSize = sizeof(SlSockAddrIn_t);
/*
	// creating a TCP socket
	iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);//vraca soket
	if( iSockID < 0 )
	{
		 // error
		Message("\n\rERROR:nisam uspeo da zauzmem soket");
	}

	// connecting to TCP server
	iStatus = sl_Connect(iSockID, ( SlSockAddr_t *)&sAddr, iAddrSize);//()
	if( iStatus < 0 )
	{
		// error
		sl_Close(iSockID);
		Message("\n\rERROR:nisam uspeo da se konektujem");
	}
	Message("\n\rUspesno kreirao soket");
*/
}


//*****************************************************************************
// MQTT - start
//*****************************************************************************
#define SUBSCRIBE_TOPIC		"config/014"
#define PUBLISH_TOPIC		"sensors/014"

int rc = 0;
//unsigned char buf[200];
//int buflen = sizeof(buf);

signed char Subscribe_MQTT(void);
int getdata(unsigned char* buf, int count);
void cfinish(int sig);

//*****************************************************************************
// MQTT function
//*****************************************************************************
int getdata(unsigned char* buf, int count)
{
	return recv(iSockID, buf, count, 0);
}

int toStop = 0;

void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
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

	// connecting to TCP server
	iStatus = sl_Connect(iSockID, ( SlSockAddr_t *)&sAddr, iAddrSize);
	if( iStatus < 0 )
	{
	// error
		sl_Close(iSockID);
		UART_PRINT("Unable to connect to TCP server\r\n");
		return -1;
	}
	topicString.cstring = PUBLISH_TOPIC;

	iStatus = sl_Send(iSockID, buf, len, 0);

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
//! Subscribe na temu
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
signed char Subscribe_MQTT(void)
{
		int len = 0;
		int msgid = 1;
		int iStatus;
		int req_qos = 0;

		signal(SIGINT, cfinish);
		signal(SIGTERM, cfinish);

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
// MQTT end
//*****************************************************************************



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

    Report("\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t      CC3200 %s Application       \n\r", AppName);
    Report("\t\t *************************************************\n\r");
    Report("\n\n\n\r");
}

//*****************************************************************************
//
//		Recieve Publish
//
//
//*****************************************************************************

signed char receivePublish(void)
{
	char *ptr = recievedFromServer;
	signed char rc = -1;
	signed char iStatus;

	iStatus = sl_Connect(iSockID, ( SlSockAddr_t *)&sAddr, iAddrSize);
	if( iStatus < 0 )
	{
		sl_Close(iSockID);
		UART_PRINT("Unable to connect to TCP server\r\n");
		return -1;
	}

	memcpy(buf, 0, buflen);

	if (MQTTPacket_read(buf, buflen, getdata) == PUBLISH)
		{
			unsigned char dup;
			int qos;
			unsigned char retained;
			unsigned short msgid;
			int payloadlen_in;
			unsigned char* payload_in;
			MQTTString receivedTopic;

			MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic, &payload_in, &payloadlen_in, buf, buflen);

				AESCrypt(AES_CFG_DIR_DECRYPT, (char *)payload_in, recievedFromServer);
				messageLength = strlen(recievedFromServer);
				UART_PRINT("\n\rRecieved message: %s Message length: %d\n\r", recievedFromServer, messageLength);

				ptr = strtok(recievedFromServer, " ;");
				ptr = strtok(NULL, " ;");
				server_rtc = atoi(ptr);
				ptr = strtok(NULL, " ;");
				ptr = strtok(NULL, " ;");
				heartbeat = atoi(ptr);

				if(heartbeat == 0)
					heartbeat = 1;
			rc = 0;
		}
	crypt_flag = 1;
	return rc;
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
  //
  // Enable Processor
  //
  MAP_IntMasterEnable();
  MAP_IntEnable(FAULT_SYSTICK);

  PRCMCC3200MCUInit();
}

//****************************************************************************
//                            MAIN FUNCTION
//****************************************************************************
void main()
{
    //unsigned char puiResultLoc[128];
    int status, battery = 10, brojac, count = 0;
	//
	// Board Initialization
	//
	BoardInit();
	//
	// Initialize the uDMA
	//
	UDMAInit();

	//
	// Configure the pinmux settings for the peripherals exercised
	//
	PinMuxConfig();
	//
	// Configuring UART
	//
	InitTerm();

	PRCMPeripheralClkEnable(PRCM_TIMERA1, PRCM_RUN_MODE_CLK);
	Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
	Timer_IF_IntSetup(TIMERA1_BASE, TIMER_A, TimerBaseIntHandler);
	Timer_IF_Start(TIMERA1_BASE, TIMER_A, (10 * SYS_CLK));
	g_usTimerInts = 0;
	interupt_flag = 1;
	crypt_flag = 1;
	//
	// Initialize I2C
	//
	I2C_IF_Open(I2C_MASTER_MODE_FST);
	//
	// Temperature sensor initialization
	//
	TMP006DrvOpen();

	initADC();
/*
	Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
	Timer_IF_IntSetup(TIMERA1_BASE, TIMER_A, TimerBaseIntHandler);
	Timer_IF_Start(TIMERA1_BASE, TIMER_A, SYS_CLK * 10);
*/
	//
	// Display banner
	//
	DisplayBanner(APPLICATION_NAME);
    //
    // Enable AES Module
    //
	AESInit();
	//
	// Connect to station
	//
	conect_to_AP();
	//
	// Subscribe on topic
	//
	Subscribe_MQTT();

	TMP006DrvGetTemp(&temp_raw);
	Temp = (int)(temp_raw*10);
	battery =  batteryVoltage();

	while (1)
	{
		if(!(--heartbeat_count))
		{
			while(!interupt_flag)
			{
				if(crypt_flag)
				{
					TMP006DrvGetTemp(&temp_raw);
					Temp = (int)(temp_raw*10);

					battery =  batteryVoltage();
					UART_PRINT("Battery: %d\r\n", battery);
					//UART_PRINT("g_usTimerInts = %d\r\n", g_usTimerInts);
					MQTT_payload_string(messageToBeCrypted,Temp, battery);
					AESCrypt(AES_CFG_DIR_ENCRYPT, messageToBeCrypted, cryptedMessage);
					crypt_flag = 0;
				}
			}

			do
			{
				//UART_PRINT("Interupt_flag: %d\r\n", interupt_flag);
				count++;
				if(crypt_flag)
				{
					MQTT_payload_string(messageToBeCrypted,Temp, battery);
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
				}
				else
				{
					UtilsDelay((SYS_CLK/4));
					status = receivePublish();
				}
			} while(status < 0 || messageLength < 27 || messageLength > 29);

			count = 0;
			messageLength = 0;
			brojac+=heartbeat;
			UART_PRINT("%d) Message <%s> sent successfuly\r\n", brojac, messageToBeCrypted);
			UART_PRINT("Server RTC: %d\r\nServer HEARTBEAT: %d\r\n", server_rtc, heartbeat);
			heartbeat_count = heartbeat;
		}

		while(!interupt_flag)
		{
		}
		PRCMPeripheralClkDisable(PRCM_TIMERA1, PRCM_RUN_MODE_CLK);
		interupt_flag = 0;
		crypt_flag = 0;
		PerformPRCMDeepSleepGPTWakeup();
		g_usTimerInts = 0;
	}
}//od main-a

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
