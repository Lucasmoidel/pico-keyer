#ifndef PWM_PICO_KEYER
#define PWM_PICO_KEYER

void initPWM(uint32_t x, int pin);
void setPWMEnable(bool x);
void setFreq(uint32_t x);

#endif