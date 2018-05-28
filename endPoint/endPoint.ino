#include <lmic.h> // Do not forget to set the radio type and the right ISM standard for your region in config.h
#include <hal/hal.h>
#include <SPI.h>

// Pin mapping... Change the pins according to microcontroller used

// for feather32u4
#define RFM95_DIO0 7 // IRQ para dado novo
#define RFM95_DIO1 5
#define RFM95_DIO2 6
#define RFM95_CS 8 // Chip select do SPI
#define RFM95_RST 4 // Pino de reset

#define ACTIVE_CHANNEL 63 // active channel (US915 channel 63 which are at 914.9Mhz)

const lmic_pinmap lmic_pins =
{
    .nss = RFM95_CS,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = RFM95_RST,
    .dio = {RFM95_DIO0, RFM95_DIO1, RFM95_DIO2},  //DIO0, DIO1 and DIO2
};

// LoRaWAN NwkSKey, TTN network session key, note: bytes are stored as big endian in this array as expected for LMIC library
static const PROGMEM u1_t NWKSKEY[16] = { 0x34, 0xA3, 0xA0, 0x2B, 0x7D, 0xAE, 0xBD, 0xA5, 0x9B, 0x80, 0x2F, 0xBA, 0x83, 0x19, 0x33, 0x82 };

// LoRaWAN AppSKey, TTN application session key, note: bytes are stored as big endian in this array as expected for LMIC library
static const u1_t PROGMEM APPSKEY[16] = { 0x15, 0x88, 0x13, 0xB7, 0xCD, 0xB0, 0x73, 0x75, 0xC9, 0x96, 0x31, 0xC9, 0x1B, 0xB3, 0x7B, 0xF9 };

// LoRaWAN end-device address, TTN DevAddr
static const u4_t DEVADDR = 0x26062857 ;

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

static uint8_t mydata[2] = {0xFF,0x00};
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

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
        if (LMIC.dataLen)
        {
            // data received in rx slot after tx
            Serial.print(F("Data Received: "));
            Serial.write(LMIC.frame + LMIC.dataBeg, LMIC.dataLen);
            Serial.println();
        }
        // Schedule next transmission
        os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
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

void do_send(osjob_t* j)
{
    static uint8_t counter = 0;

    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND)
    {
        Serial.println(F("OP_TXRXPEND, not sending"));
    }
    else
    {
        // Prepare upstream data transmission at the next possible time.
        mydata[1] = counter;
        counter++;
        LMIC_setTxData2(1, mydata, sizeof(mydata), 0);
        Serial.println(F("Packet queued"));
        Serial.println(LMIC.freq);
    }
    // Next TX is scheduled after TX_COMPLETE event.
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
    LMIC_setDrTxpow(DR_SF7, 14);
    // LMIC_setDrTxpow(DR_SF8, 14);
    // LMIC_setDrTxpow(DR_SF9, 14);
    // LMIC_setDrTxpow(DR_SF10, 14);
    // LMIC_setDrTxpow(DR_SF8C, 14);

    // Start job
    do_send(&sendjob);
    Serial.println("Freq");
    Serial.println(LMIC.freq);
}

void loop()
{
    os_runloop_once();
}
