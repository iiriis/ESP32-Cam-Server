#include <WiFi.h>
#include <WiFiClient.h>

#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LED_BUILTIN 33


const char* ssid = "Ping";
const char* password = "Pong";


WiFiServer server(80);  // Start a server on port 80
camera_config_t config;

void CameraSetup() {
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 5;
    config.pin_d1 = 18;
    config.pin_d2 = 19;
    config.pin_d3 = 21;
    config.pin_d4 = 36;
    config.pin_d5 = 39;
    config.pin_d6 = 34;
    config.pin_d7 = 35;
    config.pin_xclk = 0;
    config.pin_pclk = 22;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_sscb_sda = 26;
    config.pin_sscb_scl = 27;
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_SVGA;
    // config.frame_size = FRAME
    config.jpeg_quality = 5;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;


    // Initialize camera
    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera init failed");
        return;
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);        
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("ESP32-Cam IP address: ");
    Serial.println(WiFi.localIP());

    CameraSetup();
    server.begin();  // Start the HTTP server

    digitalWrite(LED_BUILTIN, LOW);
}

void loop() {

    WiFiClient client = server.available();  // Wait for a client to connect
    if (client) {
        Serial.println("Client connected");
        static int img_ctr;
        while (client.connected()) {
            camera_fb_t *fb = esp_camera_fb_get(); // Get a frame

            if (fb) {
                uint64_t ls = micros();
                int image_size = fb->len;/* determine image size here */;
                client.write((const uint8_t*)&image_size, sizeof(image_size));  // Send the size header
                client.write(fb->buf, fb->len);  // Send the frame to the client
                
                client.flush();

                Serial.printf("Image sent %d of size %d bytes in %lu uS\n",++img_ctr, fb->len, (micros() - ls));
                esp_camera_fb_return(fb);         // Return the frame buffer
            }
            else
              printf("Couldnt get frame buffer\n");
            
            delay(10);  // Adjust based on desired frame rate
        }

        client.stop();  // Disconnect client
        Serial.println("Client disconnected");
    }
}