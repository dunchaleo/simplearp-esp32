#include <driver/dac.h>

#define POT_TOL 50
#define POT_MAX 4095
#define PRESCALER 80 //timer speed = (cpu clock hz) / prescaler (e.g. 80mhz/80 = timer speed 1mhz )
#define CLOCK 80000000

String chromScaleS[12] = {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};
String solfa[7] = {"Do","Re","Mi","Fa","Sol","La","Ti"};

int scales[2][22]; //21 tones: 3 octaves of scale in selected key sig. tones not stored as frequencies but interrupt intervals--see updateKey()
int playing; //idx into scales of currently playing note
int lowest; //corresponds to button 0, idx of lowest playable tone on keyboard via tonic shift setting

byte waveSamples[100] = {
128, 136, 143, 151, 159, 167, 174, 182, 189, 196, 202, 209, 215, 220, 226, 231, 235, 239, 243, 246,
249, 251, 253, 254, 255, 255, 255, 254, 253, 251, 249, 246, 243, 239, 235, 231, 226, 220, 215, 209,
202, 196, 189, 182, 174, 167, 159, 151, 143, 136, 128, 119, 112, 104, 96, 88, 81, 73, 66, 59,
53, 46, 40, 35, 29, 24, 20, 16, 12, 9, 6, 4, 2, 1, 0, 0, 0, 1, 2, 4,
6, 9, 12, 16, 20, 24, 29, 35, 40, 46, 53, 59, 66, 73, 81, 88, 96, 104, 112, 119};

hw_timer_t* t0;
int totalInterrupts, totalTicks; //totalInterrupts: total samples to play current tone for selected duration
volatile int interrupts;
void IRAM_ATTR ISR0(){
  dac_output_voltage(DAC_CHANNEL_1,waveSamples[interrupts % 100]);
  interrupts++; 
}

/* unused, 
void interruptData(int* interruptInterval,int* totalInterrupts, int tHz, float freq, int ms){
  //tHz = timer clock speed; freq = output sine frequency on DAC; ms = duration
  //100 sample points in sine lookup table

  //freq periods / 1000ms
  //tHz ticks /1000ms
  // -> (tHz/freq) ticks/period /s
  //100 samples/period
  // -> (tHz/freq)/100 samples /s
  *interruptInterval = (int) tHz/(freq*100);
  *totalInterrupts = (int) ms*freq/10;  // = [total periods] * samples = ([periods in 1000ms] * (ms/1000)) * samples = freq* ms/1000 *samples 
                                        //or, [total ticks]/[interrupt interval] = tHz*ms/1000 / [interrupt interval]
  return;
}*/



void setup() {
  Serial.begin(115200);

  pinMode(34, INPUT);
  pinMode(23, INPUT_PULLUP);
  pinMode(35,INPUT);
  pinMode(32,INPUT);
  pinMode(33, INPUT_PULLUP);

  pinMode(15, INPUT_PULLDOWN);pinMode(2, INPUT_PULLDOWN);pinMode(4, INPUT_PULLDOWN);pinMode(16, INPUT_PULLDOWN);
  pinMode(17, INPUT_PULLDOWN);pinMode(5, INPUT_PULLDOWN);pinMode(18, INPUT_PULLDOWN);pinMode(19, INPUT_PULLDOWN); 

  t0 = timerBegin(0, PRESCALER, true);
  timerAttachInterrupt(t0, &ISR0, true);
  timerAlarmWrite(t0, CLOCK/PRESCALER, true);
  timerAlarmEnable(t0);

  dac_output_enable(DAC_CHANNEL_1);

  //set initial values, loop() only sets these on input pin state change detection {
  updateKey(map(analogRead(34),0,POT_MAX,0,11),digitalRead(23),CLOCK/PRESCALER,1);
  lowest = 7-map(analogRead(35),0,POT_MAX,0,7);
  totalTicks = (CLOCK/(PRESCALER*1000))*map(analogRead(32),0,POT_MAX,0,1500);
  // }

  scales[0][21]=scales[1][21] = CLOCK/PRESCALER; //better to change scales back to len 21 and stuff this in [1][0]
  
}

void loop(){

  //these statics are unmodified from actual pin readings, analog inputs mapped to usable values on state change
  static int key[2] = {0,0};  //tonic note of key 0..11
  static bool minor[2] = {0,0}; //mode of key
  static int tonicShift[2] = {0,0}; //tonic note shifted up this many buttons 0..7
  static int ms[2] = {0,0}; //duration in ms of each note in arp 
  static int order[2] = {0,0}; // 6 ways to arrange 3rd,5th,7th for tetrads starting w 1st and 2 for triads
  static bool osc = 0; static bool desc[2] = {0,0}; //...with asc, desc, and oscillating options for these notes


  static bool btns[8] = {0,0,0,0,0,0,0,0}; static byte keyboard;
  //look into hardware multiplexer? 1 analog read instead of 8 digital reads
  btns[0]=digitalRead(15);btns[1]=digitalRead(2);/*btns[2]=digitalRead(4);btns[3]=digitalRead(16);
  btns[4]=digitalRead(17);btns[5]=digitalRead(5);btns[6]=digitalRead(18);btns[7]=digitalRead(19);*/

  static short int arpSucc;

  key[0] = analogRead(34);
  minor[0] = digitalRead(23); 
  tonicShift[0] = analogRead(35);
  ms[0] = analogRead(32); 
  desc[0] = digitalRead(33);

  if(minor[0] != minor[1] || abs(key[0]-key[1]) > POT_TOL)  
    updateKey(map(key[0],0,POT_MAX,0,11),minor[0],CLOCK/PRESCALER,1);

  if(abs(tonicShift[0]-tonicShift[1]) > POT_TOL)
    lowest = 7-map(tonicShift[0],0,POT_MAX,0,7);

  //total ticks = [timer speed hz]*ms/1000 
  //totalTicks = CLOCK*map(ms[0],0,POT_MAX,0,1500)/(1000*PRESCALER); // clock*ms too big
  if(abs(ms[0]-ms[1]) > POT_TOL)
    totalTicks = (CLOCK/(PRESCALER*1000))*map(ms[0],0,POT_MAX,0,1500);

  if(desc[0] != desc[1])
    arpSucc = 0; //without this, ugly behavior if desc switched while playing

  for(byte i=0;i<8;i++){
    keyboard <<= 1; 
    if(btns[i]){ keyboard |= 1;}
  }


  //ticks stay refreshed to 0 if no buttons or multiple buttons pressed
  switch(keyboard){
    case 128:
      playing = lowest+desc[0]*6+arpSucc; 
      break;
    case 64:
      playing = lowest+1+desc[0]*6+arpSucc;
      break;
    default:
      playing = 21;
      interrupts = 0; //todo test removing this for understanding
      arpSucc = 0;
      timerWrite(t0,0);
  }

  totalInterrupts = totalTicks/scales[0][playing];

  if(interrupts >= totalInterrupts){ //can be gt if total is lowered between interrupts 
    interrupts = 0;
    if(desc[0]){arpSucc = (arpSucc-2)%(-8);}else{arpSucc = (arpSucc+2)%8;} 
 
  }

  timerAlarmWrite(t0, scales[0][playing], true);

  Serial.print(interrupts);Serial.print("\t\t");Serial.println(totalInterrupts);
  //Serial.println(keyboard);

  key[1] = key[0];
  minor[1] = minor[0];
  tonicShift[1] = tonicShift[0];
  ms[1] = ms[0];
  desc[1] = desc[0];
}

void updateKey(int key, bool minor, int tHz, bool print){
  if(print){Serial.print("Key of ");Serial.print(chromScaleS[key]);if(minor){Serial.print(" minor");}Serial.print("\n");}

  static bool Mm[2][7] = {1,1,0,1,1,1,0, 1,0,1,1,0,1,1};

  int i,s;
  float freq;
  for(i=0,s=0; i<21; i++){
    freq = /* tonicPitch * pow(2,((i+s-12)/12.0)); */ 440.00 * pow(2,((i+s+key-21)/12.0)); //freq in hz
    scales[0][i] = (int) tHz/(freq*100); //interrupt interval: timer ticks between wave samples 
    if(Mm[minor][i%7]){s++;}
    
    if(print){Serial.print(solfa[i%7]);Serial.print("\t");Serial.print(freq);Serial.print(" Hz\t");Serial.println(scales[0][i]);}
  }
  for(i=0,s=0; i<21; i++){
    freq = 440.00 * pow(2,((i+s+key-21)/12.0));
    scales[1][i] = (int) tHz/(freq*100);
    if(Mm[!minor][i%7]){s++;}
  }

  return; 
}