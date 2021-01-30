#include <Arduino.h>
#include <sewparser.h>
#include <sew-wifi-config.h>
#include <sew-web-server.hpp>
#include <sew-mqtt.hpp>
#include "sewpir.hpp"
#include <DHTesp.h>

// GPIO
#define PIR_LEFT_PIN 35
#define PIR_RIGHT_PIN 34
#define PIR_FRONT_PIN 32
#define PIR_BACK_PIN 33
#define DHT11_PIN 26

// Sensor ID
uint8_t MPL[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
uint8_t MPR[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
uint8_t MPF[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03};
uint8_t MPB[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
uint8_t DHTT[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05};
uint8_t DHTH[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06};

uint8_t * macLeftPIR = WiFi.macAddress(MPL);
uint8_t * macRightPIR = WiFi.macAddress(MPR);
uint8_t * macFrontPIR = WiFi.macAddress(MPF);
uint8_t * macBackPIR = WiFi.macAddress(MPB);
uint8_t * macTemperatureDHT = WiFi.macAddress(DHTT);
uint8_t * macHumidityDHT = WiFi.macAddress(DHTH);

// WiFI and MQTT
SewWebServer sewServer;
SewMQTT sewMQTT;

// Publishers
String MAC = WiFi.macAddress();
String publishPIR = MAC + "/status";
String publications[] = {publishPIR};

void onMQTTConnect() {
    Serial.println("Connected MQTT");
    FRAME frame;
    SewParser::encodeToggle(frame, macLeftPIR, false);
    sewMQTT.publish(0, frame.frame, frame.size);
    SewParser::encodeToggle(frame, macRightPIR, false);
    sewMQTT.publish(0, frame.frame, frame.size);
    SewParser::encodeToggle(frame, macFrontPIR, false);
    sewMQTT.publish(0, frame.frame, frame.size);
    SewParser::encodeToggle(frame, macBackPIR, false);
    sewMQTT.publish(0, frame.frame, frame.size);
}

// PIR Sensors
SewPIR leftPIR(PIR_LEFT_PIN);
SewPIR rightPIR(PIR_RIGHT_PIN);
SewPIR frontPIR(PIR_FRONT_PIN);
SewPIR backPIR(PIR_BACK_PIN);

// DHT Sensor
DHTesp sensorDHT;

void onPIRRequest(WebServer& ws) {
    String response = "{\"deviceId\": \"" + MAC + "\", \"sensors\": [";
    response += "{\"type\": \"TOGGLE\", \"sensorId\": \"" + MAC + ":00:01\"},";
    response += "{\"type\": \"TOGGLE\", \"sensorId\": \"" + MAC + ":00:02\"},";
    response += "{\"type\": \"TOGGLE\", \"sensorId\": \"" + MAC + ":00:03\"},";
    response += "{\"type\": \"TOGGLE\", \"sensorId\": \"" + MAC + ":00:04\"},";
    response += "{\"type\": \"TEMPERATURE\", \"sensorId\": \"" + MAC + ":00:05\"},";
    response += "{\"type\": \"HUMIDITY\", \"sensorId\": \"" + MAC + ":00:06\"}";

    response += "]}";
    ws.send(200, "application/json", response);
}
void onLeftPIRDetection(boolean isMotionDeteted) {
    FRAME frame;
    SewParser::encodeToggle(frame, macLeftPIR, leftPIR.isMotionDetected());
    sewMQTT.publish(0, frame.frame, frame.size);
}
void onRightPIRDetection(boolean isMotionDeteted) {
    FRAME frame;
    SewParser::encodeToggle(frame, macRightPIR, rightPIR.isMotionDetected());
    sewMQTT.publish(0, frame.frame, frame.size);
}
void onFrontPIRDetection(boolean isMotionDeteted) {
    FRAME frame;
    SewParser::encodeToggle(frame, macFrontPIR, frontPIR.isMotionDetected());
    sewMQTT.publish(0, frame.frame, frame.size);
}
void onBackPIRDetection(boolean isMotionDeteted) {
    FRAME frame;
    SewParser::encodeToggle(frame, macBackPIR, backPIR.isMotionDetected());
    sewMQTT.publish(0, frame.frame, frame.size);
}

void setup()
{
    // Debug
    Serial.begin(115200);
    // Try to connect to last WiFi
    initWifi();
    // Init WebServer + mqtt client
    sewServer.addRequest("/sensors", onPIRRequest);
    sewServer.initServer(&sewMQTT);
    sewMQTT.setSubscribers(NULL, 0);
    sewMQTT.setPublishers(publications, 1);
    sewMQTT.initMQTT(NULL, onMQTTConnect);
    sewMQTT.reconnect();

    // PIR sensors
    leftPIR.init();
    leftPIR.registerCallback(onLeftPIRDetection);
    rightPIR.init();
    rightPIR.registerCallback(onRightPIRDetection);
    frontPIR.init();
    frontPIR.registerCallback(onFrontPIRDetection);
    backPIR.init();
    backPIR.registerCallback(onBackPIRDetection);

    // DHTesp
    sensorDHT.setup(DHT11_PIN, DHTesp::DHT11);
}

unsigned long lastDHTRead = 0;
void handleDHT()
{
    unsigned long now = millis();
    if(now - lastDHTRead >= 4000) {
        lastDHTRead = now;
        Serial.println("Time passed: " + String(now) + ", " + String(lastDHTRead));
        delay(sensorDHT.getMinimumSamplingPeriod());
        float humidity = sensorDHT.getHumidity();
        float temperature = sensorDHT.getTemperature();

        FRAME frame;
        SewParser::encodeTemperature(frame, macTemperatureDHT, temperature);
        sewMQTT.publish(0, frame.frame, frame.size);
        SewParser::encodeTemperature(frame, macHumidityDHT, humidity);
        sewMQTT.publish(0, frame.frame, frame.size);
        Serial.println("DHT: " + String(temperature, 10) + "ºC, " + String(humidity, 10) + "%");
    }
}

void loop()
{
    sewServer.handleSewWebServer();
    sewMQTT.handleClient();
    leftPIR.handlePIR();
    rightPIR.handlePIR();
    frontPIR.handlePIR();
    backPIR.handlePIR();
    handleDHT();
}