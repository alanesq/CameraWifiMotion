/**************************************************************************************************
 *  
 *  Motion detection from camera image - 09Feb20 
 * 
 *  original code from: https://eloquentarduino.github.io/2020/01/motion-detection-with-esp32-cam-only-arduino-version/
 * 
 * 
 * This works by capturing a greyscale image from the camera, splitting this up in to blocks of pixels (BLOCK_SIZE x BLOCK_SIZE)
 * then reading all the pixel values inside each block and producing an average value for the block. 
 * The previous frames block values are then compared with the current and the number of blocks which have changed beyond
 * a threshold (block_threshold) are counted.  If enough of the blocks have changed between two thresholds (image_thresholdL and H)
 * then motion is detected.
 * - Many thanks to eloquentarduino for creating this code and for taking the time to answer my questions whilst I was 
 *   developing this security camera sketch.
 *  
 * For info on the camera module see: https://github.com/espressif/esp32-camera
 * 
 **************************************************************************************************/

#define DEBUG_MOTION 0        // serial debug enable for motion.h

#include "esp_camera.h"       // https://github.com/espressif/esp32-camera


// Settings
  #define CAMERA_MODEL_AI_THINKER              // type of camera
  #define FRAME_SIZE_MOTION FRAMESIZE_QVGA     // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA - Do not use sizes above QVGA when not JPEG
  #define FRAME_SIZE_PHOTO FRAMESIZE_XGA       //   Image sizes: 160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
  #define BLOCK_SIZE 20                        // size of image blocks used for motion sensing

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

  
// detection parameters (these are set by user and stored in Spiffs)
    uint16_t block_threshold = 10;     // average pixel variation in block required to count as changed - range 0 to 255
    uint16_t image_thresholdL = 15;     // min changed blocks in image required to count as movement detected in percent
    uint16_t image_thresholdH = 100;    // max changed blocks in image required to count as movement detected in percent

// misc     
  #define WIDTH 320                 // motion sensing frame size
  #define HEIGHT 240
  #define W (WIDTH / BLOCK_SIZE)
  #define H (HEIGHT / BLOCK_SIZE)
  uint32_t AveragePix = 0;          // average pixel reading from captured image (used for nighttime compensation) - bright day = around 120

// store most current readings for display on main page
    uint16_t latestChanges = 0;

// frame stores (blocks)
    uint16_t prev_frame[H][W] = { 0 };      // last captured frame
    uint16_t current_frame[H][W] = { 0 };   // current frame
    
// Image detection mask (i.e. if area of image is enabled for use when motion sensing, 1=active)
//   4 x 3 grid results in mask sections of 16 blocks (4x4) - image = 320x240 pixels, blocks = 20x20 pixels
    bool mask_frame[4][3] = { {1,1,1},
                              {1,1,1},
                              {1,1,1},
                              {1,1,1} };
    uint16_t mask_active = 12;     // number of mask blocks active (used to calculate trigger as percentage of active screen area)
    
// forward delarations
    bool setup_camera(framesize_t);
    bool capture_still();
    float motion_detect();
    void update_frame();
    void print_frame(uint16_t frame[H][W]);
    bool block_active(int x, int y);

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

    Serial.flush();   // wait for serial data to be sent first as I suspect this can cause problems capturing an image 

    AveragePix = 0;     // average pixel reading

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
  
      AveragePix += pixel;    // add pixel to average counter
    }

    AveragePix = AveragePix / (WIDTH * HEIGHT);     // convert to average

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
    const uint16_t blocks = (WIDTH * HEIGHT) / (BLOCK_SIZE * BLOCK_SIZE);     // total number of blocks in image
    
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float current = current_frame[y][x];
            float prev = prev_frame[y][x];
            float delta = abs(current - prev);             // modified code Feb20 - gives blocks average pixels variation in range 0 to 255
            // float delta = abs(current - prev) / prev;   // original code 
            if (delta >= block_threshold) {                // if change in block has changed enough to qualify
              if (block_active(x,y)) changes += 1;          // if detection mask is enabled for this block increment changed block count
#if DEBUG_MOTION
                Serial.print("diff\t");
                Serial.print(y);
                Serial.print('\t');
                Serial.println(x);
#endif
            }
        }
    }

    float tblocks = (blocks / 12.0) * mask_active;       // blocks in active mask

    if (changes > latestChanges) latestChanges = changes;           // store latest readings for display on main page (it is zeroed when displayed)
      
#if DEBUG_MOTION
    Serial.print("Changed ");
    Serial.print(changes);
    Serial.print(" out of ");
    Serial.println(tblocks);
#endif

    return float(changes / tblocks);         // number of changed blocks in range 0 to 1
}


//   ---------------------------------------------------------------------------------------------------------------------

/**
 * Is this image block active in the detection mask  (mask area is a 4 x 3 grid)
 *    returns 1 for active, 0 for disabled
 */

bool block_active(int x, int y) {

    uint16_t maskW = W / 4;                    // find pixels in each mask area 
    uint16_t maskH = H / 3;

    // Which mask area is this block in 
      uint16_t Maskx = floor(x / maskW);       // x mask area (0 to 3)
      uint16_t Masky = floor(y / maskH);       // y mask area (0 to 2)
   
    return mask_frame[Maskx][Masky];
      
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
