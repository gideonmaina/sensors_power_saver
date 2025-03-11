#include <Adafruit_FONA.h>
#include "global_configs.h"

HardwareSerial fonaSS(1);
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

char SIM_PIN[5] = GSM_PIN;
bool GSM_CONNECTED = false;
bool SIM_AVAILABLE = false;
bool GPRS_CONNECTED = false;
bool SIM_PIN_SET = false;
bool SIM_USABLE = false;
char SIM_CID[21] = "";
// String GSM_INIT_ERROR = "";
String NETWORK_NAME = "";

const int error_buff_size = 256;
char error_buffer[error_buff_size];
const int gsm_buff_size = 1024;
char raw_gsm_response[gsm_buff_size];

/**** Function Declacrations **/
bool GSM_init();
static void unlock_pin(char *PIN);
String handle_AT_CMD(String cmd, int _delay = 1000);
void SIM_PIN_Setup();
bool is_SIMCID_valid();
bool GPRS_init();
void GSM_soft_reset();
void restart_GSM();
void enableGPRS();
void flushSerial();
void QUECTEL_POST(char *url, String headers[], int header_size, const String &data, int data_length);
int GPRS_INIT_FAIL_COUNT = 0;
int HTTP_POST_FAIL = 0;
// Set a decent delay before this to warm up the GSM module

char get_raw_response(const char *cmd, char *res_buff, int timeout = 3000);

bool validate_GSM_serial_communication()
{

    Serial.println("Attempting to setup GSM connection...");

    pinMode(QUECTEL_PWR_KEY, OUTPUT);
    digitalWrite(QUECTEL_PWR_KEY, HIGH);
    delay(10000); // Give module enough time to boot up  //! fona timeout does not behave as expected
    const uint8_t GSM_RETRY_ATTEMPTS = 3;
    uint8_t retry_count = 1;
    while (retry_count <= GSM_RETRY_ATTEMPTS)
    {

        Serial.println("ATTEMPT: " + String(retry_count));
        if (fona.begin(fonaSS))
        {
            Serial.println("GSM module found!");
            GSM_CONNECTED = true;
            return true;
        }
        retry_count++;
        if (retry_count == GSM_RETRY_ATTEMPTS)
        {
            Serial.println("Exceeded maximum number of attempts to connect to GSM module. Exiting retries...");
        }
    }

    Serial.println("Could not find GSM module");

    return false;
}

bool GSM_init()
{
    Serial.println("GSM INITIALIZATION....");

    // Check if SIM is inserted
    if (!is_SIMCID_valid())
    {
        memset(error_buffer, '\0', error_buff_size);

        const char *data = "Could not get SIM CID";
        strncpy(error_buffer, data, error_buff_size - 1);
        Serial.println(error_buffer);
        return false;
    }

    // SIM setup
    Serial.println("Setting up SIM PIN..");

    SIM_PIN_Setup();

    if (!SIM_PIN_SET)
    {
        memset(error_buffer, '\0', error_buff_size);

        const char *data = "Unable to set SIM PIN";
        strncpy(error_buffer, data, error_buff_size - 1);
        Serial.println(error_buffer);
        return false;
    }
    // Set if SIM is usable flag
    SIM_USABLE = true;

    // Register to network
    bool registered_to_network = false;
    int retry_count = 0;
    while (!registered_to_network && retry_count < 10)
    {
        if (fona.getNetworkStatus() == 1 || 5)
            registered_to_network = true;

        retry_count++;
        delay(3000);
    }

    if (!registered_to_network)
    {
        memset(error_buffer, '\0', error_buff_size);

        const char *data = "Could not register to network";
        strncpy(error_buffer, data, error_buff_size - 1);
        Serial.println(error_buffer);
        return false;
    }

    // fona.sendCheckReply(F("AT+CREG?"), F("+CREG: "));
    get_raw_response("AT+CREG?", raw_gsm_response, 3000);
    get_raw_response("AT+COPS?", raw_gsm_response, 3000);

    // fona.sendCheckReply(F("AT+COPS?"), F("OK"));

    // Set GPRS APN details
    // fona.setGPRSNetworkSettings(F(GPRS_APN), F(GPRS_USERNAME), F(GPRS_PASSWORD));

    // Attempt to enable GPRS
    // Serial.println("Attempting to enable GPRS");
    // // delay(2000);

    // if (!GPRS_init())
    //     return false;

    // Serial.println("GPRS enabled!");

    // GPRS_CONNECTED = true;
    // ToDo: Attempt to do a ping test to determine whether we can communicate with the internet

    return true;
}

static void unlock_pin(char *PIN)
{
    // flushSerial();

    // Attempt to SET PIN if not empty
    Serial.print("GSM CONFIG SET PIN: ");
    Serial.println(PIN);
    Serial.print("Length of PIN");
    Serial.println(strlen(PIN));
    if (strlen(PIN) > 1)
    {
        // debug_outln(F("\nAttempting to Unlock SIM please wait: "), DEBUG_MIN_INFO);
        Serial.print("Attempting to unlock SIM using PIN: ");
        Serial.println(PIN);
        if (!fona.unlockSIM(PIN))
        {
            // debug_outln(F("Failed to Unlock SIM card with pin: "), DEBUG_MIN_INFO);
            Serial.print("Failed to Unlock SIM card with PIN: ");
            // debug_outln(gsm_pin, DEBUG_MIN_INFO);
            Serial.println(PIN);
            SIM_PIN_SET = false;
            return;
        }

        SIM_PIN_SET = true;
    }
}

char get_raw_response(const char *cmd, char *res_buff, int timeout)
{

    // while (fonaSS.available())
    // {
    //     fonaSS.read();
    // }
    flushSerial();
    delay(100);
    Serial.print("Received Command in get raw: ");
    Serial.print(cmd);
    fonaSS.println(cmd);
    int sendStartMillis = millis();
    do
    {
        if (fonaSS.available())
        {
            memset(res_buff, '\0', gsm_buff_size);
            fonaSS.readBytes(res_buff, gsm_buff_size - 1);
            break;
        }

        delay(2);
    } while (res_buff == "" || (millis() - sendStartMillis < timeout));
    Serial.println("\n-------\nGSM RAW RESPONSE:\n");
    Serial.println(res_buff);
    Serial.println("-------");

    return *res_buff;
}
String handle_AT_CMD(String cmd, int _delay)
{
    while (Serial.available() > 0)
    {
        Serial.read();
    }
    String RESPONSE = "";
    fonaSS.println(cmd);
    int sendStartMillis = millis();
    // delay(_delay); // Avoid putting any code that might delay the receiving all contents from the serial buffer as it is quickly filled up
    do
    {
        if (fonaSS.available())
        {
            RESPONSE += fonaSS.readString();
        }

        delay(2);
    } while (RESPONSE == "" || (millis() - sendStartMillis < _delay));

    Serial.println();
    Serial.println("GSM RESPONSE:");
    Serial.println("-------");
    Serial.print(RESPONSE);
    Serial.println("-----");

    return RESPONSE;
}

void SIM_PIN_Setup()
{

    // String res = handle_AT_CMD("AT+CPIN?");
    // int start_index = res.indexOf(":");
    // res = res.substring(start_index + 1);
    // res.trim();
    // Serial.print("PIN STATUS: ");
    // Serial.println(res);
    // if (res.startsWith("READY"))
    // {
    //     SIM_PIN_SET = true;
    //     return;
    // }

    // else if (res.startsWith("SIM PIN"))
    // {
    //     unlock_pin(SIM_PIN);
    //     return;
    // }
    // else if (res.startsWith("SIM PUK"))
    // { // ToDo: Attempt to set PUK;
    //     return;
    // }

    char pin_status[64] = "";
    get_raw_response("AT+CPIN?", raw_gsm_response, 15000);
    strncpy(pin_status, raw_gsm_response, 63);

    // if (fona.sendCheckReply(F("AT+CPIN?"), F("+CPIN: READY"), 15000))
    if (String(pin_status).indexOf("ERROR") > -1)
    {
        Serial.println("SIM PIN READY");
        SIM_PIN_SET = true;
        return;
    }

    else
    {
        Serial.println("SIM PIN NOT SET");
        return;
    }
}

bool is_SIMCID_valid() // ! Seems to be returning true even when there is "ERROR" in response
{
    // char res[30];
    // fona.getSIMCCID(res);
    // Serial.println(res);
    // String ccid = String(res);
    char cmd[] = "AT+QCCID";
    get_raw_response(cmd, raw_gsm_response, 1000);
    char ccid[65];
    strncpy(ccid, raw_gsm_response + 8, 20);

    // String ccid = handle_AT_CMD("AT+CCID", 10000);
    if (String(ccid).indexOf("ERROR") > -1) // Means string has the word error
    {
        SIM_AVAILABLE = false;
        return false;
    }

    else
    {
        // strcpy(SIM_CID, res);
        SIM_AVAILABLE = true;
        Serial.println("SIM card available");
        Serial.print("SIM CCID: ");
        Serial.println(ccid);
        return true;
    }
}

// Similar to FONA enableGPRS() but quicker because APN setting are not configured as it is configured during GSM_init()
bool GPRS_init()
{

    // String err = "";

    // if (!fona.sendCheckReply(F("AT+CGATT=1"), F("OK"), 10000))
    // {
    //     err = "Failed to attach GPRS service";
    //     GSM_INIT_ERROR = err;
    //     Serial.println(err);
    //     GPRS_CONNECTED = false;
    //     return GPRS_CONNECTED;
    // }
    // if (fona.sendCheckReply(F("AT+CGATT?"), F("0"), 65000)) // equivalent to fona.GPRSstate()
    // {
    //     if (!fona.sendCheckReply(F("AT+CGATT=1"), F("OK"), 65000))
    //     {
    //         memset(error_buffer, '\0', error_buff_size);

    //         const char *data = "Failed to attach GPRS service";
    //         strncpy(error_buffer, data, error_buff_size - 1);
    //         Serial.println(error_buffer);
    //         GPRS_CONNECTED = false;
    //         return GPRS_CONNECTED;
    //     }
    // }

#ifdef QUECTEL
    Serial.println("Quectel GPRS init...");

    // if (!fona.sendCheckReply(F("AT+QICSGP=1,1"), F("OK"), 10000))
    // {
    //     memset(error_buffer, '\0', error_buff_size);

    //     const char *data = "Failed to config GPRS PDP context";
    //     strncpy(error_buffer, data, error_buff_size - 1);
    //     Serial.println(error_buffer);

    //     GPRS_CONNECTED = false;
    //     return GPRS_CONNECTED;
    // }

    // char cmd[] = "AT+QICSGP=1,1";
    char cmd[] = "AT+QICSGP=?";
    get_raw_response(cmd, raw_gsm_response, 10000);

    char qiscgp[] = "AT+QICSGP=1,1";

    get_raw_response(qiscgp, raw_gsm_response, 10000);
    // Check if QIACT is already active

    get_raw_response("AT+QIACT?", raw_gsm_response, 10000);

    if (String(raw_gsm_response).indexOf("1") > 1)
    {
        Serial.println("GPRS PDP context already activated");
    }
    else
    {
        get_raw_response("AT+QIACT=1", raw_gsm_response, 10000);

        if (String(raw_gsm_response).indexOf("OK") > 1)
        {
            Serial.println("GPRS PDP context has been activated");
        }
        else
        {

            memset(error_buffer, '\0', error_buff_size);

            const char *data = "Failed to activate GPRS PDP context";
            strncpy(error_buffer, data, error_buff_size - 1);
            Serial.println(error_buffer);

            GPRS_CONNECTED = false;
            return GPRS_CONNECTED;
        }
    }

    // if (!fona.sendCheckReply(F("AT+QIACT?"), F("1"), 3000))
    // {
    //     Serial.println("Activating GPRS PDP context");
    //     flushSerial();
    //     if (!fona.sendCheckReply(F("AT+QIACT=1"), F("OK"), (uint16_t)150000))
    //     {
    //         memset(error_buffer, '\0', error_buff_size);

    //         const char *data = "Failed to activate GPRS PDP context";
    //         strncpy(error_buffer, data, error_buff_size - 1);
    //         Serial.println(error_buffer);

    //         GPRS_CONNECTED = false;
    //         return GPRS_CONNECTED;
    //     }
    // }
    // else
    // {
    //     Serial.println("GPRS PDP context already active");
    // }

#else
    String res = handle_AT_CMD("AT+SAPBR=1,1"); // Enable GPRS
    res = handle_AT_CMD("AT+QCFG=\"gprsattach\",1");
    if (res.indexOf("OK") == -1)
    {
        err = "Failed to enable GPRS";
        GSM_INIT_ERROR = err;
        Serial.println(err);
        GPRS_CONNECTED = false;
        return GPRS_CONNECTED;
    }
#endif

    GPRS_CONNECTED = true;
    return GPRS_CONNECTED;
}

void GSM_soft_reset()
{
    // #ifdef QUECTEL
    //     // ! Observation per v1 of Quectel PCB is that it POWERS BACK ON immediately after sending POWER DOWN command
    //     if (fona.sendCheckReply(F("AT+QPOWD"), F("POWERED DOWN")))
    //     {
    //         Serial.println("Restarting QUECTEL GSM");
    //         delay(10000); // Give module enough time to register to network
    //     }
    //     else
    //     {
    //         Serial.println("Failed to power down Quectel module");
    //     }

    // #else
    fona.enableGPRS(false); // basically shut down GPRS service

    if (!fona.sendCheckReply(F("AT+CFUN=1,1"), F("OK")))
    {
        Serial.println("Soft resetting GSM with full functionality failed!");
        return;
    }
    Serial.println("Soft resetting the GSM module...");
    delay(30000); // wait for GSM to warm up
    // #endif

    // if (!GSM_init(fonaSerial))
    // {
    //     Serial.println("GSM not fully configured");
    //     Serial.print("Failure point: ");
    //     Serial.println(GSM_INIT_ERROR);
    //     Serial.println();
    // }
}

/***
 * ? Called 3 times. Review the impelementation of this
 * Todo: Change implementation to shut down GSM and then call GSM_init();
 *
 *
 ***/
void restart_GSM()
{
    Serial.println("Restarting GSM");
    //! The AQ PCB board has the GSM reset physically connected to the ESP chip
    // GSM_soft_reset();
    // if (!fona.begin(*fonaSerial))
    // {
    //     Serial.println("Couldn't find GSM");
    //     return;
    // }

    if (!GSM_init())
    {
        Serial.println("GSM not fully configured");
        Serial.print("Failure point: ");
        Serial.println(error_buffer);
        Serial.println();
    }
}

void enableGPRS()
{
    // fona.setGPRSNetworkSettings(FONAFlashStringPtr(gprs_apn), FONAFlashStringPtr(gprs_username), FONAFlashStringPtr(gprs_password));

    int retry_count = 0;
    while ((fona.GPRSstate() != 0) && (retry_count < 40))
    {
        delay(3000);
        fona.enableGPRS(true);
        retry_count++;
    }

    fona.enableGPRS(true);
}

void disableGPRS()
{
    fona.enableGPRS(false);
    GPRS_CONNECTED = false;
}

/*****************************************************************
/* flushSerial                                                   *
/*****************************************************************/
void flushSerial()
{
    Serial.print("\nEmptying fona serial if any..");
    while (fonaSS.available())
    {

        Serial.print(fonaSS.read());
        delay(10);
    }
    Serial.println();
}

/// @brief Easy implementation of Quectel HTTP functionality
/// @param url url for http request sans protocol
/// @param headers array of request headers
/// @param header_size size of the headers array
/// @param data post body data
/// @param data_length length of the data
void QUECTEL_POST(char *url, String headers[], int header_size, const String &data, int data_length)
{
    /* SETTING request headers
    ! Headers are sent in two formats
    1. Format 0: headers are sent before post body
    2. Format 1: headers are sent as part of the body
    */

    // Using format 0

    // Config URL
    // String HTTP_SETUP = "AT+QHTTPURL=" + String(strlen(url), DEC) + ",10,60";

    String HTTP_CFG = "AT+QHTTPCFG=\"url\",\"http://" + String(url) + "\""; // protocol must be set before URL
    char httpcfg[1024] = "AT+QHTTPCFG=\"url\",\"http://";
    strcat(httpcfg, url);
    strcat(httpcfg, "\"");

    Serial.print("Quectel URL config: ");
    Serial.println(httpcfg);
    // handle_AT_CMD(HTTP_CFG);
    get_raw_response(httpcfg, raw_gsm_response, 3000);
    get_raw_response("AT+QHTTPCFG=\"contextid\",1", raw_gsm_response, 3000);
    get_raw_response("AT+QHTTPCFG=\"requestheader\",0", raw_gsm_response, 3000);
    get_raw_response("AT+QHTTPCFG=\"responseheader\",1", raw_gsm_response, 3000);
    get_raw_response("AT+QHTTPCFG=\"rspout/auto\",1", raw_gsm_response, 3000);

    // fona.sendCheckReply(F("AT+QHTTPCFG=\"contextid\",1"), F("OK"), 5000); // set context id
    // fona.sendCheckReply(F("AT+QHTTPCFG=\"requestheader\",0"), F("OK"), 5000); // disable request headers
    // fona.sendCheckReply(F("AT+QHTTPCFG=\"responseheader\",1"), F("OK"), 5000); // enable response headers
    // fona.sendCheckReply(F("AT+QHTTPCFG=\"rspout/auto\",1"), F("OK"), 5000); // enable auto response and "disable" HTTTPREAD

    for (int i = 0; i < header_size; i++)
    {

        memset(httpcfg, '\0', sizeof(httpcfg));
        // HTTP_CFG = "AT+QHTTPCFG=\"header\",\"" + headers[i] + "\"";
        const char *header = headers[i].c_str();
        char header_data[strlen(header)];
        strcpy(header_data, header);

        strncpy(httpcfg, "AT+QHTTPCFG=\"header\",\"", 64);
        strcat(httpcfg, header);
        strcat(httpcfg, "\"");

        // Serial.println(HTTP_CFG);
        // fonaSerial->println(HTTP_CFG);
        // handle_AT_CMD(HTTP_CFG);
        get_raw_response(httpcfg, raw_gsm_response, 3000);
    }

    // POST data
    memset(httpcfg, '\0', sizeof(httpcfg));
    // HTTP_CFG = "AT+QHTTPPOST=" + String(data_length) + ",30,60";
    strncpy(httpcfg, "AT+QHTTPPOST=", 64);
    char len[8];
    itoa(data_length, len, 10);
    strcat(httpcfg, len);
    strcat(httpcfg, ",30,60");
    // Serial.println(HTTP_CFG);
    Serial.println(httpcfg);
    // fonaSerial->println(HTTP_CFG);
    // String res = handle_AT_CMD(HTTP_CFG);
    get_raw_response(httpcfg, raw_gsm_response, 3000);
    // if (res.indexOf("OK") == -1)
    // {
    //     HTTP_POST_FAIL += 1;
    //     if (HTTP_POST_FAIL > 5)
    //     {
    //         HTTP_POST_FAIL = 0;
    //         GSM_soft_reset();
    //     }
    // }
    Serial.print("Quectel post body: ");
    // Serial.println(data);
    const char *data_copy = data.c_str();
    char my_data[strlen(data_copy)];
    strcpy(my_data, data_copy);
    Serial.println(my_data);
    get_raw_response(my_data, raw_gsm_response, 10000);
    // res = handle_AT_CMD(data, 10000);
    // if (res.indexOf("OK") == -1)
    // {
    //     HTTP_POST_FAIL += 1;
    //     if (HTTP_POST_FAIL > 5)
    //     {
    //         HTTP_POST_FAIL = 0;
    //         GSM_soft_reset();
    //     }
    // }
}

// Testing data
// http://staging.api.sensors.africa/v1/push-sensor-data/

// POST /v1/push-sensor-data/\r\nHost: http://staging.api.sensors.africa\r\nAccept: */*\r\nUser-Agent: QUECTEL EC200\r\nContent-Type: application/json\r\nX-Sensor: esp8266-15355455\r\nX-PIN: 1\r\nContent-Length: 385\r\n\r\n{"software_version": "NRZ-2020-129", "sensordatavalues":[{"value_type":"P0","value":"7.80"},{"value_type":"P1","value":"10.50"},{"value_type":"P2","value":"13.40"}]}\r\n
// data length 252

// Accept: */*\r\nUser-Agent: QUECTEL EC200\r\nContent-Type: application/json\r\nX-Sensor: esp8266-15355455\r\nX-PIN: 1\r\nContent-Length: 165\r\n\r\n{"software_version": "NRZ-2020-129", "sensordatavalues":[{"value_type":"P0","value":"7.80"},{"value_type":"P1","value":"10.50"},{"value_type":"P2","value":"13.40"}]}\r\n
/// 1234

// AT commands sequence

// AT+CGATT=1
// AT+QICSGP=1,1,"safaricom","saf","data"
// AT+QIACT=1
// AT+QIACT?
// AT+QHTTPCFG="contextid",1
// AT+QHTTPCFG="requestheader",1
// AT+QHTTPCFG="responseheader",1
// AT+QHTTPURL=54,30,60
// http://staging.api.sensors.africa/v1/push-sensor-data/
// AT+QHTTPPOST=385,30,60
// AT+QHTTPREAD
