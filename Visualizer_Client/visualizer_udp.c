#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <stdint.h>
#include <pthread.h>
#include <GL/glew.h>       // GLEW library
#include <GLFW/glfw3.h>    // GLFW library for window management
#include <SOIL/SOIL.h>     // SOIL library for image loading
#include <math.h>

#define PORT 1520
#define BUFFER_SIZE 10000000
#define WIDTH 800
#define HEIGHT 600

WSADATA wsaData;
SOCKET sock;
struct sockaddr_in server_addr;

#pragma comment(lib, "Ws2_32.lib")  // Link with Ws2_32.lib

void* receiveThread(void* arg);
void loadTexture();
void render();
void framebuffer_size_callback(GLFWwindow* window, int width, int height);


uint8_t temp_buff[BUFFER_SIZE];
GLuint textureID;
HANDLE semaphore; // Semaphore for signaling new data
int global_image_size;
int new_data = 0;
GLFWwindow* window;


// FPS variables
double lastTime = 0.0;
int FPS = 0;
double FPS_update_timer;


int main() {
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Winsock initialization failed\n");
        return 1;
    }

    // Create a UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation error\n");
        WSACleanup();
        return 1;
    }

    // Set the socket buffer size
    int buffer_size = 1024 * 5;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(buffer_size));

    // Prepare server address for receiving broadcast
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind to the port
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Bind failed\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Listening for UDP packets\n");


    // Initialize OpenGL and create a window
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    window = glfwCreateWindow(WIDTH, HEIGHT, "Broadcast ", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);  // Set the framebuffer resize callback


    // pthread_attr_t attr;
    // pthread_attr_init(&attr);
    
    // // Set the stack size for the thread
    // pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
    
    // Start the receiving thread
    pthread_t thread;
    int result = pthread_create(&thread, NULL, receiveThread, NULL);
    if (result != 0) {
        fprintf(stderr, "Thread creation failed with error code: %d\n", result);
        return 1; // Exit program if thread creation fails
    }

    // Initialize GLEW
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
        // double currentTime = glfwGetTime();
        // nbFrames++;
        // if (currentTime - lastTime >= 1.0) { // If last update was over a second ago
        //     // Create a title string with FPS
        //     char title[256];
        //     snprintf(title, sizeof(title), "Broadcasted Stream - FPS: %d", nbFrames);

        //     // Set the window title
        //     glfwSetWindowTitle(window, title);

        //     nbFrames = 0;
        //     lastTime += 1.0;
        // }

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

void* receiveThread(void* arg) {
    enum {
        WAIT_FOR_START,
        STORE_DATA,
        COMPLETE
    } state = WAIT_FOR_START;

    uint8_t buffer[BUFFER_SIZE]; // Temporary buffer to store the incoming data
    uint8_t *image_data = temp_buff; // The main buffer to hold the image once complete
    int bytes_received;
    int image_size = 0;
    int from_len = sizeof(struct sockaddr_in);
    struct sockaddr_in from_addr;

    while (1) {
        bytes_received = recvfrom(sock, (char*)buffer, BUFFER_SIZE, 0, (struct sockaddr*)&from_addr, &from_len);

        if (bytes_received <= 0) {
            fprintf(stderr, "recvfrom failed: %d\n", WSAGetLastError());
            continue;
        }

        for (int i = 0; i < bytes_received; i++) {
            uint8_t byte = buffer[i];

            switch (state) {
                case WAIT_FOR_START:
                    // Check for JPEG start marker 0xFF 0xD8
                    if (byte == 0xFF && buffer[i + 1] == 0xD8) {
                        state = STORE_DATA;
                        image_data[0] = 0xFF;
                        image_data[1] = 0xD8;
                        image_size = 2;
                        i++; // Skip the next byte since we know it's 0xD8
                        // printf("Start of JPEG detected.\n");
                    }
                    break;

                case STORE_DATA:
                    // Add current byte to image buffer
                    image_data[image_size++] = byte;

                    // Check if the last two bytes in the buffer are the end marker 0xFF 0xD9
                    if (image_size > 1 && image_data[image_size - 2] == 0xFF && image_data[image_size - 1] == 0xD9) {
                        state = COMPLETE;
                        // printf("End of JPEG detected. Image size: %d bytes\n", image_size);
                    }
                    // Handle buffer overflow
                    if (image_size >= BUFFER_SIZE) {
                        fprintf(stderr, "Buffer overflow, resetting.\n");
                        state = WAIT_FOR_START;
                        image_size = 0;
                    }
                    break;

                case COMPLETE:
                    // Signal that a full image has been received and is ready to process
                    global_image_size = image_size;
                    new_data = 1;
                    ReleaseSemaphore(semaphore, 1, NULL);

                    // for(int i=0;i<global_image_size;i++)
                    //     printf("%02X ", temp_buff[i]);
                    
                    // printf("\n\n\n\n");

                    // Reset state and index for the next image
                    state = WAIT_FOR_START;
                    image_size = 0;
                    break;
            }
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
    // Wait for new image data to be ready
    WaitForSingleObject(semaphore, 10);

    if (new_data == 1) {
        loadTexture();  // Reload texture if data has been updated
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
void loadTexture() {
    // Check if data is a valid JPEG
    if (!(temp_buff[0] == 0xFF && temp_buff[1] == 0xD8 && temp_buff[global_image_size - 1] == 0xD9)) {
        printf("Corrupted JPEG\n");
        return;
    }

    //FPS Calculation
    double currentTime = glfwGetTime();
    FPS = round(1/(currentTime - lastTime));
    lastTime = currentTime;

    if (currentTime - FPS_update_timer >= 1.0) { // If last update was over a second ago
        // Create a title string with FPS
        char title[256];
        snprintf(title, sizeof(title), "FPS: %d", FPS);
        // Set the window title
        glfwSetWindowTitle(window, title);
        FPS_update_timer = currentTime;
    }


    // Free previous texture memory and reset textureID
    glDeleteTextures(1, &textureID);
    textureID = 0;

    // Load texture from memory using SOIL
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

    // Set up texture parameters
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}



// Save buffer data to a JPEG file
void saveToFile(uint8_t *buffer, int size) {
    static int fileCounter = 0;
    char filename[50];
    sprintf(filename, "jpeg_image_%d.jpg", fileCounter++);

    FILE *fp = fopen(filename, "wb");
    if (fp) {
        fwrite(buffer, 1, size, fp);
        fclose(fp);
        printf("Saved JPEG to %s\n", filename);
    } else {
        printf("Failed to save JPEG\n");
    }
}
