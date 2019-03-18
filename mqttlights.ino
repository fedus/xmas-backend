#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "cie1931.h"


// Update these with values suitable for your network.

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";

const String sub_topic = "house/xmas/control/";
const String pub_topic = "house/xmas/status/";

// PIN Settings
const int fairypin = 5;

// Brightness values
int brightness = 100;         // Keep track of current brightness
int brightness_prev = 100;    // Keep track of previous brightness
int brightness_goal = 100;    // We want to bring brightness to the same level as brightness_goal
int brightness_step = 1;      // Amount of increase / decrease of brightness
int brightness_min = 0;       // Min amount used for "swing" mode
int brightness_max = 1024;    // Max amount used for "bumps"

int loop_delay = 10;
int light_mode = 0;
bool mix_mode = false;
int mode_count = 1;
int mode_step = 500;

bool mode_flag = true;      // Flag used for random stuff

bool ota_blink = false;

int mqtt_delay = 15;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void OTAinit() {
  ArduinoOTA.setHostname("Fairylights");
  ArduinoOTA.setPassword("your-password");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    analogWrite(fairypin, 1024);
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int _ota_bright;
    if (ota_blink == true) {
      _ota_bright = 0;
      ota_blink = false;
    }
    else {
      _ota_bright = 1024;
      ota_blink = true;
    }
    analogWrite(fairypin, _ota_bright);
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    light_mode = 3;
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void set_brightness(int new_brightness, bool instant = true, bool set_prev = true) {
  if (set_prev == true) {
    brightness_prev = brightness;
  }
  if (instant == true) {
    brightness_goal = new_brightness;
  }
  brightness = _max(0, _min(new_brightness, 1024));
  analogWrite(fairypin, cie[int(brightness)]);
}

void check_brightness() {
  if (brightness != brightness_goal) {
    int step;
    if (brightness < brightness_goal) {
      step = brightness_step;
    }
    else {
      step = -brightness_step;
    }
    set_brightness(brightness + step, false, false);
  }
}

void bump(int amount) {
  set_brightness(brightness + amount, false, false);
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Use built-in LED to show message processing
  digitalWrite(2, LOW);
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Set brightness instantaneously
  if(strcmp(topic, (sub_topic + "brightness").c_str()) == 0) {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    int pwmVal = atoi((char *)payload);
    Serial.print("[ Setting brightness to ");
    Serial.print(pwmVal);
    Serial.println(" ]");
    set_brightness(pwmVal, true, true);
    mix_mode = false;
    if(light_mode != 0) {
      light_mode = 0;
      client.publish((pub_topic + "mode").c_str(), String(light_mode).c_str());
      delay(mqtt_delay);
    }
    client.publish((pub_topic + "brightness").c_str(), String(brightness).c_str());
    client.publish((pub_topic + "debug/cie").c_str(), String(cie[brightness]).c_str());
  }

  // Set brightness by fading
  if(strcmp(topic, (sub_topic + "fade").c_str()) == 0) {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    brightness_goal = atoi((char *)payload);
    Serial.print("[ Fading brightness to ");
    Serial.print(brightness_goal);
    Serial.println(" ]");
    mix_mode = false;
    if(light_mode != 0) {
      light_mode = 0;
      client.publish((pub_topic + "mode").c_str(), String(light_mode).c_str());
      delay(mqtt_delay);
    }
    client.publish((pub_topic + "fade").c_str(), String(brightness_goal).c_str());
  }

  // Set loop delay
  if(strcmp(topic, (sub_topic + "delay").c_str()) == 0) {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    loop_delay = atoi((char *)payload);
    Serial.print("[ Setting loop delay to ");
    Serial.print(loop_delay);
    Serial.println(" ]");
    client.publish((pub_topic + "delay").c_str(), String(loop_delay).c_str());
  }

  // Set min brightness
  if(strcmp(topic, (sub_topic + "min").c_str()) == 0) {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    brightness_min = atoi((char *)payload);
    Serial.print("[ Setting MIN brightness to ");
    Serial.print(brightness_min);
    Serial.println(" ]");
    client.publish((pub_topic + "min").c_str(), String(brightness_min).c_str());
  }

  // Set max brightness
  if(strcmp(topic, (sub_topic + "max").c_str()) == 0) {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    brightness_max = atoi((char *)payload);
    Serial.print("[ Setting MAX brightness to ");
    Serial.print(brightness_max);
    Serial.println(" ]");
    client.publish((pub_topic + "max").c_str(), String(brightness_max).c_str());
  }

  // Set brightness step
  if(strcmp(topic, (sub_topic + "step").c_str()) == 0) {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    brightness_step = _max(1, atoi((char *)payload));
    Serial.print("[ Setting step to ");
    Serial.print(brightness_step);
    Serial.println(" ]");
    client.publish((pub_topic + "step").c_str(), String(brightness_step).c_str());
  }

  if(strcmp(topic, (sub_topic + "bump").c_str()) == 0) {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    int amount = _max(1, atoi((char *)payload));
    bump(amount);
    Serial.print("[ Bumping by ");
    Serial.print(amount);
    Serial.println(" ]");
    client.publish((pub_topic + "bump").c_str(), String(amount).c_str());
  }

  if(strcmp(topic, (sub_topic + "get_status").c_str()) == 0) {
    sendStatus();
  }

  if(strcmp(topic, (sub_topic + "mode").c_str()) == 0) {
    payload[length] = '\0'; // Make payload a string by NULL terminating it.
    int _mode = atoi((char *)payload);
    Serial.print("[ Setting mode ");
    mix_mode = false;
    mode_flag = true;
    if (_mode == 1) {
      light_mode = 1;
      Serial.print("FADER BI-DIRECTIONAL");
    }
    else if (_mode == 2) {
      light_mode = 2;
      Serial.print("FADER DOWN");
    }
    else if (_mode == 3) {
      light_mode = 3;
      Serial.print("BLINK");
    }
    else if (_mode == 4) {
      light_mode = 4;
      Serial.print("FADER UP");
    }
    else if (_mode == 5) {
      mix_mode = true;
      light_mode = 1;
      mode_count = 1;
      Serial.print("MIX");
    }
    else {
      light_mode = 0;
      Serial.print("STATIC");
      if (brightness == brightness_goal) {
        client.publish((pub_topic + "brightness").c_str(), String(brightness).c_str());
      }
      else client.publish((pub_topic + "fade").c_str(), String(brightness_goal).c_str());
    }
    Serial.println(" ]");
    if (mix_mode) client.publish((pub_topic + "mode").c_str(), String(5).c_str());
    else client.publish((pub_topic + "mode").c_str(), String(light_mode).c_str());
  }

  // Turn built-in LED off
  digitalWrite(2, HIGH);
}

void sendStatus() {
    Serial.print("[ Sending status ]");
    
    if (mix_mode) client.publish((pub_topic + "mode").c_str(), String(5).c_str());
    else client.publish((pub_topic + "mode").c_str(), String(light_mode).c_str());
    delay(mqtt_delay);
    if (light_mode == 0) {
      if (brightness == brightness_goal) {
        client.publish((pub_topic + "brightness").c_str(), String(brightness).c_str());
      }
      else client.publish((pub_topic + "fade").c_str(), String(brightness_goal).c_str());
    }
    delay(mqtt_delay);
    client.publish((pub_topic + "step").c_str(), String(brightness_step).c_str());
    delay(mqtt_delay);
    client.publish((pub_topic + "min").c_str(), String(brightness_min).c_str());
    delay(mqtt_delay);
    client.publish((pub_topic + "max").c_str(), String(brightness_max).c_str());
    delay(mqtt_delay);
    client.publish((pub_topic + "delay").c_str(), String(loop_delay).c_str());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "mqtt-xmas";
    // Attempt to connect
    if (client.connect(clientId.c_str(), "", "")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish((pub_topic + "greeting").c_str(), "hello world");
      // ... and resubscribe
      client.subscribe((sub_topic + "#").c_str());
      set_brightness(brightness_prev, true, false);
      sendStatus();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(2, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  digitalWrite(2, HIGH);
  pinMode(fairypin, OUTPUT);
  Serial.begin(115200);
  set_brightness(50, true, false);
  setup_wifi();
  set_brightness(0, true, false);
  OTAinit();
  // Set brightness to default value after signalling wifi setup
  brightness = 100;
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void fade_bi() {
    if (mode_flag == true && brightness == brightness_goal) {
      brightness_goal = brightness_max;
      mode_flag = false;
    }
    else if (mode_flag == false && brightness == brightness_goal) {
      brightness_goal = brightness_min;
      mode_flag = true;
      mode_count += 4;
    }
    check_brightness();
}

void fade_down() {
    if (mode_flag == true && brightness == brightness_goal) {
      brightness_goal = brightness_min;
      mode_flag = false;
    }
    else if (mode_flag == false && brightness == brightness_goal) {
      set_brightness(brightness_max, true, false);
      mode_flag = true;
      mode_count += 4;
    }
    check_brightness();
}

void fade_up() {
    if (mode_flag == true && brightness == brightness_goal) {
      brightness_goal = brightness_max;
      mode_flag = false;
    }
    else if (mode_flag == false && brightness == brightness_goal) {
      set_brightness(brightness_min, true, false);
      mode_flag = true;
      mode_count += 4;
    }
    check_brightness();
}

void blink_lights() {
    if (mode_flag == true) {
      set_brightness(brightness_max, true, false);
      mode_flag = false;
    }
    else if (mode_flag == false) {
      set_brightness(brightness_min, true, false);
      mode_flag = true;
      mode_count++;
    }
    delay(500);
}

void loop() {
  ArduinoOTA.handle();
  if (!client.connected()) {
    digitalWrite(2, LOW);
    set_brightness(0, true);
    delay(50);
    set_brightness(100, true, false);
    delay(50);
    set_brightness(0, true, false);
    reconnect();
    digitalWrite(2, HIGH);
  }
  client.loop();

  if (light_mode == 1) {
    // FADER BI
    fade_bi();
  }
  else if (light_mode == 2) {
    // FADER DOWN
    fade_down();
  }
  else if (light_mode == 3) {
    // BLINK
    blink_lights();
  }
  else if (light_mode == 4) {
    // FADER UP
    fade_up();
  }
  else {
    check_brightness();
  }
  if (mix_mode == true) {
    if (mode_count % 9 == 0) {
      mode_count = 1;
      light_mode++;
      if (light_mode >= 5) {
        light_mode = 1;
      }
    }
  }
  delay(loop_delay);
}
