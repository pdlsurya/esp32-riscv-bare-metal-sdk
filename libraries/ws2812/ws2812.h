#pragma once

#include <stdint.h>

//Use RMT peripheral
#define WS2812_USE_RMT 1

//Use SPI peripheral
#define WS2812_USE_SPI 0

// Primary Colors
#define WS2812_COLOR_RED {.red = 0xFF, .blue = 0x00, .green = 0x00}
#define WS2812_COLOR_GREEN {.red = 0x00, .blue = 0x00, .green = 0xFF}
#define WS2812_COLOR_BLUE {.red = 0x00, .blue = 0xFF, .green = 0x00}
#define WS2812_COLOR_WHITE {.red = 0xFF, .blue = 0xFF, .green = 0xFF}
#define WS2812_COLOR_BLACK {.red = 0x00, .blue = 0x00, .green = 0x00}

// Secondary Colors
#define WS2812_COLOR_YELLOW {.red = 0xFF, .blue = 0x00, .green = 0xFF}
#define WS2812_COLOR_CYAN {.red = 0x00, .blue = 0xFF, .green = 0xFF}
#define WS2812_COLOR_MAGENTA {.red = 0xFF, .blue = 0xFF, .green = 0x00}

// Warm Colors
#define WS2812_COLOR_ORANGE {.red = 0xFF, .blue = 0x00, .green = 0xA5}
#define WS2812_COLOR_RED_ORANGE {.red = 0xFF, .blue = 0x00, .green = 0x45}
#define WS2812_COLOR_YELLOW_ORANGE {.red = 0xFF, .blue = 0x00, .green = 0x80}
#define WS2812_COLOR_GOLD {.red = 0xFF, .blue = 0x00, .green = 0xD4}
#define WS2812_COLOR_BROWN {.red = 0xA5, .blue = 0x2A, .green = 0x2A}
#define WS2812_COLOR_MAROON {.red = 0x80, .blue = 0x00, .green = 0x00}
#define WS2812_COLOR_TAN {.red = 0xD2, .blue = 0xB4, .green = 0x8C}
#define WS2812_COLOR_SANDY_BROWN {.red = 0xF4, .blue = 0xA4, .green = 0x60}
#define WS2812_COLOR_PERU {.red = 0xCD, .blue = 0x85, .green = 0x3F}

// Cool Colors
#define WS2812_COLOR_LIME {.red = 0x00, .blue = 0x00, .green = 0xFF}
#define WS2812_COLOR_TEAL {.red = 0x00, .blue = 0x80, .green = 0x80}
#define WS2812_COLOR_AQUA {.red = 0x00, .blue = 0xFF, .green = 0xFF}
#define WS2812_COLOR_OLIVE {.red = 0x80, .blue = 0x00, .green = 0x80}
#define WS2812_COLOR_NAVY {.red = 0x00, .blue = 0x80, .green = 0x00}
#define WS2812_COLOR_TURQUOISE {.red = 0x40, .blue = 0xE0, .green = 0xD0}
#define WS2812_COLOR_MEDIUM_AQUAMARINE {.red = 0x66, .blue = 0xCD, .green = 0xAA}
#define WS2812_COLOR_SKY_BLUE {.red = 0x87, .blue = 0xCE, .green = 0xEB}
#define WS2812_COLOR_LIGHT_BLUE {.red = 0xAD, .blue = 0xD8, .green = 0xE6}
#define WS2812_COLOR_DEEP_SKY_BLUE {.red = 0x00, .blue = 0xBF, .green = 0xFF}

// Purple & Pink Shades
#define WS2812_COLOR_PINK {.red = 0xFF, .blue = 0xC0, .green = 0xCB}
#define WS2812_COLOR_HOT_PINK {.red = 0xFF, .blue = 0x69, .green = 0xB4}
#define WS2812_COLOR_DEEP_PINK {.red = 0xFF, .blue = 0x14, .green = 0x93}
#define WS2812_COLOR_PURPLE {.red = 0x80, .blue = 0xFF, .green = 0x00}
#define WS2812_COLOR_VIOLET {.red = 0xEE, .blue = 0x82, .green = 0xEE}
#define WS2812_COLOR_INDIGO {.red = 0x4B, .blue = 0x82, .green = 0x00}
#define WS2812_COLOR_MEDIUM_ORCHID {.red = 0xBA, .blue = 0x55, .green = 0xD3}
#define WS2812_COLOR_DARK_VIOLET {.red = 0x94, .blue = 0x00, .green = 0xD3}
#define WS2812_COLOR_DARK_ORCHID {.red = 0x99, .blue = 0x32, .green = 0xCC}
#define WS2812_COLOR_ORCHID {.red = 0xDA, .blue = 0x70, .green = 0xD6}
#define WS2812_COLOR_PALE_VIOLET_RED {.red = 0xDB, .blue = 0x70, .green = 0x93}

// Neutral & Grayscale Colors
#define WS2812_COLOR_SLATE_GRAY {.red = 0x70, .blue = 0x80, .green = 0x90}
#define WS2812_COLOR_SILVER {.red = 0xC0, .blue = 0xC0, .green = 0xC0}
#define WS2812_COLOR_GRAY {.red = 0x80, .blue = 0x80, .green = 0x80}
#define WS2812_COLOR_BEIGE {.red = 0xF5, .blue = 0xF5, .green = 0xDC}
#define WS2812_COLOR_MISTY_ROSE {.red = 0xFF, .blue = 0xE4, .green = 0xE1}

// Additional Shades
#define WS2812_COLOR_CRIMSON {.red = 0xDC, .blue = 0x14, .green = 0x3C}
#define WS2812_COLOR_TOMATO {.red = 0xFF, .blue = 0x45, .green = 0x00}
#define WS2812_COLOR_SPRING_GREEN {.red = 0x00, .blue = 0xFF, .green = 0x7F}
#define WS2812_COLOR_AQUAMARINE {.red = 0x7F, .blue = 0xFF, .green = 0xD4}
#define WS2812_COLOR_LEMON_CHIFFON {.red = 0xFF, .blue = 0xFA, .green = 0xCD}
#define WS2812_COLOR_GOLDENROD {.red = 0xDA, .blue = 0xA5, .green = 0x20}
#define WS2812_COLOR_SADDLE_BROWN {.red = 0x8B, .blue = 0x45, .green = 0x13}
#define WS2812_COLOR_SUN_YELLOW {.red = 0xFF, .blue = 0xD7, .green = 0x00}
#define WS2812_COLOR_MEDIUM_TURQUOISE {.red = 0x48, .blue = 0xD1, .green = 0xCC}
#define WS2812_COLOR_PEACH {.red = 0xFF, .blue = 0xDA, .green = 0xB9}
#define WS2812_COLOR_CORAL {.red = 0xFF, .blue = 0x7F, .green = 0x50}
#define WS2812_COLOR_SALMON {.red = 0xFA, .blue = 0x80, .green = 0x72}
#define WS2812_COLOR_LAVENDER {.red = 0xE6, .blue = 0xE6, .green = 0xFA}
#define WS2812_COLOR_WHEAT {.red = 0xF5, .blue = 0xDE, .green = 0xB3}

typedef union
{
    struct
    {
        uint32_t blue : 8;
        uint32_t red : 8;
        uint32_t green : 8;
        uint32_t reserverd : 8;
    };
    uint32_t val;
} ws2812_color_t;

typedef union
{
    struct
    {
        uint32_t duration0 : 15;
        uint32_t level0 : 1;
        uint32_t duration1 : 15;
        uint32_t level1 : 1;
    };
    uint32_t val;
} ws2812_pulse_t;

void ws2812_init();

void ws2812_write(ws2812_color_t color);