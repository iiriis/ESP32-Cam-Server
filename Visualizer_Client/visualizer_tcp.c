#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <stdint.h>
#include <pthread.h>
#include <GL/glew.h>       // GLEW library
#include <GLFW/glfw3.h>    // GLFW library for window management
#include <SOIL/SOIL.h>     // SOIL library for image loading
// #include <time.h>          // For FPS calculation
#include <math.h>

#define PORT 80
#define BUFFER_SIZE 10000000
#define WIDTH 800
#define HEIGHT 600

WSADATA wsaData;
SOCKET sock;
struct sockaddr_in server_addr;

#pragma comment(lib, "Ws2_32.lib")  // Link with Ws2_32.lib

void* receiveThread(void* arg);
int loadTexture();
void render();
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

uint8_t temp_buff[BUFFER_SIZE];
int idx = 0;  // Current index in buffer
GLuint textureID;
HANDLE semaphore; // Semaphore for signaling new data
int global_image_size;
int new_data = 0;
GLFWwindow* window;

char esp32_ip[18];

uint8_t image_buffer[BUFFER_SIZE];

// FPS variables
double lastTime = 0.0;
int FPS = 0;
double FPS_update_timer;


int main() {
    
    printf("Enter ESP-Cam's IP (192.168.xxx.xxx) : ");
    scanf("%16s", esp32_ip);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Winsock initialization failed\n");
        return 1;
    }

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation error\n");
        WSACleanup();
        return 1;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    int buffer_size = 1024 * 32; // 32 KB, for example
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(buffer_size));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, sizeof(buffer_size));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(esp32_ip);

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Connection failed\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Connected to ESP32 at %s\n", esp32_ip);

    // Start the receiving thread
    pthread_t thread;
    pthread_create(&thread, NULL, receiveThread, NULL);

    // Initialize OpenGL and create a window
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    window = glfwCreateWindow(WIDTH, HEIGHT, "  ", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);  // Set the framebuffer resize callback

    // Initialize GLEW (optional but recommended for modern OpenGL)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return -1;
    }

    // Initialize semaphore
    semaphore = CreateSemaphore(NULL, 0, 1, NULL);

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate FPS


        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    // Clean up
    closesocket(sock);
    WSACleanup();

    return 0;
}

// Thread function to handle receiving image data from ESP32Cam
void* receiveThread(void* arg) {
    int image_size;
    // uint8_t *image_buffer = NULL;

    // Continuous reading loop
    while (1) {
        int bytes_read = recv(sock, (char*)&image_size, sizeof(image_size), 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("Connection closed by the server.\n");
            } else {
                fprintf(stderr, "recv failed: %d\n", WSAGetLastError());
            }
            break;
        }


        int total_received = 0;
        while (total_received < image_size) {
            bytes_read = recv(sock, (char*)(image_buffer + total_received), image_size - total_received, 0);
            if (bytes_read <= 0) {
                fprintf(stderr, "recv failed or connection closed: %d\n", WSAGetLastError());
                break;
            }
            total_received += bytes_read;
        }

        if (total_received == image_size) {
            memcpy(temp_buff, image_buffer, image_size);
            global_image_size = image_size;
            new_data = 1;
            ReleaseSemaphore(semaphore, 1, NULL);
        }
    }
    return NULL;
}

// Resize callback to handle window resizing
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Render function to display the texture
void render() {
    WaitForSingleObject(semaphore, 10);
    if (new_data == 1) {
        if (loadTexture() != 0)  // Reload texture if data has been updated
            return;
        new_data = 0;
    }

    // Clear the screen and render the texture
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, 1.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// Load texture from received data in memory
int loadTexture() {
    if (!(temp_buff[0] == 0xFF && temp_buff[1] == 0xD8 && temp_buff[global_image_size - 1] == 0xD9 && temp_buff[global_image_size - 2] == 0xFF)) {
        printf("Corrupted JPEG\n\n");
        return -1;
    }

    double currentTime = glfwGetTime();
    FPS = round(1/(currentTime - lastTime));
    lastTime = currentTime;

    if (currentTime - FPS_update_timer >= 1.0) { // If last update was over a second ago
        // Create a title string with FPS
        char title[256];
        snprintf(title, sizeof(title), "%16s - FPS: %d",esp32_ip, FPS);
        // Set the window title
        glfwSetWindowTitle(window, title);
        FPS_update_timer = currentTime;
    }

    glDeleteTextures(1, &textureID);
    textureID = 0;

    textureID = SOIL_load_OGL_texture_from_memory(
        temp_buff,
        global_image_size,
        SOIL_LOAD_AUTO,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_INVERT_Y
    );

    if (textureID == 0) {
        fprintf(stderr, "SOIL loading error: '%s'\n", SOIL_last_result());
    }

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return 0;
}
