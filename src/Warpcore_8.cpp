#include <FastLED.h>
#include <ESP8266WiFi.h>

#include <RemoteMe.h>
#include <RemoteMeSocketConnector.h>
#include <RemoteMeDirectWebSocketConnector.h> // !important library needs WebSockets by Markus Sattler Please install it from Library manager

#include "secrets.h"

char ssid[] = SECRET_SSID;   // your network SSID (name) 
char pass[] = SECRET_PASS;   // your network password

#define DEVICE_ID 1
#define DEVICE_NAME "WarpCore"

// WiFiServer server(80);

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806, define both DATA_PIN and CLOCK_PIN
#define DATA_PIN D3
//#define CLOCK_PIN D2
#define SerialSpeed 115200

// How many LEDs in your strip?
#define NUM_LEDS 26 // Total number of LEDs

// How are the LEDs distributed?
#define SegmentSize 4	   // How many LEDs in each "Magnetic Constrictor" segment
#define TopLEDcount 12	   // LEDs above the "Reaction Chamber"
#define ReactionLEDcount 2 // LEDs inside the "Reaction Chamber"
#define BottomLEDcount 12  // LEDs below the "Reaction Chamber"

// Default Settings
#define DefaultWarpFactor 1	  // 1-9
#define DefaultMainHue 160	  // 1-255	1=Red 32=Orange 64=Yellow 96=Green 128=Aqua 160=Blue 192=Purple 224=Pink 255=Red
#define DefaultSaturation 255 // 1-255
#define DefaultBrightness 128 // 1-255
#define DefaultPattern 1	  // 1-5		1=Standard 2=Breach 3=Rainbow 4=Fade 5=Slow Fade

// Initialize internal variables
#define PulseLength SegmentSize * 2
#define TopChases TopLEDcount / PulseLength + 1 * PulseLength
#define TopLEDtotal TopLEDcount + ReactionLEDcount
#define TopDiff TopLEDcount - BottomLEDcount
#define RateMultiplier 3
byte MainHue = DefaultMainHue;
byte ReactorHue = DefaultMainHue;
byte LastHue = DefaultMainHue;
byte WarpFactor = DefaultWarpFactor;
byte LastWarpFactor = DefaultWarpFactor;
byte Rate = RateMultiplier * WarpFactor;
byte Saturation = DefaultSaturation;
byte Brightness = DefaultBrightness;
byte Pattern = DefaultPattern;
byte Pulse;
boolean Rainbow = false;
boolean Fade = false;
boolean SlowFade = false; // Default Settings
boolean AutoBright = false;
boolean Disabled = false;

// Serial input variables
const byte numChars = 20;
char receivedChars[numChars];
char tempChars[numChars]; // temporary array for use when parsing

// Parsing variables
byte warp_factor = WarpFactor;
byte hue = MainHue;
byte saturation = Saturation;
byte brightness = Brightness;
byte pattern = Pattern;
boolean disabled = Disabled;

bool newData = false;

String warpFactorArg = "warpfactor";
String hueArg = "hue";
String saturationArg = "saturation";
String brightnessArg = "brightness";
String patternArg = "pattern";

// Define the array of LEDarray
CRGB LEDarray[NUM_LEDS];

RemoteMe& remoteMe = RemoteMe::getInstance(SECRET_TOKEN, DEVICE_ID);

//*************** CODE FOR COMFORTABLE VARIABLE SET *********************

inline void setWarp_brightness(int32_t i) {remoteMe.getVariables()->setInteger("warp_brightness", i); }
inline void setWarp_factor(int32_t i) {remoteMe.getVariables()->setInteger("warp_factor", i); }
inline void setWarp_hue(int32_t i) {remoteMe.getVariables()->setInteger("warp_hue", i); }
inline void setWarp_pattern(int32_t i) {remoteMe.getVariables()->setInteger("warp_pattern", i); }
inline void setWarp_saturation(int32_t i) {remoteMe.getVariables()->setInteger("warp_saturation", i); }
inline void setWarp_disabled(boolean b) {remoteMe.getVariables()->setBoolean("warp_disabled", b); }

//*************** IMPLEMENT FUNCTIONS BELOW *********************

void mapBrightness()
{
	if (AutoBright)
	{
		Brightness = brightness; //map(readAnalog(A0), 0, 1024, 255, 6);
		Serial.println("Brightness auto-updated to: ");
		Serial.println(Brightness);
	}
	else
	{
		Brightness = brightness;
		Serial.println("Brightness updated to: ");
		Serial.println(Brightness);
	}
}

void updateSettings()
{
	Disabled = disabled;
	if (disabled) {
		return;
	}
	if (pattern > 0 && pattern < 6 && pattern != Pattern)
	{
		// if (warp_factor > 0)
		// {
			warp_factor = DefaultWarpFactor;
			Rate = RateMultiplier * WarpFactor;
		// }
		// if (hue > 0)
		// {
			hue = DefaultMainHue;
		// }
		// if (saturation > 0)
		// {
			saturation = DefaultSaturation;
		// }
		// if (brightness > 0)
		// {
			brightness = DefaultBrightness;
		// }
		Pattern = pattern;
		Serial.print("Color Pattern Set To ");
		Serial.println(Pattern);
		updateSettings();
	}
	else
	{
		if (warp_factor > 0 && warp_factor < 10)
		{
			WarpFactor = warp_factor;
			LastWarpFactor = warp_factor;
			Rate = RateMultiplier * WarpFactor;
			Serial.print(F("Warp Factor Set To "));
			Serial.println(warp_factor);
		}
		if (hue > 0 && hue < 256)
		{
			MainHue = hue;
			ReactorHue = hue;
			LastHue = hue;
			Serial.print(F("Color Hue Set To "));
			Serial.println(hue);
		}
		if (saturation > 0 && saturation < 256)
		{
			Saturation = saturation;
			Serial.print(F("Color Saturation Set To "));
			Serial.println(saturation);
		}
		if (brightness > 0 && brightness < 256)
		{
			if (brightness == 1)
			{
				AutoBright = true;
			}
			mapBrightness();
			FastLED.setBrightness(brightness);
			Brightness = brightness;
			Serial.print(F("Brightness Set To "));
			Serial.println(brightness);
		}
	}
	newData = false;
}

void onWarp_disabledChange(boolean b) {
	Serial.printf("onWarp_disabledChange: b: %d\n",b);
	disabled = b;
	updateSettings();
}

void onWarp_brightnessChange(int32_t i) {
	Serial.printf("onWarp_brightnessChange: i: %d\n",i);
	brightness = i;
	updateSettings();
}
void onWarp_factorChange(int32_t i) {
	Serial.printf("onWarp_factorChange: i: %d\n",i);
	warp_factor = i;
}
void onWarp_hueChange(int32_t i) {
	Serial.printf("onWarp_hueChange: i: %d\n",i);
	hue = i;
	updateSettings();
}
void onWarp_patternChange(int32_t i) {
	Serial.printf("onWarp_patternChange: i: %d\n",i);
	// Bedtime
	if (i == 6) {
		setWarp_disabled(false);
		setWarp_factor(1);
		setWarp_pattern(1);
		setWarp_brightness(6);
	}
	else if (i == 7) {
		setWarp_disabled(false);
		setWarp_brightness(DefaultBrightness);
		setWarp_factor(DefaultWarpFactor);
		setWarp_saturation(DefaultSaturation);
		setWarp_hue(DefaultMainHue);
		setWarp_pattern(DefaultPattern);
	} 
	else
	{
		pattern = i;
	}
	updateSettings();
}
void onWarp_saturationChange(int32_t i) {
	Serial.printf("onWarp_saturationChange: i: %d\n",i);
	saturation = i;
	updateSettings();
}

void incrementMainHue()
{
	if (MainHue == 255)
	{
		MainHue = 1;
	}
	else
	{
		MainHue++;
	}
}

void incrementHue()
{
	if (hue == 255)
	{
		hue = 1;
	}
	else
	{
		hue++;
	}
}

void incrementReactorHue()
{
	if (ReactorHue == 255)
	{
		ReactorHue = 1;
	}
	else
	{
		ReactorHue++;
	}
}

void chase()
{
	if (Pulse == PulseLength - 1)
	{
		Pulse = 0;
		if (SlowFade == true)
		{
			incrementHue();
		}
	}
	else
	{
		Pulse++;
	}
	if (Fade == true)
	{
		incrementHue();
	}
	// Ramp LED brightness
	for (int value = 32; value < 255; value = value + Rate)
	{
		if (Rainbow == true)
		{
			incrementHue();
		}
		// Set every Nth LED
		for (int chases = 0; chases < TopChases; chases = chases + PulseLength)
		{
			byte Top = Pulse + chases;
			byte Bottom = NUM_LEDS + TopDiff - (Pulse + chases) - 1;
			if (Top < TopLEDtotal)
			{
				LEDarray[Top] = CHSV(MainHue, Saturation, value);
			}
			if (Bottom > TopLEDcount && Bottom < NUM_LEDS)
			{
				LEDarray[Bottom] = CHSV(MainHue, Saturation, value);
			}
		}
		// Keep reaction chamber at full brightness even though we chase the leds right through it
		for (int reaction = 0; reaction < ReactionLEDcount; reaction++)
		{
			LEDarray[TopLEDcount + reaction] = CHSV(ReactorHue, Saturation, 255);
		}
		fadeToBlackBy(LEDarray, NUM_LEDS, (Rate * 0.5)); // Dim all LEDs by Rate/2
		FastLED.show();									 // Show set LEDs
	}
}

void standard()
{
	ReactorHue = MainHue;
	chase();
}

void breach()
{
	byte breach_diff = 255 - LastHue;
	byte transition_hue = LastHue + (breach_diff / 2);
	if (ReactorHue < 255)
	{
		incrementReactorHue();
	}
	if (ReactorHue > transition_hue && MainHue < 255)
	{
		incrementMainHue();
	}
	if (ReactorHue >= 255 && MainHue >= 255)
	{
		MainHue = LastHue;
		ReactorHue = MainHue + 1;
	}
	Rate = (((ReactorHue - MainHue) / (breach_diff / 9) + 1) * RateMultiplier);
	WarpFactor = Rate / RateMultiplier;
	chase();
}

void rainbow()
{
	Rainbow = true;
	chase();
	Rainbow = false;
}

void fade()
{
	Fade = true;
	chase();
	Fade = false;
}

void slowFade()
{
	SlowFade = true;
	chase();
	SlowFade = false;
}

/*
void parseDataClient(char *msg)
{
	Serial.print(F("Parsing String: "));
	Serial.println(msg);
	char *ptr = strtok(msg, "&");
	while (ptr)
	{
		int startIndex = String(ptr).indexOf("=");
		if (startIndex != -1)
		{
			String inputArg = String(ptr).substring(0, startIndex);
			String tmpVal = String(ptr).substring(startIndex + 1, String(ptr).length());
			// Length (with one extra character for the null terminator)
			int tmpLen = tmpVal.length() + 1;
			char inputVal[tmpLen];
			tmpVal.toCharArray(inputVal, tmpLen);

			if (inputArg == patternArg)
			{
				Serial.print("Set pattern to ");
				Serial.println(atol(inputVal));
				pattern = atol(inputVal);
			}
			else if (inputArg == hueArg)
			{
				Serial.print("Set hue to ");
				Serial.println(atol(inputVal));
				hue = atol(inputVal);
			}
			else if (inputArg == warpFactorArg)
			{
				Serial.print("Set warp factor to ");
				Serial.println(atol(inputVal));
				warp_factor = atol(inputVal);
			}
			else if (inputArg == brightnessArg)
			{
				Serial.print("Set brightness to ");
				Serial.println(atol(inputVal));
				brightness = atol(inputVal);
			}
			else if (inputArg == saturationArg)
			{
				Serial.print("Set saturation to ");
				Serial.println(atol(inputVal));
				saturation = atol(inputVal);
			}
		}
		ptr = strtok(NULL, "&");
	}
}

void PrintInfoClient(WiFiClient client)
{
	client.println(F("** Current Settings **"));
	client.println("</br>");
	client.print(F("Warp factor: "));
	client.print(WarpFactor);
	client.println("</br>");
	client.print(F("Hue: "));
	client.print(MainHue);
	client.println("</br>");
	client.print(F("Saturation: "));
	client.print(Saturation);
	client.println("</br>");
	client.print(F("Brightness: "));
	client.print(Brightness);
	client.println("</br>");
	client.print(F("Pattern: "));
	client.print(Pattern);
	client.println("</br>");
	client.println(F("**********************"));
	client.println("<table align=\"left\">");
	client.println(F("<form action=\"/get\">"));
	client.println(F("<br/><tr><th align=\"left\">  Pattern:     </th><th align=\"left\"> <input type=\"radio\" id=\"standard\" name=\"pattern\" value=\"1\">"));
	client.println(F("<label for=\"standard\">Standard (1)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"breach\" name=\"pattern\" value=\"2\">"));
	client.println(F("<label for=\"breach\">Breach (2)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"rainbow\" name=\"pattern\" value=\"3\">"));
	client.println(F("<label for=\"rainbow\">Rainbow (3)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"fade\" name=\"pattern\" value=\"4\">"));
	client.println(F("<label for=\"fade\">Fade (4)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"slowfade\" name=\"pattern\" value=\"5\">"));
	client.println(F("<label for=\"slowfade\">Slow Fade (5)</label></th></tr>"));
	client.println(F("<br/><tr><th align=\"left\">  Warp factor (1-9):   </th><th align=\"left\"> <input type=\"number\" name=\"warpfactor\" min=\"1\" max=\"9\"></th><tr>"));
	client.println(F("<br/><tr><th align=\"left\">  Hue:     </th><th align=\"left\"> <input type=\"radio\" id=\"red\" name=\"hue\" value=\"1\">"));
	client.println(F("<label for=\"red\">Red (1)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"orange\" name=\"hue\" value=\"32\">"));
	client.println(F("<label for=\"orange\">Orange (32)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"yellow\" name=\"hue\" value=\"64\">"));
	client.println(F("<label for=\"yellow\">Yellow (64)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"green\" name=\"hue\" value=\"96\">"));
	client.println(F("<label for=\"green\">Green (96)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"aqua\" name=\"hue\" value=\"128\">"));
	client.println(F("<label for=\"aqua\">Aqua (128)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"blue\" name=\"hue\" value=\"160\">"));
	client.println(F("<label for=\"blue\">Blue (160)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"purple\" name=\"hue\" value=\"192\">"));
	client.println(F("<label for=\"purple\">Purple (192)</label></th></tr>"));
	client.println(F("<tr><th></th><th align=\"left\"><input type=\"radio\" id=\"pink\" name=\"hue\" value=\"224\">"));
	client.println(F("<label for=\"pink\">Pink (224)</label></th></tr>"));
	client.println(F("<br/><tr><th align=\"left\">  Saturation (1-255):  </th><th align=\"left\"> <input type=\"number\" name=\"saturation\" min=\"1\" max=\"255\"></th><tr>"));
	client.println(F("<br/><tr><th align=\"left\">  Brightness (1-255):  </th><th align=\"left\"> <input type=\"number\" name=\"brightness\" min=\"1\" max=\"255\"></th><tr>"));
	client.println(F("<br/><tr><th>            </th><th align=\"right\">  <input type=\"submit\" value=\"Submit\"></th><tr>"));
	client.println(F("</form><br>"));
	client.println("</table>");
}

void PrintInfo()
{
	Serial.println(F("******** Help ********"));
	Serial.println(F("Input Format - _2, 160, 220, 255-"));
	Serial.println(F("Input Fields - <Warp Factor, Hue, Saturation, Brightness, Pattern>"));
	Serial.println(F("Warp Factor range - 1-9"));
	Serial.println(F("Hue range - 1-255 1=Red 32=Orange 64=Yellow 96=Green 128=Aqua 160=Blue 192=Purple 224=Pink 255=Red"));
	Serial.println(F("Saturation range - 1-255"));
	Serial.println(F("Brightness range - 1-255"));
	Serial.println(F("Pattern - 1-5 1=Standard 2=Breach 3=Rainbow 4=Fade 5=Slow Fade"));
	Serial.println(F(""));
	Serial.println(F("** Current Settings **"));
	Serial.print(F(" _"));
	Serial.print(WarpFactor);
	Serial.print(F(", "));
	Serial.print(MainHue);
	Serial.print(F(", "));
	Serial.print(Saturation);
	Serial.print(F(", "));
	Serial.print(Brightness);
	Serial.print(F(", "));
	Serial.print(Pattern);
	Serial.println(F("-"));
	Serial.println(F("**********************"));
}

/*
void parseData()
{
	Serial.println(tempChars);
	char *strtokIndx;					 // this is used by strtok() as an index
	strtokIndx = strtok(tempChars, "."); // get the first part of the string
	warp_factor = atoi(strtokIndx);		 // convert this part to an integer
	strtokIndx = strtok(NULL, ".");		 // this continues where the previous call left off
	hue = atoi(strtokIndx);
	strtokIndx = strtok(NULL, ".");
	saturation = atoi(strtokIndx);
	strtokIndx = strtok(NULL, ".");
	brightness = atoi(strtokIndx);
	strtokIndx = strtok(NULL, ".");
	pattern = atoi(strtokIndx);
}

void receiveSerialData()
{
	static bool recvInProgress = false;
	static byte ndx = 0;
	char startMarker = '_';
	char endMarker = '-';
	char helpMarker = '?';

	char rc;

	while (Serial.available() > 0 && newData == false)
	{
		rc = Serial.read();

		if (rc == helpMarker)
		{
			PrintInfo();
		}
		else if (recvInProgress == true)
		{
			if (rc != endMarker)
			{
				receivedChars[ndx] = rc;
				ndx++;
				if (ndx >= numChars)
				{
					ndx = numChars - 1;
				}
			}
			else if (rc == endMarker)
			{
				receivedChars[ndx] = '\0';
				recvInProgress = false;
				ndx = 0;
				newData = true;
			}
		}
		else if (rc == startMarker)
		{
			recvInProgress = true;
		}
	}
}
*/

void setup()
{
	delay(2000); // 2 second delay for recovery
	Serial.begin(SerialSpeed);
	FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDarray, NUM_LEDS); // GRB ordering is typical
																 //	FastLED.addLeds<WS2812B,DATA_PIN, CLOCK_PIN, RGB>(LEDarray,NUM_LEDS);
																 //	FastLED.setCorrection(Typical8mmPixel);	// (255, 224, 140)
																 //	FastLED.setCorrection(TypicalSMD5050);	// (255, 176, 240)
	FastLED.setCorrection(CRGB(255, 200, 245));
	FastLED.setMaxPowerInVoltsAndMilliamps(5, 750);
	FastLED.setBrightness(Brightness);
	// PrintInfo();

	// Connect to WiFi network
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(SECRET_SSID);

	IPAddress ip(192, 168, 2, 220);
	IPAddress gateway(192, 168, 2, 1);
	IPAddress subnet(255, 255, 255, 0);
	WiFi.config(ip, gateway, subnet);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	//  WiFi.hostname("warpcore");
	wifi_station_set_hostname("warpcore");
	WiFi.begin(SECRET_SSID, SECRET_PASS);

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");

	remoteMe.getVariables()->observeInteger("warp_brightness" ,onWarp_brightnessChange);
	remoteMe.getVariables()->observeInteger("warp_factor" ,onWarp_factorChange);
	remoteMe.getVariables()->observeInteger("warp_hue" ,onWarp_hueChange);
	remoteMe.getVariables()->observeInteger("warp_pattern" ,onWarp_patternChange);
	remoteMe.getVariables()->observeInteger("warp_saturation" ,onWarp_saturationChange);
	remoteMe.getVariables()->observeBoolean("warp_disabled" ,onWarp_disabledChange);

	remoteMe.setConnector(new RemoteMeSocketConnector());
	remoteMe.setDirectConnector(new RemoteMeDirectWebSocketConnector());
	remoteMe.sendRegisterDeviceMessage(DEVICE_NAME);

	// Start the server
	// server.begin();
	Serial.println("Server started");

	// Print the IP address
	Serial.print("Use this URL : ");
	Serial.print("http://");
	Serial.print(WiFi.localIP());
	Serial.println("/");
}

void loop()
{
	remoteMe.loop();
	// receiveSerialData();
	// if (newData == true)
	// {
	// strcpy(tempChars, receivedChars); // this is necessary because strtok() in parseData() replaces the commas with \0
	// parseData();
	// updateSettings();
	// newData = false;
	// }

	if (!disabled) 
	{
		// Serial.println("--> Running pattern <--");
		if (Pattern == 1)
		{
			// Serial.println("-- Standard --");
			standard();
		}
		else if (Pattern == 2)
		{
			// Serial.println("-- Breach --");
			breach();
		}
		else if (Pattern == 3)
		{
			// Serial.println("-- Rainbow --");
			rainbow();
		}
		else if (Pattern == 4)
		{
			// Serial.println("-- Fade --");
			fade();
		}
		else if (Pattern == 5)
		{
			// Serial.println("-- Slowfade --");
			slowFade();
		}
		else
		{
			// Serial.println("-- Standard --");
			standard();
		}
	}
	else 
	{
		FastLED.clear();  // clear all pixel data
  		FastLED.show();
	}

	// Check if a client has connected
	// WiFiClient client = server.available();
	// if (!client)
	// {
	// 	return;
	// }

	// Wait until the client sends some data
	// Serial.println("new client");
	//  while(!client.available()){
	//    delay(1);
	//  }

	// Read the first line of the request
	// String request = client.readStringUntil('\r');
	// Serial.println(request);
	// client.flush();
	// String inputMessage;
	// String inputParam;
	// // GET input1 value on <ESP_IP> ?input1=<inputMessage>
	// String startToken = "/get?";
	// int startIndex = request.indexOf(startToken);
	// if (startIndex != -1)
	// {
	// 	inputMessage = request.substring(startIndex + startToken.length(), request.lastIndexOf(" ") + 1);
	// 	Serial.println("InputMessage: " + inputMessage);
	// 	//    inputParam = PARAM_INPUT_1;
	// 	char charBuf[inputMessage.length()];
	// 	inputMessage.toCharArray(charBuf, inputMessage.length());
	// 	Serial.println("Parsing data");
	// 	// parseDataClient(charBuf);
	// 	Serial.println("Updating settings");
	// 	updateSettings();
	// }

	// // Return the response
	// client.println("HTTP/1.1 200 OK");
	// client.println("Content-Type: text/html");
	// client.println(""); //  do not forget this one
	// client.println("<!DOCTYPE HTML>");
	// client.println("<html>");

	// // PrintInfoClient(client);

	// client.println("</html>");

	// delay(1000);
	// Serial.println("Client disconnected");
	// Serial.println("");
}