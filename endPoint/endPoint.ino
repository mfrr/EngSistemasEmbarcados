#include <lmic.h> // Do not forget to set the radio type and the right ISM standard for your region in config.h
#include <hal/hal.h>
#include <SPI.h>

// Pin mapping... Change the pins according to microcontroller used
// for featherInterrupt
#define RFM95_DIO0 7 // IRQ for new data
#define RFM95_DIO1 5
#define RFM95_DIO2 6
#define RFM95_CS 8 // SPI's chip select
#define RFM95_RST 4 // Reset pin
#define INTERRUPT_PIN 2

#define ACTIVE_CHANNEL 63 // US915 standard, channel 63 is at 914.9MHz

const lmic_pinmap lmic_pins =
{
    .nss = RFM95_CS,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = RFM95_RST,
    .dio = {RFM95_DIO0, RFM95_DIO1, RFM95_DIO2},  //DIO0, DIO1 and DIO2
};

// LoRaWAN NwkSKey, TTN network session key, note: bytes are stored as big endian in this array as expected for LMIC library
static const PROGMEM u1_t NWKSKEY[16] = {  };

// LoRaWAN AppSKey, TTN application session key, note: bytes are stored as big endian in this array as expected for LMIC library
static const u1_t PROGMEM APPSKEY[16] = { };

// LoRaWAN end-device address, TTN DevAddr
static const u4_t DEVADDR = 0x0;

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }


volatile bool sendMsg = false;
static uint8_t mydata[2] = {0xFF,0x00};
static osjob_t sendjob;

void onEvent (ev_t ev)
{
    Serial.print(os_getTime());
    Serial.print(": ");
    switch (ev)
    {
    case EV_SCAN_TIMEOUT:
        Serial.println(F("EV_SCAN_TIMEOUT"));
        break;
    case EV_BEACON_FOUND:
        Serial.println(F("EV_BEACON_FOUND"));
        break;
    case EV_BEACON_MISSED:
        Serial.println(F("EV_BEACON_MISSED"));
        break;
    case EV_BEACON_TRACKED:
        Serial.println(F("EV_BEACON_TRACKED"));
        break;
    case EV_JOINING:
        Serial.println(F("EV_JOINING"));
        break;
    case EV_JOINED:
        Serial.println(F("EV_JOINED"));
        break;
    case EV_RFU1:
        Serial.println(F("EV_RFU1"));
        break;
    case EV_JOIN_FAILED:
        Serial.println(F("EV_JOIN_FAILED"));
        break;
    case EV_REJOIN_FAILED:
        Serial.println(F("EV_REJOIN_FAILED"));
        break;
        break;
    case EV_TXCOMPLETE:
        Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
        sendMsg = false;
        if (LMIC.dataLen)
        {
            // data received in rx slot after tx
            Serial.print(F("Data Received: "));
            Serial.write(LMIC.frame + LMIC.dataBeg, LMIC.dataLen);
            Serial.println();
        }
        break;
    case EV_LOST_TSYNC:
        Serial.println(F("EV_LOST_TSYNC"));
        break;
    case EV_RESET:
        Serial.println(F("EV_RESET"));
        break;
    case EV_RXCOMPLETE:
        // data received in ping slot
        Serial.println(F("EV_RXCOMPLETE"));
        break;
    case EV_LINK_DEAD:
        Serial.println(F("EV_LINK_DEAD"));
        break;
    case EV_LINK_ALIVE:
        Serial.println(F("EV_LINK_ALIVE"));
        break;
    default:
        Serial.println(F("Unknown event"));
        break;
    }
}

void doSend(osjob_t* j)
{
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND)
    {
        Serial.println(F("OP_TXRXPEND, not sending"));
        sendMsg = true; // retry to send
    }
    else
    {
        // Prepare upstream data transmission at the next possible time.
        mydata[1] = 1;
        LMIC_setTxData2(1, mydata, sizeof(mydata), 0);
        Serial.println(F("Packet queued"));
        Serial.println(LMIC.freq);
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void initTimer()
{
    noInterrupts();    
    TCCR1A = 0; // config mode of operation
    TCCR1B = 0; // config mode of operation
    OCR1A = 1562; // 1562 ticks with prescale 1024 at 16MHz ~ 100ms    
    TIMSK1 |= (1 << OCIE1A); // enable TIMER1_COMPA_vect interrupt
    interrupts();
}

void startTimer()
{
    noInterrupts();   
    TCNT1 = 0; // initialize counter reg with 0
    TCCR1B |= (1 << WGM12) | (1<<CS10) | (1 << CS12); // start timer and set up timer with prescaler = 1024 
    interrupts();
}

void stopTimer()
{
    noInterrupts();
    TCCR1B = 0; // stop counting
    interrupts();
}

ISR(TIMER1_COMPA_vect)
{
    if(HIGH == digitalRead(INTERRUPT_PIN))
        sendMsg = true;

    stopTimer();
}

void pinInterrupt()
{
    startTimer(); // start ~ 100ms timer
}

void setup()
{
    Serial.begin(115200);
    Serial.println(F("Starting"));

    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.

    // On AVR, these values are stored in flash and only copied to RAM
    // once. Copy them to a temporary buffer here, LMIC_setSession will
    // copy them into a buffer of its own again.
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);

    // Configuring channels
    for (uint8_t channel = 0; channel < 72; channel++)  
    {
        if (channel != ACTIVE_CHANNEL) LMIC_disableChannel(channel);
        else LMIC_enableChannel(channel);
    }

    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // Set data rate and transmit power (note: txpow seems to be ignored by the library)
    // LMIC_setDrTxpow(DR_SF7, 14);
    // LMIC_setDrTxpow(DR_SF8, 14);
    LMIC_setDrTxpow(DR_SF9, 14);
    // LMIC_setDrTxpow(DR_SF10, 14);
    // LMIC_setDrTxpow(DR_SF8C, 14);

    // Attach Interrupt
    pinMode(INTERRUPT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), pinInterrupt, RISING);

    initTimer(); 
}

void loop()
{   
    if (sendMsg)
    {
        // Schedule next transmission for the next second
        os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(1), doSend);
        sendMsg = false;
    }
    os_runloop_once();    
}
