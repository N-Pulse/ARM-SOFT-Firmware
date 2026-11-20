#include "status_leds_codec.h"
#include "status_leds_config.h"
void StatusLeds_Encode(const status_led_color_t colors[STATUS_LED_COUNT],
                       uint8_t out[STATUS_LEDS_WIRE_BYTES])
{
    /* Inverted start/stop are H/L. Each LED bit occupies five 250 ns cells:
     * 0 = HLLLL (0.25/1.00 us), 1 = HHHHL (1.00/0.25 us).
     * Both fit Dialight's specified 0.3/0.9 us +/-0.15 us bounds.
     * Packed UART data is transmitted LSB first, LED data remains MSB first. */
    static const uint8_t pairs[4]={0xef,0x0f,0xe8,0x08};
    unsigned n=0;
    for(unsigned i=0;i<STATUS_LED_COUNT;++i) {
        uint8_t grb[3]={colors[i]==STATUS_LED_GREEN ? STATUS_LEDS_BRIGHTNESS : 0,
                        colors[i]==STATUS_LED_RED ? STATUS_LEDS_BRIGHTNESS : 0,0};
        for(unsigned c=0;c<3;++c)
            for(int shift=6;shift>=0;shift-=2) out[n++]=pairs[(grb[c]>>shift)&3U];
    }
}
