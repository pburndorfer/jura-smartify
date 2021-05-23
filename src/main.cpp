#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <SoftwareSerial.h>

#define HOSTNAME "Jura-S95"
#define WIFISSID  "YourWifiSSID"
#define WIFIPASSWORD "YourWifiPassword"

#define GPIORX    12
#define GPIOTX    13

SoftwareSerial softserial(GPIORX, GPIOTX);
ESP8266WebServer webserver(80);

/*
  Common S95-commands:
  AN:01 switches coffeemaker on
  AN:02 switches coffeemaker off
  AN:03 display test
  FA:02 flush
  FA:04 small cup
  FA:05 two small cups
  FA:06 large cup
  FA:07 two large cups
  FA:08 Steam portion
  FA:09 Steam
  FA:0C XXL cup
  RR:03 current state
*/

String cmd2jura(String outbytes) {
  String inbytes;
  int w = 0;

  while (softserial.available()) {
    softserial.read();
  }

  outbytes += "\r\n";
  for (int i = 0; i < outbytes.length(); i++) {
    for (int s = 0; s < 8; s += 2) {
      char rawbyte = 255;
      bitWrite(rawbyte, 2, bitRead(outbytes.charAt(i), s + 0));
      bitWrite(rawbyte, 5, bitRead(outbytes.charAt(i), s + 1));
      softserial.write(rawbyte);
    }
    delay(8);
  }

  int s = 0;
  char inbyte;
  while (!inbytes.endsWith("\r\n")) {
    if (softserial.available()) {
      byte rawbyte = softserial.read();
      bitWrite(inbyte, s + 0, bitRead(rawbyte, 2));
      bitWrite(inbyte, s + 1, bitRead(rawbyte, 5));
      if ((s += 2) >= 8) {
        s = 0;
        inbytes += inbyte;
      }
    } else {
      delay(10);
    }
    if (w++ > 500) {
      return "";
    }
  }

  return inbytes.substring(0, inbytes.length() - 2);
}

void handle_api() {
  String cmd;
  String result;

  if (webserver.method() != HTTP_POST) {
    webserver.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  cmd = webserver.arg("plain");
  if (cmd.length() < 3) {
    webserver.send(400, "text/plain", "Bad Request");
    return;
  }

  digitalWrite(LED_BUILTIN, LOW);
  result = cmd2jura(cmd);
  digitalWrite(LED_BUILTIN, HIGH);

  if (result.length() < 3) {
    webserver.send(503, "text/plain", "Service Unavailable");
    return;
  }

  webserver.send(200, "text/plain", result);
}

void handle_web() {
  String html;

  html  = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><style>button {height: 50px;line-height: 45px;padding-right: 15px;padding-left: 45px;position: relative;background-color:rgb(41,127,184);color: white;text-transform: uppercase;border-radius: 5px;text-shadow:0px 1px 0px rgba(0,0,0,0.5);box-shadow:0px 2px 2px rgba(0,0,0,0.2);}";
  html += "button span {position: absolute;left: 0;width: 40px;height: 45px;border-top-left-radius: 5px;border-bottom-left-radius: 5px;border-right: 1px solid rgba(0,0,0,0.15);}</style></head>";
  html += "<body style='text-align: center;'><div style='width: 100%;max-width: 1000px;margin: 0 auto;'><h1>&#9749; Jura Coffee Machine Gateway</h1>";
  html += "<form><div style='display: flex;justify-content: space-between;'><p>State: <span id=\"state\">Unknown</span></p><button onclick=\"return s('RR:03')\"><span>&#128260;</span>refresh</button></div>";
  html += "<button style='width: 33.333%;height: 50px;' onclick=\"return s('AN:01')\"><span>&#128522;</span>ON</button><button style='width: 33.333%;height: 50px;' onclick=\"return s('AN:02')\"><span>&#128564;</span>OFF</button>";
  html += "<button style='width: 33.333%;height: 50px;' onclick=\"return s('FA:02')\"><span>&#128166;</span>RINSE</button><button style='width: 50%;height: 100px;' onclick=\"return s('FA:0C')\"><span>&#9749;</span>SPECIAL COFFEE</button>";
  html += "<div><button style='width: 50%;height: 100px;' onclick=\"return s('FA:04')\"><span>&#9749;</span>SMALL CUP</button><button style='width: 50%;height: 100px;' onclick=\"return s('FA:06')\"><span>&#9749;</span>LARGE CUP</button><button style='width: 50%;height: 100px;' onclick=\"return s('FA:05')\"><span>&#9749;</span>SMALL CUP (2x)</button><button style='width: 50%;height: 100px;' onclick=\"return s('FA:07')\"><span>&#9749;</span>LARGE CUP (2x)</button></div>";
  html += "<div style='margin: 2rem 0;'>Custom command: <input type=\"text\" placeholder=\"enter command\" id=\"c\" />";
  html += "<input type=\"submit\" value=\"Send command\" onclick=\"return s(c.value())\"/></div></form>";
  html += "<div style='text-align: left;'><h2>Command responses:</h2><ul style=\"font-family: monospace\" id=\"r\"></ul></div></div>";
  html += "<script>function statusHandler(s) { if (s && s.length === 35) { var subValue = s.substring(3, 8); if (subValue.substring(0, 3) === \"000\") { return \"OFF\"; } else if (subValue === \"04002\" || subValue === \"14002\") { return \"Please wait - Heating\"; } else if (subValue === \"040C2\" && s.substring(26, 28) === \"45\") { return \"Tray missing\"; } else if (subValue === \"040C2\" && s.substring(26, 28) === \"04\") { return \"Fill water\"; } else if (subValue === \"04042\") { return \"COFFEE READY\"; } else if (subValue.substring(0, 3) === \"540\" || subValue.substring(0, 3) === \"440\") { return \"Drainage active\"; } else if (subValue.substring(0, 3) === \"250\") { return \"Empty tray\"; } else if (subValue === \"84042\") { return \"Grinding\"; } } return \"Unknown\"; };";
  html += "function s(f) { var x = new XMLHttpRequest();";
  html += "x.open('POST', '/api', true); x.onreadystatechange = function() { if(x.readyState === XMLHttpRequest.DONE && x.status === 200) { ";
  html += "if (f === 'RR:03') { var state = document.getElementById('state'); state.innerHTML = statusHandler(x.responseText); } var r = document.getElementById('r'); r.innerHTML = '<li>' + Date().toLocaleString() + ";
  html += "':&emsp;' + f + '&emsp;&#8594;&emsp;' + x.responseText + '</li>' + r.innerHTML; }}; x.send(f);";
  html += "return false;} document.addEventListener(\"DOMContentLoaded\", function() { s('RR:03'); });</script></body></html>";

  webserver.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFISSID, WIFIPASSWORD);
  Serial.println("Connecting to WiFi (" + String(WIFISSID) + ").");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + String(WiFi.localIP().toString()));

  // Serve site and handle api
  webserver.on("/", handle_web);
  webserver.on("/api", handle_api);
  webserver.begin();

  softserial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  webserver.handleClient();
}