#include <SPI.h>
#include <Ethernet2.h>
#include <Adafruit_NeoPixel.h>
#include <string.h>


#define PIN 6


// ===========================================================================
// const variables
// ===========================================================================
const int MAX_PARAMS = 4;
const int MAX_LENGTH = 16;
// also counted "=" and "&" symbols
const int MAX_POST_LENGTH = (MAX_LENGTH * MAX_PARAMS) + 2 * MAX_PARAMS - 1;

const int SEGMENTS = 50;
const double MAX_HUE = 360.0;
const double SATURATION = 95.0;
const double LIGHTNESS = 50.0;
const double HUE_STEP = MAX_HUE / SEGMENTS;

const int MAX_DELAY = 500;

// hue shifts after every loop with this value
// 80, 150, 220, ..., 60, 130, 200, ...
const int HUE_INCREMENT = 70;

byte mac[] = { 0xD1, 0xA2, 0xBE, 0xE4, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 200);


// ===========================================================================
// global variables
// ===========================================================================
char names[MAX_PARAMS][MAX_LENGTH];
char values[MAX_PARAMS][MAX_LENGTH];
double globalGradientHue = 0.0;
int globalDelay = 0;
boolean globalBlinkState = false;


// ===========================================================================
// init global variables
// ===========================================================================
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(SEGMENTS, PIN, NEO_BRG + NEO_KHZ800);
EthernetServer server(80);


// ===========================================================================
// convert HSL to RGB color
// ===========================================================================
void hsl2rgb(double h, double s, double l, int *r, int *g, int *b) {
	double L = l / 100.0;
	double S = s / 100.0;

	double C = (1 - abs(2 * L - 1)) * S;
	double X = C * (1 - abs(fmod(h / 60.0, 2) - 1));
	double r1 = 0, g1 = 0, b1 = 0;

	if (h >= 0 && h < 60) {
		r1 = C; g1 = X;
	} else if (h >= 60 && h < 120) {
		r1 = X; g1 = C;
	} else if (h >= 120 && h < 180) {
		g1 = C; b1 = X;
	} else if (h >= 180 && h < 240) {
		g1 = X; b1 = C;
	} else if (h >= 240 && h < 300) {
		r1 = X; b1 = C;
	} else if (h >= 300 && h < 360) {
		r1 = C; b1 = X;
	}

	double M = L - C / 2.0;
	*r = (int)round((r1 + M) * 255);
	*g = (int)round((g1 + M) * 255);
	*b = (int)round((b1 + M) * 255);
}


// ===========================================================================
// getting string value from global dict
// "NULL" if not found
// ===========================================================================
char* getStringValue(char *name) {
	for (int i = 0; i < MAX_PARAMS; i++) {
		if (strcmp(names[i], name) == 0) return values[i];
	}
	return NULL;
}


// ===========================================================================
// getting int (> 0) value from global dict
// "-1" if not found
// ===========================================================================
int getIntValue(char *name) {
	char *value = getStringValue(name);
	return (value != NULL) ? atoi(value) : -1;
}


// ===========================================================================
// getting boolean value from global dict
// "false" if not found
// ===========================================================================
boolean getBooleanValue(char *name) {
	char *value = getStringValue(name);
	return (value != NULL) ? strcmp(value, "true") : false;
}


// ===========================================================================
// analyzing incoming POST paramaters and sending them in global dict
// ===========================================================================
void analyzePostParams(const char *postString) {
	char buffer[MAX_POST_LENGTH];
	char output[64];
	strcpy(buffer, postString);

	char *find;
	char *params = strtok(buffer, "&");
	int i = 0;
	while (params != NULL && i < MAX_PARAMS) {
		find = strchr(params, '=');
		*find++ = 0;

		sprintf(output, "POST param %d: '%s' = '%s'", i, params, find);
		Serial.println(output);
		strcpy(names[i], params);
		strcpy(values[i], find);
		i++;

		params = strtok(NULL, "&");
	}

	while (i < MAX_PARAMS) {
		names[i][0] = values[i][0] = '\0';
		i++;
	}
}


// ===========================================================================
// check hue value for diapasone (0, 360]
// ===========================================================================
double checkHue(double &hue) {
	if (hue >= MAX_HUE || hue < 0.0) {
		hue = fmod(hue, MAX_HUE);
	}
}


// ===========================================================================
// set gradient color to all segments
// start hue from "globalGradientHue" variable
// ===========================================================================
void updateGradient() {
	int r, g, b;
	for (int i = 0; i < SEGMENTS; i++) {
		checkHue(globalGradientHue);
		hsl2rgb(globalGradientHue, SATURATION, LIGHTNESS, &r, &g, &b);
		pixels.setPixelColor(i, pixels.Color(r, g, b));
		globalGradientHue += HUE_STEP;
	}
	pixels.show();
}


// ===========================================================================
// set solid color to all segments
// ===========================================================================
void updateColor(int r, int g, int b) {
	for (int i = 0; i < SEGMENTS; i++) {
		pixels.setPixelColor(i, pixels.Color(r, g, b));
	}
	pixels.show();
}


// ===========================================================================
// set color to segment
// ===========================================================================
void updateSegmentColor(int i, int r, int g, int b) {
	pixels.setPixelColor(i, pixels.Color(r, g, b));
	pixels.show();
}


// ===========================================================================
// main setup function
// ===========================================================================
void setup() {
	Serial.begin(9600);

	Ethernet.begin(mac, ip);
	Serial.print("Start on IP: ");
	Serial.println(Ethernet.localIP());

	Serial.println("Init diodes to black color");
	pixels.begin();
	updateColor(0, 0, 0);
}


// ===========================================================================
// main loop function
// ===========================================================================
void loop() {
	EthernetClient client = server.available();

	if (client) {
		boolean crlf = false, post = false;
		char buffer[MAX_POST_LENGTH];
		char *p = buffer;

		while (client.connected()) {
			int size = client.available();

			if (size) {
				int c = client.read();

				if (!post) {
					if (c == '\n') {
						if (crlf) post = true;
						crlf = true;
					} else if (c != '\r') {
						crlf = false;
					}
				} else {
					*(p++) = c;
				}
			} else {
				*p = '\0';
				analyzePostParams(buffer);

				client.println("HTTP/1.1 200 OK");
				client.println("Content-Type: text/plain");
				client.println("Connection: close");
				client.println();
				client.println("Color of diodes is set");
				break;
			}
		}

		globalDelay = 0;

		client.stop();
		Serial.println("Client disconnected");
	} else {
		char *mode = getStringValue("mode");
		int delayParam = getIntValue("delay");
		int r, g, b;

		if (mode != NULL && globalDelay == 0) {
			if (strcmp(mode, "snake") == 0) {
				// params example:
				//     mode=snake
				//     delay=2000 (default: 1000) - between change color
				checkHue(globalGradientHue);
				hsl2rgb(globalGradientHue, SATURATION, LIGHTNESS, &r, &g, &b);
				for (int i = 0; i < SEGMENTS; i++) {
					updateSegmentColor(i, r, g, b);
					delay(20);
				}
				globalGradientHue += HUE_INCREMENT;
				globalDelay = delayParam >= 0 ? delayParam : 1000;
			} else if (strcmp(mode, "color") == 0) {
				// params example:
				//     mode=color
				//     red=150
				//     green=100
				//     blue=145
				r = getIntValue("red");
				g = getIntValue("green");
				b = getIntValue("blue");
				updateColor(r, g, b);
			} else if (strcmp(mode, "gradient") == 0) {
				// params example:
				//     mode=gradient
				//     start_hue=50
				//     animation=true (default: false) - permanent animation
				int startHueParam = getIntValue("start_hue");
				boolean doAnimation = getBooleanValue("animation");

				if (startHueParam >= 0) globalGradientHue = (double)startHueParam;
				updateGradient();
				if (doAnimation) globalGradientHue -= HUE_STEP;
			} else if (strcmp(mode, "random_solid") == 0) {
				// params example:
				//     mode=random_color
				//     blink=true (default: false)
				//     delay=1000 (default: 2000)
				boolean doBlink = getBooleanValue("blink");

				if (globalBlinkState && doBlink) {
					updateColor(0, 0, 0);
				} else {
					checkHue(globalGradientHue);
					hsl2rgb(globalGradientHue, SATURATION, LIGHTNESS, &r, &g, &b);
					updateColor(r, g, b);
					globalGradientHue += HUE_INCREMENT;
				}

				globalDelay = delayParam >= 0 ? delayParam : 2000;
				globalBlinkState = !globalBlinkState;
			} else if (strcmp(mode, "zebra") == 0) {
				// params example:
				//     mode=zebra
				//     delay=2000 (default: 1000)
				int odd = random(0, MAX_HUE), even = random(0, MAX_HUE);
				for (int i = 0; i < SEGMENTS; i++) {
					hsl2rgb(i % 2 == 0 ? even : odd, SATURATION, LIGHTNESS, &r, &g, &b);
					pixels.setPixelColor(i, pixels.Color(r, g, b));
				}
				pixels.show();
				globalDelay = delayParam >= 0 ? delayParam : 1000;
			}
		}

		if (globalDelay) {
			if (globalDelay > MAX_DELAY) {
				delay(MAX_DELAY);
				globalDelay -= MAX_DELAY;
			} else {
				delay(globalDelay);
				globalDelay = 0;
			}
		}
	}
}