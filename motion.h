/*
 *  Motion detection from camera image - 31Jan20 
 * 
 *  original code from: https://eloquentarduino.github.io/2020/01/motion-detection-with-esp32-cam-only-arduino-version/
 * 
 * 
 * This works by capturing a greyscale image from the camera, splitting this up in to blocks of pixels (BLOCK_SIZE x BLOCK_SIZE)
 * then reading all the pixel values inside each block and producing an average value for the block. 
 * The previous frames block values are then compared with the current and the number of blocks which have changed beyond
 * a threshold (block_threshold) are counted.  If enough of the blocks have changed beyond a threshold (image_threshold)
 * then motion is detected.
 * - Many thanks to eloquentarduino for creating this code and for taking the time to answer my questions whilst I was 
 *   developing my security camera sketch.
 *  
 * For info on the camera module see: https://github.com/espressif/esp32-camera
 * 
*/

#define DEBUG_MOTION 0        // serial debug enable for motion.h


// Settings
  #define CAMERA_MODEL_AI_THINKER              // type of camera
  #define FRAME_SIZE_MOTION FRAMESIZE_QVGA     // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA - Do not use sizes above QVGA when not JPEG
  #define FRAME_SIZE_PHOTO FRAMESIZE_XGA       //   160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
  #define BLOCK_SIZE 10                        // size of image blocks used for motion sensing

// camera type (CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32      // power down pin
  #define RESET_GPIO_NUM    -1      // -1 = not used
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26      // i2c sda
  #define SIOC_GPIO_NUM     27      // i2c scl
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25      // vsync_pin
  #define HREF_GPIO_NUM     23      // href_pin
  #define PCLK_GPIO_NUM     22      // pixel_clock_pin

  
//   ---------------------------------------------------------------------------------------------------------------------


#include "esp_camera.h"       // https://github.com/espressif/esp32-camera
  
// detection parameters (these are set by user and stored in Spiffs)
    int block_threshold = 20;
    int image_threshold = 15;

// misc     
  #define WIDTH 320                 // motion sensing frame size
  #define HEIGHT 240
  #define W (WIDTH / BLOCK_SIZE)
  #define H (HEIGHT / BLOCK_SIZE)
  
// frame stores (blocks)
    uint16_t prev_frame[H][W] = { 0 };
    uint16_t current_frame[H][W] = { 0 };

// pre declare procedures
    bool setup_camera(framesize_t);
    bool capture_still();
    float motion_detect();
    void update_frame();
    void print_frame(uint16_t frame[H][W]);

// camera configuration settings
    camera_config_t config;


//   ---------------------------------------------------------------------------------------------------------------------

    
/**
 * Setup camera
 */

bool setup_camera() {
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;               // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    config.pixel_format = PIXFORMAT_GRAYSCALE;    // PIXFORMAT_ + YUV422,GRAYSCALE,RGB565,JPEG
    config.frame_size = FRAME_SIZE_MOTION;        // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA 
    config.jpeg_quality = 12;                     //0-63 lower number means higher quality
    config.fb_count = 1;                          //if more than one, i2s runs in continuous mode. Use only with JPEG

    bool ok = esp_camera_init(&config) == ESP_OK;

    sensor_t *sensor = esp_camera_sensor_get();
    sensor->set_framesize(sensor, FRAME_SIZE_MOTION);

    return ok;
}


//   ---------------------------------------------------------------------------------------------------------------------


/**
 * Capture image and down-sample in to blocks
 *   this sets all blocks to value zero then goes through each pixel in the greyscale image and adds its value to 
 *   the blocks total that it is in.  After this each blocks value is divided by the number of pixels in it 
 *   resulting in each blocks value being the average value of all the pixels within it.
 */
bool capture_still() {

    Serial.flush();   // wait for serial data to be sent first as suspected this may cause interference (now think this not required?)

    camera_fb_t *frame_buffer = esp_camera_fb_get();          // capture frame from camera

    if (!frame_buffer) return false;

    // set all 0s in current frame
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            current_frame[y][x] = 0;


    // down-sample image in blocks
    for (uint32_t i = 0; i < WIDTH * HEIGHT; i++) {
        const uint16_t x = i % WIDTH;
        const uint16_t y = floor(i / WIDTH);
        const uint8_t block_x = floor(x / BLOCK_SIZE);
        const uint8_t block_y = floor(y / BLOCK_SIZE);
        const uint8_t pixel = frame_buffer->buf[i];
        const uint16_t current = current_frame[block_y][block_x];

    // accumulate all the pixels in each block
        current_frame[block_y][block_x] += pixel;
    }

    // average pixels in each block (rescale)
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            current_frame[y][x] /= BLOCK_SIZE * BLOCK_SIZE;

#if DEBUG_MOTION
    Serial.println("Current frame:");
    print_frame(current_frame);
    Serial.println("---------------");
#endif

    return true;
}


//   ---------------------------------------------------------------------------------------------------------------------


/**
 * Compute the number of different blocks.  If there are enough, then motion happened
 *    returns the average number of changes per block
 */
float motion_detect() {
    uint16_t changes = 0;
    const uint16_t blocks = (WIDTH * HEIGHT) / (BLOCK_SIZE * BLOCK_SIZE);
    
    float threshold_dec = (float)(block_threshold / 100.0);     // convert from percentage to decimal 

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float current = current_frame[y][x];
            float prev = prev_frame[y][x];
            float delta = abs(current - prev) / prev;

            if (delta >= threshold_dec) {
#if DEBUG_MOTION
                Serial.print("diff\t");
                Serial.print(y);
                Serial.print('\t');
                Serial.println(x);
#endif

                changes += 1;
            }
        }
    }

#if DEBUG_MOTION
    Serial.print("Changed ");
    Serial.print(changes);
    Serial.print(" out of ");
    Serial.println(blocks);
#endif

    return (1.0 * changes / blocks);
}


//   ---------------------------------------------------------------------------------------------------------------------


/**
 * Copy current frame to previous
 */
void update_frame() {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            prev_frame[y][x] = current_frame[y][x];
        }
    }
}


//   ---------------------------------------------------------------------------------------------------------------------


/**
 * For serial debugging
 * @param frame
 */
void print_frame(uint16_t frame[H][W]) {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            Serial.print(frame[y][x]);
            Serial.print('\t');
        }

        Serial.println();
    }
}

// ------------------------------------------------- end ----------------------------------------------------------------
