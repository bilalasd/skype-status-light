#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#define LED_PIN D2
#define NUMPIXELS 12

#define DELAYVAL 500

double CIRCULAR_WAVE_ANIMATION_CONSTANT = 0.6; //percent
double SINE_WAVE_VERTICAL_FACTOR = 1.0;
double LIGHT_LOOP_TIME = 5.0; //seconds
double TIME_TO_FADE = 5.0;    //seconds
double DEGREES_PER_MS_PER_LOOP = 360.0 / ((double)LIGHT_LOOP_TIME * 1000);

const char *ssid = "hpeguest";
const char *password = "";
const char *mqtt_server = "35.227.168.166";
const long utcOffsetInSeconds = -21600;

WiFiClient wifiClient;
ESP8266WiFiMulti WiFiMulti;
PubSubClient mqttClient(wifiClient);
WiFiUDP wifiUDP;
NTPClient timeClient(wifiUDP, "pool.ntp.org", utcOffsetInSeconds);

const uint8_t PROGMEM gamma8[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
    2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
    5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
    10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
    17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
    25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
    37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
    51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
    69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
    90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
    115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
    144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
    177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255};

double currentDEGREES_PER_MS_PER_LOOP = 0.0;

double sineDegreeCounter = 0;
int lastMillis = 0;
int currentMillis = 0;

double fadeCoefficient_Red = 0;
double fadeCoefficient_Blue = 0;
double fadeCoefficient_Green = 0;

double setRed = 0;
double setBlue = 0;
double setGreen = 0;

double fadeStepSizeRed = 0;
double fadeStepSizeBlue = 0;
double fadeStepSizeGreen = 0;

double BUSY_LIGHT_COLORS[] = {1, 0, 0};
double AWAY_LIGHT_COLORS[] = {1, 0.7, 0};
double FREE_LIGHT_COLORS[] = {0, 1, 0};
double OFF_LIGHT_COLORS[] = {0, 0, 0};

String currentStatus = "free";

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

double getWaveCoefficient(int phase);
double deg2Rad(double deg);
void setColor(double red, double green, double blue);
void fadeColorLoop(int millis);
void connectToWifi();
void checkIfConnectedToInternet();
void mqttMessageReceived(char *topic, byte *payload, unsigned int length);
void reconnectMQTT();
void connectTohpeguest();
void setStatus();
void setColor(double color[]);

void setup()
{
  pixels.begin();

  Serial.begin(115200);

  connectToWifi();

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttMessageReceived);

  timeClient.begin();

  timeClient.update();
}

void loop()
{
  pixels.clear(); // Set all pixel colors to 'off'

  currentMillis = (int)millis();

  fadeColorLoop(currentMillis - lastMillis);

  currentDEGREES_PER_MS_PER_LOOP += DEGREES_PER_MS_PER_LOOP * (currentMillis - lastMillis);
  if (currentDEGREES_PER_MS_PER_LOOP > 360.0)
  {
    currentDEGREES_PER_MS_PER_LOOP -= 360;
  }

  if (!mqttClient.connected())
  {
    reconnectMQTT();
  }
  mqttClient.loop();

  for (int i = 0; i < NUMPIXELS; i++)
  {
    double waveCoefficient = getWaveCoefficient(((double)360 / (double)NUMPIXELS) * (double)i);

    int red = pgm_read_byte(&gamma8[(int)(fadeCoefficient_Red * waveCoefficient * 255)]);
    int green = pgm_read_byte(&gamma8[(int)(fadeCoefficient_Green * waveCoefficient * 255)]);
    int blue = pgm_read_byte(&gamma8[(int)(fadeCoefficient_Blue * waveCoefficient * 255)]);

    pixels.setPixelColor(i, pixels.Color(red, green, blue));

    // Serial.print("currentMillis: ");
    // Serial.print(currentMillis);
    // Serial.print(" WC: ");
    // Serial.print(waveCoefficient, 5);
    // Serial.print(" [");
    // Serial.print(red);
    // Serial.print(", ");
    // Serial.print(green);
    // Serial.print(", ");
    // Serial.print(blue);
    // Serial.println("]");
  }
  pixels.show();

  lastMillis = currentMillis;

  // delay(100);
}

double getWaveCoefficient(int phase)
{

  double sinValue = (SINE_WAVE_VERTICAL_FACTOR * sin(deg2Rad(currentDEGREES_PER_MS_PER_LOOP + phase))) - (SINE_WAVE_VERTICAL_FACTOR - 1);
  double returnVal = 0;

  if (sinValue > CIRCULAR_WAVE_ANIMATION_CONSTANT)
  {
    returnVal = sinValue;
  }
  else
  {
    returnVal = CIRCULAR_WAVE_ANIMATION_CONSTANT;
  }

  // Serial.print(" currentDEGREES_PER_MS_PER_LOOP:");
  // Serial.print(currentSineTimeValue, 5);
  // Serial.print(" sinValue:");
  // Serial.print(sinValue, 5);
  // Serial.print(" returnVal:");
  // Serial.println(returnVal, 5);
  // Serial.print("]    ");

  return returnVal;
}

double deg2Rad(double deg)
{
  return (deg * 1000 / 57296);
}

void fadeColorLoop(int millis)
{
  if (fadeStepSizeRed < 0)
  {
    //Red Going Down
    if (fadeCoefficient_Red >= setRed)
    {
      fadeCoefficient_Red += ((double)fadeStepSizeRed * (double)millis);
    }

    //If the current value goes past the set value, set it to set value
    if (fadeCoefficient_Red < setRed)
    {
      fadeStepSizeRed = 0;
      fadeCoefficient_Red = setRed;
    }
  }
  else
  {
    //Red Going Up
    if (fadeCoefficient_Red <= setRed)
    {
      fadeCoefficient_Red += ((double)fadeStepSizeRed * (double)millis);
    }

    //If the current value goes past the set value, set it to set value
    if (fadeCoefficient_Red > setRed)
    {
      fadeStepSizeRed = 0;
      fadeCoefficient_Red = setRed;
    }
  }

  if (fadeStepSizeGreen < 0)
  {
    //Green Going Down
    if (fadeCoefficient_Green >= setGreen)
    {
      fadeCoefficient_Green += ((double)fadeStepSizeGreen * (double)millis);
    }

    //If the current value goes past the set value, set it to set value
    if (fadeCoefficient_Green < setGreen)
    {
      fadeStepSizeGreen = 0;
      fadeCoefficient_Green = setGreen;
    }
  }
  else
  {
    //Green Going Up
    if (fadeCoefficient_Green <= setGreen)
    {
      fadeCoefficient_Green += ((double)fadeStepSizeGreen * (double)millis);
    }

    //If the current value goes past the set value, set it to set value
    if (fadeCoefficient_Green > setGreen)
    {
      fadeStepSizeGreen = 0;
      fadeCoefficient_Green = setGreen;
    }
  }

  if (fadeStepSizeBlue < 0)
  {
    //Blue Going Down
    if (fadeCoefficient_Blue >= setBlue)
    {
      fadeCoefficient_Blue += ((double)fadeStepSizeBlue * (double)millis);
    }

    //If the current value goes past the set value, set it to set value
    if (fadeCoefficient_Blue < setBlue)
    {
      fadeStepSizeBlue = 0;
      fadeCoefficient_Blue = setBlue;
    }
  }
  else
  {
    //Blue Going Up
    if (fadeCoefficient_Blue <= setBlue)
    {
      fadeCoefficient_Blue += ((double)fadeStepSizeBlue * (double)millis);
    }

    //If the current value goes past the set value, set it to set value
    if (fadeCoefficient_Blue > setBlue)
    {
      fadeStepSizeBlue = 0;
      fadeCoefficient_Blue = setBlue;
    }
  }
}

void setColor(double color[])
{
  setRed = color[0];
  setGreen = color[1];
  setBlue = color[2];

  fadeStepSizeRed = ((double)setRed - (double)fadeCoefficient_Red) / ((double)TIME_TO_FADE * 1000.0);
  fadeStepSizeGreen = ((double)setGreen - (double)fadeCoefficient_Green) / ((double)TIME_TO_FADE * 1000.0);
  fadeStepSizeBlue = ((double)setBlue - (double)fadeCoefficient_Blue) / ((double)TIME_TO_FADE * 1000.0);
}

void setStatus()
{
  if (currentStatus == "busy")
  {
    setColor(BUSY_LIGHT_COLORS);
  }
  else if (currentStatus == "away")
  {
    setColor(AWAY_LIGHT_COLORS);
  }
  else if (currentStatus == "free")
  {
    setColor(FREE_LIGHT_COLORS);
  }
  else if (currentStatus == "off")
  {
    setColor(OFF_LIGHT_COLORS);
  }
}

// Serial.print("setColor[");
// Serial.print(setRed, 5);
// Serial.print(",");
// Serial.print(setGreen, 5);
// Serial.print(",");
// Serial.print(setBlue, 5);
// Serial.print("]    ");

// Serial.print("currentColor[");
// Serial.print(fadeCoefficient_Red, 5);
// Serial.print(",");
// Serial.print(fadeCoefficient_Green, 5);
// Serial.print(",");
// Serial.print(fadeCoefficient_Blue, 5);
// Serial.print("]    ");

// Serial.print("fadeStepSizes[");
// Serial.print(fadeStepSizeRed, 5);
// Serial.print(",");
// Serial.print(fadeStepSizeGreen, 5);
// Serial.print(",");
// Serial.print(fadeStepSizeBlue, 5);
// Serial.print("]    ");

// Serial.print("Millis: ");
// Serial.print(millis);

void connectToWifi()
{
  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");

  while (WiFiMulti.run() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

void reconnectMQTT()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "light-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("mqtt connected");
      mqttClient.subscribe("status");
      mqttClient.subscribe("initialSettings");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      checkIfConnectedToInternet();
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void checkIfConnectedToInternet()
{
  HTTPClient http;

  String websiteToCheckInternet = "http://jsonplaceholder.typicode.com/posts/1";

  Serial.println("Checking if connected to internet!");
  Serial.println("Making a request to: " + websiteToCheckInternet);

  http.begin(wifiClient, websiteToCheckInternet);
  int httpCode = http.GET();
  String payload = http.getString();

  //Check if the data downloaded is correct
  if (payload.indexOf("userId") > 0)
  {
    Serial.println("Connected to the internet!");
  }
  else
  {
    Serial.println("Not connected to the internet!");
    connectTohpeguest();
  }
}

void connectTohpeguest()
{
  Serial.println("Making a request to connect to hpeguest");
  HTTPClient http;

  //  Serial.print("[HTTP] begin...\n");
  http.begin(wifiClient, "http://securewireless.hpe.com/cgi-bin/login");

  http.addHeader("Host", "securewireless.hpe.com");
  http.addHeader("Connection", "keep-alive");
  http.addHeader("Content-Length", "215");
  http.addHeader("Cache-Control", "max-age=0");
  http.addHeader("Origin", "https://cce02cpsub07.houston.hpe.com");
  http.addHeader("Upgrade-Insecure-Requests", "1");
  http.addHeader("DNT", "1");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/74.0.3729.169 Safari/537.36");
  http.addHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3");
  http.addHeader("Referer", "https://cce02cpsub07.houston.hpe.com/guest/hpe_guest.php?_browser=1");
  http.addHeader("Accept-Encoding", "gzip, deflate, br");
  http.addHeader("Accept-Language", "en-US,en;q=0.9");

  int httpCode = http.POST("user=HPE+Guest&password=125487&cmd=authenticate&url=https%3A%2F%2Fwww.google.com&Login=Log+In");

  String payload = http.getString(); //Get the response payload

  http.end();
}

void mqttMessageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String msg = "";
  for (int i = 0; i < length; i++)
  {
    msg += (char)payload[i];
  }
  Serial.println(msg);
  if (strcmp(topic, "initialSettings") == 0)
  {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, msg);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }

    for (int i = 0; i < 3; i++)
    {
      BUSY_LIGHT_COLORS[i] = doc["BUSY_LIGHT_COLORS"][i].as<double>();
      AWAY_LIGHT_COLORS[i] = doc["AWAY_LIGHT_COLORS"][i].as<double>();
      FREE_LIGHT_COLORS[i] = doc["FREE_LIGHT_COLORS"][i];
      OFF_LIGHT_COLORS[i] = doc["OFF_LIGHT_COLORS"][i].as<double>();
    }
    Serial.print(FREE_LIGHT_COLORS[0]);
    Serial.print(FREE_LIGHT_COLORS[1]);
    Serial.println(FREE_LIGHT_COLORS[2]);

    CIRCULAR_WAVE_ANIMATION_CONSTANT = doc["CIRCULAR_WAVE_ANIMATION_CONSTANT"].as<double>(); //percent
    SINE_WAVE_VERTICAL_FACTOR = doc["SINE_WAVE_VERTICAL_FACTOR"].as<double>();
    LIGHT_LOOP_TIME = doc["LIGHT_LOOP_TIME"].as<double>(); //seconds
    TIME_TO_FADE = doc["TIME_TO_FADE"].as<double>();       //seconds
    DEGREES_PER_MS_PER_LOOP = 360.0 / ((double)LIGHT_LOOP_TIME * 1000.0);

    setStatus();
  }

  if (strcmp(topic, "status") == 0)
  {
    currentStatus = msg;
    setStatus();
  }
}