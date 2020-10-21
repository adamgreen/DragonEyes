// Pin selections here are based on the original Adafruit Learning System
// guide for the Teensy 3.x project.  Some of these pin numbers don't even
// exist on the smaller SAMD M0 & M4 boards, so you may need to make other
// selections:

// GRAPHICS SETTINGS (appearance of eye) -----------------------------------

// If using a SINGLE EYE, you might want this next line enabled, which
// uses a simpler "football-shaped" eye that's left/right symmetrical.
// Default shape includes the caruncle, creating distinct left/right eyes.
// Hallowing, with one eye, does this by default
//#define SYMMETRICAL_EYELID

// Enable ONE of these #includes -- HUGE graphics tables for various eyes:
//#include "graphics/defaultEye.h"      // Standard human-ish hazel eye -OR-
#include "graphics/dragonEye.h"     // Slit pupil fiery dragon/demon eye -OR-
//#include "graphics/noScleraEye.h"   // Large iris, no sclera -OR-
//#include "graphics/goatEye.h"       // Horizontal pupil goat/Krampus eye -OR-
//#include "graphics/newtEye.h"       // Eye of newt -OR-
//#include "graphics/terminatorEye.h" // Git to da choppah!
//#include "graphics/catEye.h"        // Cartoonish cat (flat "2D" colors)
//#include "graphics/owlEye.h"        // Minerva the owl (DISABLE TRACKING)
//#include "graphics/naugaEye.h"      // Nauga googly eye (DISABLE TRACKING)
//#include "graphics/doeEye.h"        // Cartoon deer eye (DISABLE TRACKING)

// Optional: enable this line for startup logo (screen test/orient):
#include "graphics/logo.h"        // Otherwise your choice, if it fits

// Analog pin to use for seeding the random number generator.
#define ANALOG_PIN    p15

// Pins connected to the OLED screen.
#define OLED_MOSI_PIN     p5
#define OLED_SCK_PIN      p7
#define OLED_DC_PIN       p6
#define OLED_RST_PIN      p8
#define OLED_LEFT_CS_PIN  p9
#define OLED_RIGHT_CS_PIN p10

// Dimensions of the display.
#define OLED_WIDTH      128
#define OLED_HEIGHT     128


// EYE LIST ----------------------------------------------------------------

// This table contains ONE LINE PER EYE.  The table MUST be present with
// this name and contain ONE OR MORE lines.  Each line contains THREE items:
// a pin number for the corresponding TFT/OLED display's SELECT line, a pin
// pin number for that eye's "wink" button (or -1 if not used), and a screen
// rotation value (0-3) for that eye.
typedef struct {
  PinName select;       // pin numbers for each eye's screen select line
  uint8_t rotation;     // also display rotation.
} eyeInfo_t;

eyeInfo_t eyeInfo[] = {
  {  OLED_LEFT_CS_PIN, 0  }, // LEFT EYE display-select and no rotation
  {  OLED_RIGHT_CS_PIN, 0 }, // RIGHT EYE display-select and no rotation
};

// INPUT SETTINGS (for controlling eye motion) -----------------------------

// LIGHT_PIN speficies an analog input pin for a photocell to make pupils
// react to light (or potentiometer for manual control).  If set to -1 or
// if not defined, the pupils will change on their own.

#define TRACKING            // If defined, eyelid tracks pupil
#define AUTOBLINK           // If defined, eyes also blink autonomously
#ifdef UNDONE
  #define LIGHT_PIN      A1 // Hallowing light sensor pin
  #define LIGHT_CURVE  0.33 // Light sensor adjustment curve
  #define LIGHT_MIN      30 // Minimum useful reading from light sensor
  #define LIGHT_MAX     980 // Maximum useful reading from sensor
  #define IRIS_SMOOTH       // If enabled, filter input from LIGHT_PIN
#endif // UNDONE

#if !defined(IRIS_MIN)      // Each eye might have its own MIN/MAX
  #define IRIS_MIN      120 // Iris size (0-1023) in brightest light
#endif
#if !defined(IRIS_MAX)
  #define IRIS_MAX      720 // Iris size (0-1023) in darkest light
#endif
