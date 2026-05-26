#define HARDWARE_TYPE MEGA2560

#if HARDWARE_TYPE == MEGA2560

#define CHARGE1_PIN 6        //Drive to high-voltage charge MOSFET (PWM)
#define DISCHARGE1_PIN 5     //Drive to high-voltage discharge MOSFET (PWM)
#define FIRE1_PIN 14         //Drive to 'apply high voltage' MOSFET 

#define CHARGE2_PIN 4        //Drive to high-voltage charge MOSFET (PWM)
#define DISCHARGE2_PIN 3     //Drive to high-voltage discharge MOSFET (PWM)
#define FIRE2_PIN 15         //Drive to 'apply high voltage' MOSFET

#define VA1_PIN A8           //High voltage sense 1
#define VA2_PIN A9           //High voltage sense 2

#define IA1_XHI_PIN A4       //Small anode-current sense (large sense resistance with a gain of 4) 
#define IA1_HI_PIN A3        //Small anode-current sense (large sense resistance) 
#define IA1_LO_PIN A2        //Large anode-current sense (small sense resistance)

#define IA2_XHI_PIN A7       //Small anode-current sense (large sense resistance with a gain of 4) 
#define IA2_HI_PIN A5        //Small anode-current sense (large sense resistance) 
#define IA2_LO_PIN A6        //Large anode-current sense (small sense resistance)

#elif HARDWARE_TYPE == NANO

#define CHARGE1_PIN 6        //Drive to high-voltage charge MOSFET (PWM)
#define DISCHARGE1_PIN 5     //Drive to high-voltage discharge MOSFET (PWM)
#define FIRE1_PIN 14         //Drive to 'apply high voltage' MOSFET 

#define CHARGE2_PIN 4        //Drive to high-voltage charge MOSFET (PWM)
#define DISCHARGE2_PIN 3     //Drive to high-voltage discharge MOSFET (PWM)
#define FIRE2_PIN 15         //Drive to 'apply high voltage' MOSFET

#define VA1_PIN A0           //High voltage sense 1
#define VA2_PIN A1           //High voltage sense 2

#define IA1_XHI_PIN A2       //Small anode-current sense (large sense resistance with a gain of 4) 
#define IA1_HI_PIN A3        //Small anode-current sense (large sense resistance) 
#define IA1_LO_PIN A4        //Large anode-current sense (small sense resistance)

#define IA2_XHI_PIN A5       //Small anode-current sense (large sense resistance with a gain of 4) 
#define IA2_HI_PIN A6        //Small anode-current sense (large sense resistance) 
#define IA2_LO_PIN A7        //Large anode-current sense (small sense resistance)

#else
#error "Unknown HARDWARE_TYPE. Define HARDWARE_TYPE as MEGA2560 or NANO."
#endif