#include <WiFi.h>
#include <esp_camera.h>
#include <WiFiUdp.h>

#define LED_BUILTIN 33


const char* ssid = "Ping";
const char* password = "Pong";

const char* udpAddress = "255.255.255.255"; // Broadcast address
const int udpPort = 1520;                   // UDP port to send data

WiFiUDP udp;

void setup() {
    Serial.begin(115200);
    
    pinMode(LED_BUILTIN, OUTPUT);

    // Initialize camera
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 5;   // Change pins according to your wiring
    config.pin_d1 = 18;  // Change pins according to your wiring
    config.pin_d2 = 19;  // Change pins according to your wiring
    config.pin_d3 = 21;  // Change pins according to your wiring
    config.pin_d4 = 36;  // Change pins according to your wiring
    config.pin_d5 = 39;  // Change pins according to your wiring
    config.pin_d6 = 34;  // Change pins according to your wiring
    config.pin_d7 = 35;  // Change pins according to your wiring
    config.pin_xclk = 0; // Change pins according to your wiring
    config.pin_pclk = 22; // Change pins according to your wiring
    config.pin_vsync = 25; // Change pins according to your wiring
    config.pin_href = 23; // Change pins according to your wiring
    config.pin_sscb_sda = 26; // Change pins according to your wiring
    config.pin_sscb_scl = 27; // Change pins according to your wiring
    config.pin_pwdn = 32; // Change pins according to your wiring
    config.pin_reset = -1; // Reset pin (optional)
    config.xclk_freq_hz = 20000000; // Set clock frequency
    config.pixel_format = PIXFORMAT_JPEG; // Set pixel format to JPEG
      config.frame_size = FRAMESIZE_SVGA;
    // config.frame_size = FRAME
    config.jpeg_quality = 5;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;


    // Initialize the camera
    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera init failed");
        return;
    }

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);   
        Serial.print(".");
    }
    Serial.println("Connected to Wi-Fi");
    Serial.println(WiFi.localIP());

    digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
    // Capture an image
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    static int img_ctr;
    // Send the image size first (4 bytes)
    uint32_t image_size = fb->len;
    udp.beginPacket(udpAddress, udpPort);
    udp.write((uint8_t*)&image_size, sizeof(image_size)); // Send image size
    udp.endPacket();

    uint64_t ls = micros();

    // Send the image data
    udp.beginPacket(udpAddress, udpPort);
    udp.write(fb->buf, fb->len); // Send image data
    udp.endPacket();

    udp.flush();
    
    Serial.printf("Image sent %d of size %d bytes in %lu uS\n",++img_ctr, fb->len, (micros() - ls));


    // Return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);

    delay(20); // Delay between captures (adjust as necessary)
}
