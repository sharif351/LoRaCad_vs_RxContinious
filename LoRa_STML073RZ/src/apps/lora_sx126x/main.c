/*!
 * \file      main.c
 *
 * \brief     Radio TX/RX Test
 *
 * \remark    LED toggles upon each transmit/receive packet
 *            When LED stops blinking LoRa packets aren't transmitting/receiving any more
 *
 * \author    Uttam ( Amazon/Lab126/Ring )
 *
 */
#include "board.h"
#include "sx126x.h"

#define RF_POWER                                    0 // dBm
#define LORA_BANDWIDTH                              LORA_BW_500
#define LORA_SPREADING_FACTOR                       7        // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_SYMBOL_TIMEOUT                         5         // Symbols
#define LORA_PREAMBLE_LENGTH                        250        // Same for Tx and Rx
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define LORA_PYLD_LENGTH                            255       // Num of Bytes


// Define Radio States
typedef enum
{
    TX_START,
		CAD,
    TX,
    RX,
    CAD_DET,
    TX_DONE,
    RX_DONE,
    CAD_DONE,
    LOWPOWER,
    RX_ERROR,
    TIMEOUT,
}States_t;

typedef enum
{
    TX_MODE = 1,
    RX_MODE,
}eMode_t;

// State Machine variable 
States_t State = CAD;

// IRQ variable
volatile uint16_t irqRegs;

static uint16_t rxCnt = 0;
static uint16_t txCnt = 0;
static uint8_t buffer[LORA_PYLD_LENGTH] = {0};

// define Timer Event
TimerEvent_t TxTimer;
TimerEvent_t RxTimer;

ModulationParams_t ModulationParams;
PacketParams_t PacketParams;

/*!
 * LED GPIO pins objects (FOR SEMTECH REF BOARD ONLY)
 */
extern Gpio_t LedTx;
extern Gpio_t LedRx;

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnDIO1Interrupt( void )
{
        irqRegs = SX126xGetIrqStatus( ); 
        SX126xClearIrqStatus( IRQ_RADIO_ALL );

        if( irqRegs & IRQ_RX_DONE )
        {
        		State = RX_DONE;
        }   
        else if( ( irqRegs & IRQ_CAD_DONE ) == IRQ_CAD_DONE )
        {
            if( ( irqRegs & IRQ_CAD_ACTIVITY_DETECTED ) == IRQ_CAD_ACTIVITY_DETECTED )
            {
            		State = RX;
            }
            else
            {
                State = CAD_DONE;
            }
        }
        else if( irqRegs & IRQ_TX_DONE )
        {
        		State = TX_DONE;
        }
        else if( irqRegs & IRQ_RX_TX_TIMEOUT )
        {
            State = TIMEOUT;
        }
}

/*!
 * \brief Function executed on Radio Sleep
 */
void RadioSetSleep( void )
{
    SleepParams_t params = { 0 };

    params.Fields.WarmStart = 1;
    SX126xSetSleep( params );

    DelayMs( 2 );
}

/*!
 * \brief Function executed on Radio CAD start
 */
void RadioStartCad( void )
{
    SX126xSetDioIrqParams( IRQ_RADIO_ALL, IRQ_RADIO_ALL, IRQ_RADIO_NONE, IRQ_RADIO_NONE );

    // Set CAD parameters
    SX126xSetCadParams( LORA_CAD_04_SYMBOL, 21, 10, LORA_CAD_ONLY, 0 );
    
    // Start CAD
    SX126xSetCad( );
}

/*!
 * \brief Function executed on Timer Interrupt
 */
void OnTxTimer( void )
{
        // Stop Timer
        TimerStop( &TxTimer );

        // Check Tx Ready
        State = TX;         // do the config and lunch first CAD
}

/*!
 * \brief Function executed on Timer Interrupt
 */
void OnRxTimer( void )
{
        // Stop Timer
        TimerStop( &RxTimer );

        // Check Tx Ready
        State = CAD;         // do the config and lunch first CAD
}


void SetLoRaParam( void )
{
    // Intialize Modulation and Packet Parameters
	ModulationParams.PacketType = PACKET_TYPE_LORA;
	ModulationParams.Params.LoRa.SpreadingFactor = LORA_SPREADING_FACTOR;
	ModulationParams.Params.LoRa.Bandwidth =  LORA_BANDWIDTH;
	ModulationParams.Params.LoRa.CodingRate= LORA_CODINGRATE;
	ModulationParams.Params.LoRa.LowDatarateOptimize = 0x00;
	PacketParams.PacketType = PACKET_TYPE_LORA;
	PacketParams.Params.LoRa.PreambleLength = LORA_PREAMBLE_LENGTH;

	PacketParams.Params.LoRa.HeaderType = LORA_FIX_LENGTH_PAYLOAD_ON;
	PacketParams.Params.LoRa.PayloadLength = LORA_PYLD_LENGTH;
	PacketParams.Params.LoRa.CrcMode = true;
	PacketParams.Params.LoRa.InvertIQ = LORA_IQ_INVERSION_ON;
	
	SX126xSetStandby( STDBY_RC );
	SX126xSetPacketType( PACKET_TYPE_LORA );
	SX126xSetModulationParams( &ModulationParams );
	SX126xSetPacketParams( &PacketParams );
}

void SetGFSKParam( void )
{
    // Intialize Modulation and Packet Parameters
	ModulationParams.PacketType = PACKET_TYPE_GFSK;
	ModulationParams.Params.Gfsk.BitRate = 100000;
	ModulationParams.Params.Gfsk.Fdev =  58000;
	ModulationParams.Params.Gfsk.ModulationShaping= MOD_SHAPING_G_BT_1;
	ModulationParams.Params.Gfsk.Bandwidth = 0x1a;

	PacketParams.PacketType = PACKET_TYPE_GFSK;
	PacketParams.Params.Gfsk.PreambleLength = 40;
	PacketParams.Params.Gfsk.PreambleMinDetect = RADIO_PREAMBLE_DETECTOR_08_BITS;
	PacketParams.Params.Gfsk.SyncWordLength = 4;
	PacketParams.Params.Gfsk.AddrComp = RADIO_ADDRESSCOMP_FILT_OFF;
	PacketParams.Params.Gfsk.HeaderType = RADIO_PACKET_VARIABLE_LENGTH;
    PacketParams.Params.Gfsk.PayloadLength = 16;
    PacketParams.Params.Gfsk.CrcLength = RADIO_CRC_2_BYTES_CCIT;
    PacketParams.Params.Gfsk.DcFree = RADIO_DC_FREE_OFF;
	
}


/*!
 * Main application entry point.
 */
int main( void )
{
    uint32_t mask;
    uint32_t freq = 915000000;
    eMode_t mode = 1;

    // Target board initialization
    BoardInitMcu( );
    BoardInitPeriph( );

    // Clear Terminal Screen
    printf("\033[2J\033[H");
    printf("\r\nStart\r\n");

    // Initialize Radio
    SX126xInit( OnDIO1Interrupt );
    // Set Radio to StandBy Mode

    // Radio initialization
    SX126xSetStandby( STDBY_RC );
    SX126xSetRegulatorMode( USE_DCDC );

    // Initialize Buffer
    SX126xSetBufferBaseAddress( 0x00, 0x00 );

    // Set TX Parameters
    SX126xSetTxParams( RF_POWER, RADIO_RAMP_200_US );
    
    // Clear IRQ
    SX126xSetDioIrqParams( IRQ_RADIO_ALL, IRQ_RADIO_ALL, IRQ_RADIO_NONE, IRQ_RADIO_NONE );

    // Get Frequency as input from User
    printf("Enter LoRa frequency in Hz...\r\n");
    //scanf("%u",&freq);
    // Set RF Frequency
    SX126xSetRfFrequency( freq );

    printf("\r\nEnter Mode: 1 -> TX; 2 -> RX\r\n");
    scanf("%d",&mode);

    // Initialize LoRa Parameters
    SetLoRaParam( );

    // Initialize buffer values
    for(int i = 0; i < LORA_PYLD_LENGTH; i++)
    {
        buffer[i] = i;
    }

    if( mode == TX_MODE )
    {
        TimerInit( &TxTimer, OnTxTimer );
        TimerSetValue( &TxTimer, 500 );
        State = TX_START;
    }
		else if( mode == RX_MODE )
    {
        TimerInit( &RxTimer, OnRxTimer );
        TimerSetValue( &RxTimer, 60 );
        State = CAD;
				SX126xSetLoRaSymbNumTimeout( 13 );
    }

    while( 1 )
    {
        switch( State )
        {
            case TX_START:
                TimerStart( &TxTimer );
                State = LOWPOWER;
                break;

            case TX:
								printf("Transmitting----");
                State = LOWPOWER;                
                SX126xSetStandby( STDBY_RC );
                // Start TX
                SX126xSetDioIrqParams( IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT,
                       IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT,
                       IRQ_RADIO_NONE,
                       IRQ_RADIO_NONE );

                SX126xSendPayload( buffer, LORA_PYLD_LENGTH, 0 );
                break;
						
						 case CAD:
                State = LOWPOWER;
                // Start CAD
                RadioStartCad( );         // do the config and lunch first CAD
								State = LOWPOWER;
                break;

            case RX:
                SX126xSetStandby( STDBY_RC );
                SX126xSetDioIrqParams( IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT,
                       IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT,
                       IRQ_RADIO_NONE,
                       IRQ_RADIO_NONE );
                SX126xSetRx( 0 );

                State = LOWPOWER;

                break;

            case RX_DONE:
                // Uncomment for debug purposes
                printf("\r\nRX: %u ", ++rxCnt );

                // Disable Board Interrupt
                BoardCriticalSectionBegin( &mask );
						
								PacketStatus_t pktStatus = {0};
								SX126xGetPacketStatus( &pktStatus );
								//printf("Received RSSI_inst = %d\t", RssiInst);
								printf("Received RssiPkt = %d\t", pktStatus.Params.LoRa.RssiPkt);
								//printf("Received SignalRssiPkt = %d\t", pktStatus.Params.LoRa.SignalRssiPkt);
								printf("Received SnrPkt = %d\n\r", pktStatus.Params.LoRa.SnrPkt);
						
                State = CAD_DONE;
                // Toggle LED (FOR SEMTECH REF BOARD ONLY)
                GpioToggle( &LedRx );
                // Enable Board Interrupt
                BoardCriticalSectionEnd( &mask );								

                break;

            case TX_DONE:
                printf("TX-done: %u\r\n",++txCnt);

                // Disable Board Interrupt
                BoardCriticalSectionBegin( &mask );
                State = TX_START;
                GpioToggle( &LedTx );
                // Enable Board Interrupt
                BoardCriticalSectionEnd( &mask );

                break;

            case TIMEOUT:
                //printf("\r\nTimeOut");

                // Disable Board Interrupt
                BoardCriticalSectionBegin( &mask );

                if( mode == TX_MODE )
                    State = TX_START;
                else if( mode == RX_MODE )
                    State = CAD_DONE;

                // Enable Board Interrupt
                BoardCriticalSectionEnd( &mask );

                break;

            case LOWPOWER:
                break;
												
						case CAD_DONE:
								//printf("\r\n<<<<<<<<<<< CAD Done >>>>>>>>>>>>");
								BoardCriticalSectionBegin( &mask );	
								TimerStart( &RxTimer );						
								State = LOWPOWER;
								BoardCriticalSectionEnd( &mask );
								break;

            default:
                printf("\r\nERR");
                State = LOWPOWER;
                break;
        }
      
			//TimerLowPowerHandler( );

    }
}

