//--------------------------------------------------------------------------
// Uncanny eyes for Adafruit 1.5" OLED (product #1431) or 1.44" TFT LCD
// (#2088).  Works on PJRC Teensy 3.x and on Adafruit M0 and M4 boards
// (Feather, Metro, etc.).  This code uses features specific to these
// boards and WILL NOT work on normal Arduino or other boards!
//
// SEE FILE "config.h" FOR MOST CONFIGURATION (graphics, pins, display type,
// etc).  Probably won't need to edit THIS file unless you're doing some
// extremely custom modifications.
//
// Adafruit invests time and resources providing this open source code,
// please support Adafruit and open-source hardware by purchasing products
// from Adafruit!
//
// Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.
// MIT license.  SPI FIFO insight from Paul Stoffregen's ILI9341_t3 library.
// Inspired by David Boccabella's (Marcwolf) hybrid servo/OLED eye concept.
//--------------------------------------------------------------------------
#include <mbed.h>
#include <SSD1351.h>
// Configuraion is done in the following header.
#include "config.h"


// Number of eyes is based on eyeInfo array size in config.h
#define NUM_EYES (sizeof eyeInfo / sizeof eyeInfo[0]) // config.h pin list

// Define screen limits.
#define SCREEN_X_START 0
#define SCREEN_X_END   SCREEN_WIDTH
#define SCREEN_Y_START 0
#define SCREEN_Y_END   SCREEN_HEIGHT

// A simple state machine is used to control eye blinks/winks:
#define NOBLINK 0       // Not currently engaged in a blink
#define ENBLINK 1       // Eyelid is currently closing
#define DEBLINK 2       // Eyelid is currently opening
typedef struct {
  uint8_t  state;       // NOBLINK/ENBLINK/DEBLINK
  uint32_t duration;    // Duration of blink state (micros)
  uint32_t startTime;   // Time (micros) of last state change
} eyeBlink;

struct {            // One-per-eye structure
  SSD1351* display; // -> OLED/TFT object
  eyeBlink blink;   // Current blink/wink state
} g_eye[NUM_EYES];

static uint32_t       g_startTime;  // For FPS indicator
static FastSpiWriter  g_spi(OLED_MOSI_PIN, NC, OLED_SCK_PIN, NC);
static Timer          g_timer;
static AnalogIn       g_analog(ANALOG_PIN);


// INITIALIZATION -- runs once at startup ----------------------------------
static void setup(void)
{
  uint8_t e; // Eye index, 0 to NUM_EYES-1

  printf("Init\n");
  srand(g_analog.read());

  // Initialize eye objects based on eyeInfo list in config.h:
  for(e=0; e<NUM_EYES; e++) {
    printf("Create display #%u\n", e);
    // Only setup the first display to perform the reset for both.
    g_eye[e].display     = new SSD1351(OLED_WIDTH, OLED_HEIGHT, &g_spi, OLED_DC_PIN, e==0 ? OLED_RST_PIN : NC, eyeInfo[e].select);
    g_eye[e].blink.state = NOBLINK;
  }

  printf("Rotate\n");
  for(e=0; e<NUM_EYES; e++) {
    g_eye[e].display->init();
    g_eye[e].display->setRotation(eyeInfo[e].rotation);
  }
  printf("done\n");

#if defined(LOGO_TOP_WIDTH) || defined(COLOR_LOGO_WIDTH)
  // I noticed lots of folks getting right/left eyes flipped, or
  // installing upside-down, etc.  Logo split across screens may help:
  int x = g_eye[0].display->width() * NUM_EYES / 2;
  int y = (g_eye[0].display->height() - SCREEN_HEIGHT) / 2;
  for(e=0; e<NUM_EYES; e++) { // Another pass, after all screen inits
    g_eye[e].display->fillScreen(0);
    #ifdef LOGO_TOP_WIDTH
      // Monochrome Adafruit logo is 2 mono bitmaps:
      g_eye[e].display->drawBitmap(x - LOGO_TOP_WIDTH / 2 - 20,
        y, logo_top, LOGO_TOP_WIDTH, LOGO_TOP_HEIGHT, 0xFFFF);
      g_eye[e].display->drawBitmap(x - LOGO_BOTTOM_WIDTH/2,
        y + LOGO_TOP_HEIGHT, logo_bottom, LOGO_BOTTOM_WIDTH, LOGO_BOTTOM_HEIGHT,
        0xFFFF);
    #else
      // Color sponsor logo is one RGB bitmap:
      g_eye[e].display->fillScreen(color_logo[0]);
      g_eye[e].display->drawRGBBitmap(
        (g_eye[e].display->width()  - COLOR_LOGO_WIDTH ) / 2,
        (g_eye[e].display->height() - COLOR_LOGO_HEIGHT) / 2,
        color_logo, COLOR_LOGO_WIDTH, COLOR_LOGO_HEIGHT);
    #endif
    x -= g_eye[e].display->width();
  }
  wait_ms(2000); // Pause for screen layout/orientation
#endif // LOGO_TOP_WIDTH

  // One of the displays is configured to mirror on the X axis.  Simplifies
  // eyelid handling in the drawEye() function -- no need for distinct
  // L-to-R or R-to-L inner loops.  Just the X coordinate of the iris is
  // then reversed when drawing this eye, so they move the same.  Magic!
  // Mirror the right eye.
  for(e=0; e<NUM_EYES; e++) {
    g_eye[e].display->mirrorDisplay(e != 0);
  }

  g_timer.start();
  g_startTime = g_timer.read_ms(); // For frame-rate calculation
}


// EYE-RENDERING FUNCTION --------------------------------------------------
static void drawEye( // Renders one eye.  Inputs must be pre-clipped & valid.
  uint8_t  e,       // Eye array index; 0 or 1 for left/right
  uint16_t iScale,  // Scale factor for iris (0-1023)
  uint8_t  scleraX, // First pixel X offset into sclera image
  uint8_t  scleraY, // First pixel Y offset into sclera image
  uint8_t  uT,      // Upper eyelid threshold value
  uint8_t  lT)      // Lower eyelid threshold value
{
  uint8_t  screenX, screenY, scleraXsave;
  int16_t  irisX, irisY;
  uint16_t p, a;
  uint32_t d;

  uint8_t  irisThreshold = (128 * (1023 - iScale) + 512) / 1024;
  uint32_t irisScale     = IRIS_MAP_HEIGHT * 65536 / irisThreshold;

  // Set up raw pixel dump to entire screen.  Although such writes can wrap
  // around automatically from end of rect back to beginning, the region is
  // reset on each frame here in case of an SPI glitch.
  g_eye[e].display->setAddrWindow(0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1);

  // Now just issue raw 16-bit values for every pixel...
  scleraXsave = scleraX + SCREEN_X_START; // Save initial X value to reset on each line
  irisY       = scleraY - (SCLERA_HEIGHT - IRIS_HEIGHT) / 2;
  for(screenY=SCREEN_Y_START; screenY<SCREEN_Y_END; screenY++, scleraY++, irisY++) {
    scleraX = scleraXsave;
    irisX   = scleraXsave - (SCLERA_WIDTH - IRIS_WIDTH) / 2;
    for(screenX=SCREEN_X_START; screenX<SCREEN_X_END; screenX++, scleraX++, irisX++) {
      if((lower[screenY][screenX] <= lT) ||
         (upper[screenY][screenX] <= uT)) {             // Covered by eyelid
        p = 0;
      } else if((irisY < 0) || (irisY >= IRIS_HEIGHT) ||
                (irisX < 0) || (irisX >= IRIS_WIDTH)) { // In sclera
        p = sclera[scleraY][scleraX];
      } else {                                          // Maybe iris...
        p = polar[irisY][irisX];                        // Polar angle/dist
        d = p & 0x7F;                                   // Distance from edge (0-127)
        if(d < irisThreshold) {                         // Within scaled iris area
          d = d * irisScale / 65536;                    // d scaled to iris image height
          a = (IRIS_MAP_WIDTH * (p >> 7)) / 512;        // Angle (X)
          p = iris[d][a];                               // Pixel = iris
        } else {                                        // Not in iris
          p = sclera[scleraY][scleraX];                 // Pixel = sclera
        }
      }
      g_eye[e].display->pushColor(p);
    } // end column
  } // end scanline
}

// EYE ANIMATION -----------------------------------------------------------
const uint8_t ease[] = { // Ease in/out curve for eye movements 3*t^2-2*t^3
    0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  3,   // T
    3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  7,  8,  9,  9, 10, 10,   // h
   11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 18, 19, 20, 21, 22, 23,   // x
   24, 25, 26, 27, 27, 28, 29, 30, 31, 33, 34, 35, 36, 37, 38, 39,   // 2
   40, 41, 42, 44, 45, 46, 47, 48, 50, 51, 52, 53, 54, 56, 57, 58,   // A
   60, 61, 62, 63, 65, 66, 67, 69, 70, 72, 73, 74, 76, 77, 78, 80,   // l
   81, 83, 84, 85, 87, 88, 90, 91, 93, 94, 96, 97, 98,100,101,103,   // e
  104,106,107,109,110,112,113,115,116,118,119,121,122,124,125,127,   // c
  128,130,131,133,134,136,137,139,140,142,143,145,146,148,149,151,   // J
  152,154,155,157,158,159,161,162,164,165,167,168,170,171,172,174,   // a
  175,177,178,179,181,182,183,185,186,188,189,190,192,193,194,195,   // c
  197,198,199,201,202,203,204,205,207,208,209,210,211,213,214,215,   // o
  216,217,218,219,220,221,222,224,225,226,227,228,228,229,230,231,   // b
  232,233,234,235,236,237,237,238,239,240,240,241,242,243,243,244,   // s
  245,245,246,246,247,248,248,249,249,250,250,251,251,251,252,252,   // o
  252,253,253,253,254,254,254,254,254,255,255,255,255,255,255,255 }; // n

#ifdef AUTOBLINK
uint32_t timeOfLastBlink = 0L, timeToNextBlink = 0L;
#endif

static int map(int val, int fromLow, int fromHigh, int toLow, int toHigh)
{
  return (val - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

static void frame( // Process motion for a single frame of left or right eye
  uint16_t        iScale)     // Iris scale (0-1023) passed in
{
  static uint32_t frames   = 0; // Used in frame rate calculation
  static uint8_t  eyeIndex = 0; // g_eye[] array counter
  int16_t         eyeX, eyeY;
  uint32_t        t = g_timer.read_us(); // Time at start of function

  if(!(++frames & 255)) { // Every 256 frames...
    uint32_t elapsed = g_timer.read_ms() - g_startTime;
    if(elapsed) printf("%lu\n", frames * 1000 / elapsed); // Print FPS
  }

  if(++eyeIndex >= NUM_EYES) eyeIndex = 0; // Cycle through eyes, 1 per call

  // Autonomous X/Y eye motion
  // Periodically initiates motion to a new random point, random speed,
  // holds there for random period until next motion.
  static bool     eyeInMotion      = false;
  static int16_t  eyeOldX=512, eyeOldY=512, eyeNewX=512, eyeNewY=512;
  static uint32_t eyeMoveStartTime = 0L;
  static int32_t  eyeMoveDuration  = 0L;

  int32_t dt = t - eyeMoveStartTime;      // uS elapsed since last eye event
  if(eyeInMotion) {                       // Currently moving?
    if(dt >= eyeMoveDuration) {           // Time up?  Destination reached.
      eyeInMotion      = false;           // Stop moving
      eyeMoveDuration  = rand()%3000000;  // 0-3 sec stop
      eyeMoveStartTime = t;               // Save initial time of stop
      eyeX = eyeOldX = eyeNewX;           // Save position
      eyeY = eyeOldY = eyeNewY;
    } else { // Move time's not yet fully elapsed -- interpolate position
      int16_t e = ease[255 * dt / eyeMoveDuration] + 1;   // Ease curve
      eyeX = eyeOldX + (((eyeNewX - eyeOldX) * e) / 256); // Interp X
      eyeY = eyeOldY + (((eyeNewY - eyeOldY) * e) / 256); // and Y
    }
  } else {                                // Eye stopped
    eyeX = eyeOldX;
    eyeY = eyeOldY;
    if(dt > eyeMoveDuration) {            // Time up?  Begin new move.
      int16_t  dx, dy;
      uint32_t d;
      do {                                // Pick new dest in circle
        eyeNewX = rand() % 1024;
        eyeNewY = rand() % 1024;
        dx      = (eyeNewX * 2) - 1023;
        dy      = (eyeNewY * 2) - 1023;
      } while((d = (dx * dx + dy * dy)) > (1023 * 1023)); // Keep trying
      eyeMoveDuration  = rand()%(144000-72000)+72000; // ~1/14 - ~1/7 sec
      eyeMoveStartTime = t;               // Save initial time of move
      eyeInMotion      = true;            // Start move on next frame
    }
  }

  // Blinking
#ifdef AUTOBLINK
  // Similar to the autonomous eye movement above -- blink start times
  // and durations are random (within ranges).
  if((t - timeOfLastBlink) >= timeToNextBlink) { // Start new blink?
    timeOfLastBlink = t;
    uint32_t blinkDuration = rand()%(72000-36000)+36000; // ~1/28 - ~1/14 sec
    // Set up durations for both eyes (if not already winking)
    for(uint8_t e=0; e<NUM_EYES; e++) {
      if(g_eye[e].blink.state == NOBLINK) {
        g_eye[e].blink.state     = ENBLINK;
        g_eye[e].blink.startTime = t;
        g_eye[e].blink.duration  = blinkDuration;
      }
    }
    timeToNextBlink = blinkDuration * 3 + rand()%4000000;
  }
#endif

  if(g_eye[eyeIndex].blink.state) { // Eye currently blinking?
    // Check if current blink state time has elapsed
    if((t - g_eye[eyeIndex].blink.startTime) >= g_eye[eyeIndex].blink.duration) {
      // Yes -- increment blink state
      if(++g_eye[eyeIndex].blink.state > DEBLINK) { // Deblinking finished?
        g_eye[eyeIndex].blink.state = NOBLINK;      // No longer blinking
      } else { // Advancing from ENBLINK to DEBLINK mode
        g_eye[eyeIndex].blink.duration *= 2; // DEBLINK is 1/2 ENBLINK speed
        g_eye[eyeIndex].blink.startTime = t;
      }
    }
  }

  // Process motion, blinking and iris scale into renderable values

  if (eyeIndex == 1) {
    // UNDONE: Specific to my pumpkin mounting.
    // The eyes are mounted a bit askew in my pumpkin so correct them here.
    eyeX -= 128;
    eyeY += 224;
    if (eyeX > 1023) eyeX = 1023;
    if (eyeX < 0) eyeX = 0;
    if (eyeY > 1023) eyeY = 1023;
    if (eyeY < 0) eyeY = 0;
  }

  // Scale eye X/Y positions (0-1023) to pixel units used by drawEye()
  eyeX = map(eyeX, 0, 1023, 0, SCLERA_WIDTH  - 128);
  eyeY = map(eyeY, 0, 1023, 0, SCLERA_HEIGHT - 128);
  if(eyeIndex == 1) eyeX = (SCLERA_WIDTH - 128) - eyeX; // Mirrored display

  // Horizontal position is offset so that eyes are very slightly crossed
  // to appear fixated (converged) at a conversational distance.  Number
  // here was extracted from my posterior and not mathematically based.
  // I suppose one could get all clever with a range sensor, but for now...
  // UNDONE: if(NUM_EYES > 1) eyeX += 4;
  if(eyeX > (SCLERA_WIDTH - 128)) eyeX = (SCLERA_WIDTH - 128);

  // Eyelids are rendered using a brightness threshold image.  This same
  // map can be used to simplify another problem: making the upper eyelid
  // track the pupil (eyes tend to open only as much as needed -- e.g. look
  // down and the upper eyelid drops).  Just sample a point in the upper
  // lid map slightly above the pupil to determine the rendering threshold.
  static uint8_t uThreshold = 128;
  uint8_t        lThreshold, n;
#ifdef TRACKING
  int16_t sampleX = SCLERA_WIDTH  / 2 - (eyeX / 2), // Reduce X influence
          sampleY = SCLERA_HEIGHT / 2 - (eyeY + IRIS_HEIGHT / 4);
  // Eyelid is slightly asymmetrical, so two readings are taken, averaged
  if(sampleY < 0) n = 0;
  else            n = (upper[sampleY][sampleX] +
                       upper[sampleY][SCREEN_WIDTH - 1 - sampleX]) / 2;
  uThreshold = (uThreshold * 3 + n) / 4; // Filter/soften motion
  // Lower eyelid doesn't track the same way, but seems to be pulled upward
  // by tension from the upper lid.
  lThreshold = 254 - uThreshold;
#else // No tracking -- eyelids full open unless blink modifies them
  uThreshold = lThreshold = 0;
#endif

  // The upper/lower thresholds are then scaled relative to the current
  // blink position so that blinks work together with pupil tracking.
  if(g_eye[eyeIndex].blink.state) { // Eye currently blinking?
    uint32_t s = (t - g_eye[eyeIndex].blink.startTime);
    if(s >= g_eye[eyeIndex].blink.duration) s = 255;   // At or past blink end
    else s = 255 * s / g_eye[eyeIndex].blink.duration; // Mid-blink
    s          = (g_eye[eyeIndex].blink.state == DEBLINK) ? 1 + s : 256 - s;
    n          = (uThreshold * s + 254 * (257 - s)) / 256;
    lThreshold = (lThreshold * s + 254 * (257 - s)) / 256;
  } else {
    n          = uThreshold;
  }

  // Pass all the derived values to the eye-rendering function:
  drawEye(eyeIndex, iScale, eyeX, eyeY, n, lThreshold);
}

// AUTONOMOUS IRIS SCALING (if no photocell or dial) -----------------------

#if !defined(LIGHT_PIN) || (LIGHT_PIN < 0)

// Autonomous iris motion uses a fractal behavior to similate both the major
// reaction of the eye plus the continuous smaller adjustments that occur.

uint16_t oldIris = (IRIS_MIN + IRIS_MAX) / 2, newIris;

void split( // Subdivides motion path into two sub-paths w/randimization
  int16_t  startValue, // Iris scale value (IRIS_MIN to IRIS_MAX) at start
  int16_t  endValue,   // Iris scale value at end
  uint32_t startTime,  // micros() at start
  int32_t  duration,   // Start-to-end time, in microseconds
  int16_t  range)      // Allowable scale value variance when subdividing
{
  if(range >= 8) {     // Limit subdvision count, because recursion
    range    /= 2;     // Split range & time in half for subdivision,
    duration /= 2;     // then pick random center point within range:
    int16_t  midValue = (startValue + endValue - range) / 2 + rand() % range;
    uint32_t midTime  = startTime + duration;
    split(startValue, midValue, startTime, duration, range); // First half
    split(midValue  , endValue, midTime  , duration, range); // Second half
  } else {             // No more subdivisons, do iris motion...
    int32_t dt;        // Time (micros) since start of motion
    int16_t v;         // Interim value
    while((dt = (g_timer.read_us() - startTime)) < duration) {
      v = startValue + (((endValue - startValue) * dt) / duration);
      if(v < IRIS_MIN)      v = IRIS_MIN; // Clip just in case
      else if(v > IRIS_MAX) v = IRIS_MAX;
      frame(v);        // Draw frame w/interim iris scale value
    }
  }
}

#endif // !LIGHT_PIN

void loop() {

#if defined(LIGHT_PIN) && (LIGHT_PIN >= 0) // Interactive iris

  int16_t v = analogRead(LIGHT_PIN);       // Raw dial/photocell reading
#ifdef LIGHT_PIN_FLIP
  v = 1023 - v;                            // Reverse reading from sensor
#endif
  if(v < LIGHT_MIN)      v = LIGHT_MIN;  // Clamp light sensor range
  else if(v > LIGHT_MAX) v = LIGHT_MAX;
  v -= LIGHT_MIN;  // 0 to (LIGHT_MAX - LIGHT_MIN)
#ifdef LIGHT_CURVE  // Apply gamma curve to sensor input?
  v = (int16_t)(pow((double)v / (double)(LIGHT_MAX - LIGHT_MIN),
    LIGHT_CURVE) * (double)(LIGHT_MAX - LIGHT_MIN));
#endif
  // And scale to iris range (IRIS_MAX is size at LIGHT_MIN)
  v = map(v, 0, (LIGHT_MAX - LIGHT_MIN), IRIS_MAX, IRIS_MIN);
#ifdef IRIS_SMOOTH // Filter input (gradual motion)
  static int16_t irisValue = (IRIS_MIN + IRIS_MAX) / 2;
  irisValue = ((irisValue * 15) + v) / 16;
  frame(irisValue);
#else // Unfiltered (immediate motion)
  frame(v);
#endif // IRIS_SMOOTH

#else  // Autonomous iris scaling -- invoke recursive function

  newIris = rand()%(IRIS_MAX-IRIS_MIN)+IRIS_MIN;
  split(oldIris, newIris, g_timer.read_us(), 10000000L, IRIS_MAX - IRIS_MIN);
  oldIris = newIris;

#endif // LIGHT_PIN
}


int main()
{
  setup();
  while(true) {
    loop();
  }
  return 0;
}
