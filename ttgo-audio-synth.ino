

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

#define APPLEMIDI_INITIATOR
#include <AppleMIDI.h>// V2.1.0
USING_NAMESPACE_APPLEMIDI

#include <NeoPixelBus.h>// V2.5.7

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <WM8978.h> //https://github.com/CelliesProjects/wm8978-esp32/ V1.0
#include "untitled.h"

#define I2C_SCL 18
#define I2C_SDA 19

#define I2S_BCK     33
#define I2S_WS      25
#define I2S_DOUT    26
#define I2S_DIN     27
#define I2S_MCLKPIN 0
#define I2S_MFREQ  (24 * 1000 * 1000)

untitled* DSP = new untitled(48000, 32);

double gfrequency = 0;
double gvelocity = 0;

char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";



int show_debug = 0;

// NEOPIXEL SETUP
#define PIN            22

#define NUMPIXELS      19

//C1
#define LOWEST_NOTE   24


#include <ETH.h>
#include <ESPmDNS.h>

static bool eth_connected = false;


static double midiToFreq(double note)
{
  return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}


void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

bool ETH_startup()
{
  WiFi.onEvent(WiFiEvent);
  ETH.begin();

  Serial.println(F("Getting IP address..."));

  while (!eth_connected)
    delay(100);

  return true;
}

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0800KbpsMethod> pixels (NUMPIXELS, PIN);

unsigned long t0 = millis();
bool isConnected = false;

APPLEMIDI_CREATE_INSTANCE(WiFiUDP, MIDI, "ESP32", DEFAULT_CONTROL_PORT);


void setAllLeds(int v)
{
  for (int i = 0; i < NUMPIXELS; i++)
  {
    pixels.SetPixelColor(i, RgbColor(v, v, v));

  }
  pixels.Show();
}



// ====================================================================================
// Event handlers for incoming MIDI messages
// ====================================================================================

void OnAppleMidiConnected(const ssrc_t & ssrc, const char* name) {
  isConnected = true;
  Serial.print(F("Connected to session "));
  Serial.println(name);
}

void OnAppleMidiDisconnected(const ssrc_t & ssrc) {
  isConnected = false;
  Serial.println(F("Disconnected"));
}

void OnAppleMidiError(const ssrc_t& ssrc, int32_t err) {
  Serial.print  (F("Exception "));
  Serial.print  (err);
  Serial.print  (F(" from ssrc 0x"));
  Serial.println(ssrc, HEX);

  switch (err)
  {
    case Exception::NoResponseFromConnectionRequestException:
      Serial.println(F("xxx:yyy did't respond to the connection request. Check the address and port, and any firewall or router settings. (time)"));
      break;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void OnAppleMidiNoteOn(byte channel, byte note, byte velocity) {
  /*
    Serial.print(F("Incoming NoteOn from channel:"));
    Serial.print(channel);
    Serial.print(F(" note:"));
    Serial.print(note);
    Serial.print(F(" velocity:"));
    Serial.print(velocity);
    Serial.println();
  */

  int i = note - LOWEST_NOTE;
  if (i < 0)
  {
    i = 0;
  }

  if (show_debug)
  {
    Serial.println(i);
  }

  if (i < NUMPIXELS)
  {
    pixels.SetPixelColor(i, RgbColor(50, 40, 30));
    pixels.Show();
  }

  gfrequency = midiToFreq(note);
  gvelocity = 1;
  DSP->setParamValue("freq", gfrequency);
  //DSP->setParamValue("gate", gvelocity);
  //DSP->setParamValue("/gate", gvelocity);
  //DSP->setParamValue("/synth/gate", gvelocity);
  DSP->setParamValue("gain", gvelocity);
  //Serial.println(gfrequency);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void OnAppleMidiNoteOff(byte channel, byte note, byte velocity) {
  /*
    Serial.print(F("Incoming NoteOff from channel:"));
    Serial.print(channel);
    Serial.print(F(" note:"));
    Serial.print(note);
    Serial.print(F(" velocity:"));
    Serial.print(velocity);
    Serial.println();
  */
  int i = note - LOWEST_NOTE;
  if (i < 0)
  {
    i = 0;
  }

  if (i < NUMPIXELS)
  {
    pixels.SetPixelColor(i, RgbColor(0, 0, 0));
    pixels.Show();
  }

  if (gfrequency != midiToFreq(note))
  {
    //we received a note on message from a different note in the meantime...
    return;
  }

  gvelocity = 0;
  //DSP->setParamValue("gate", gvelocity);
  //DSP->setParamValue("/gate", gvelocity);
  //DSP->setParamValue("/synth/gate", gvelocity);
  DSP->setParamValue("gain", gvelocity);
}


void OnAppleMidiByte(const ssrc_t & ssrc, byte data) {
  Serial.print(F("raw MIDI: "));
  Serial.println(data);
}




// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void setup()
{
  // Serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  //pinMode(TRANSISTOR_PIN, OUTPUT);


  Serial.print(F("Getting IP address..."));


  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println(F(""));
  Serial.println(F("WiFi connected"));


  Serial.println();
  Serial.print(F("IP address is "));
  Serial.println(WiFi.localIP());

  // SETUP NeoPixel
  pixels.Begin();
  delay(10);
  pixels.Show();


  Serial.println(F("OK, now make sure you an rtpMIDI session that is Enabled"));
  Serial.print(F("Add device named ESP32 with Host/Port "));
  Serial.print(WiFi.localIP());
  Serial.println(F(":5004"));


  WM8978 wm8978;

if (!wm8978.begin(I2C_SDA, I2C_SCL)) {
    ESP_LOGE(TAG, "Error setting up dac. System halted");
    while (1) delay(100);
  }

  /* Setup wm8978 MCLK on gpio - for example M5Stack Node needs this clock on gpio 0 */
  double retval = wm8978.setPinMCLK(I2S_MCLKPIN, I2S_MFREQ);
  if (!retval)
    ESP_LOGE(TAG, "Could not set %.2fMHz clock signal on GPIO %i", I2S_MFREQ / (1000.0 * 1000.0), I2S_MCLKPIN);
  else
    ESP_LOGI(TAG, "Set %.2fMHz clock signal on GPIO %i", retval / (1000.0 * 1000.0), I2S_MCLKPIN);

  wm8978.setHPvol(40, 40);
  //wm8978.spkVolSet(60); // [0-63]

  DSP->setParamValue("gain", 0);
  DSP->start();

  MDNS.begin(AppleMIDI.getName());

  MIDI.begin(1); // listen on channel 1

  AppleMIDI.setHandleConnected(OnAppleMidiConnected);
  AppleMIDI.setHandleDisconnected(OnAppleMidiDisconnected);
  AppleMIDI.setHandleError(OnAppleMidiError);


  MDNS.addService("apple-midi", "udp", AppleMIDI.getPort());

  MIDI.setHandleNoteOn(OnAppleMidiNoteOn);
  MIDI.setHandleNoteOff(OnAppleMidiNoteOff);

  //AppleMIDI.setHandleReceivedMidi(OnAppleMidiByte);

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void loop()
{
  // Listen to incoming notes
  MIDI.read();

  
  //AppleMIDI.sendNoteOn(note, velocity, channel);
  //AppleMIDI.sendNoteOff(note, velocity, channel);
}
