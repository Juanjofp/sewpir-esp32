#include <Arduino.h>
#include <sewparser.h>
#include <sew-wifi-config.h>
#include <sew-web-server.hpp>
#include <sew-mqtt.hpp>
#include "sewpir.hpp"

// GPIO
#define PIR_LEFT_PIN 12
#define PIR_RIGHT_PIN 14

// Sensor ID
uint8_t MPL[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
uint8_t MPR[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
uint8_t * macLeftPIR = WiFi.macAddress(MPL);
uint8_t * macRightPIR = WiFi.macAddress(MPR);

// WiFI and MQTT
SewWebServer sewServer;
SewMQTT sewMQTT;

// Publishers
String MAC = WiFi.macAddress();
String publishLeftPIR = MAC + "/TOGGLE/" + MAC + ":00:01/status";
String publishRightPIR = MAC + "/TOGGLE/" + MAC + ":00:02/status";
String publications[] = {publishLeftPIR, publishRightPIR};

void onMQTTConnect() {
    Serial.println("Connected MQTT");
    FRAME frame;
    SewParser::encodeToggle(frame, macLeftPIR, false);
    sewMQTT.publish(0, frame.frame, frame.size);
    SewParser::encodeToggle(frame, macRightPIR, false);
    sewMQTT.publish(1, frame.frame, frame.size);
}

// PIR Sensors
SewPIR leftPIR(PIR_LEFT_PIN);
SewPIR rightPIR(PIR_RIGHT_PIN);

void onPIRRequest(WebServer& ws) {
    String leftPIRStatus = leftPIR.isMotionDetected() ? "true" : "false";
    String rightPIRStatus = rightPIR.isMotionDetected() ? "true" : "false";
    ws.send(200, "application/json", "{\"leftPIR\": " + leftPIRStatus + ", \"rightPIR\": " + rightPIRStatus +"}");
}
void onLeftPIRDetection(boolean isMotionDeteted) {
    FRAME frame;
    SewParser::encodeToggle(frame, macLeftPIR, leftPIR.isMotionDetected());
    sewMQTT.publish(0, frame.frame, frame.size);
}
void onRightPIRDetection(boolean isMotionDeteted) {
    FRAME frame;
    SewParser::encodeToggle(frame, macRightPIR, rightPIR.isMotionDetected());
    sewMQTT.publish(1, frame.frame, frame.size);
}

void setup()
{
    // Debug
    Serial.begin(115200);
    // Try to connect to last WiFi
    initWifi();
    // Init WebServer + mqtt client
    sewServer.addRequest("/pir", onPIRRequest);
    sewServer.initServer(&sewMQTT);
    sewMQTT.setSubscribers(NULL, 0);
    sewMQTT.setPublishers(publications, 2);
    sewMQTT.initMQTT(NULL, onMQTTConnect);
    sewMQTT.reconnect();

    // PIR sensors
    leftPIR.init();
    leftPIR.registerCallback(onLeftPIRDetection);
    rightPIR.init();
    rightPIR.registerCallback(onRightPIRDetection);
}

void loop()
{
    sewServer.handleSewWebServer();
    sewMQTT.handleClient();
    leftPIR.handlePIR();
    rightPIR.handlePIR();
}