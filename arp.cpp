#include <Arduino.h>
#include "DacESP32.h"

float* chromScale;
String* chromScaleS;
int* indxs;

int* keySigIndxs(int rootIndx, bool minor);
float* genChromScale(/*tuning scheme*/);
String* chromString();

int dacpin=25;
int mywrite;
DacESP32 dac1(GPIO_NUM_25);

void setup() {
  Serial.begin(9600);
  delay(5000);
  //pinMode(34, INPUT);
  //pinMode(35, INPUT);
  int keySig = 10;//analogRead(34);
  //keySig = map(keySig,0,1023,0,12)%12;  //fix that irl
  bool minor = 1;//digitalRead(35);

  chromScale = genChromScale(/*12TET*/);
  chromScaleS = chromString();
  Serial.print("\n\nKey of ");Serial.println(chromScaleS[keySig]);if(minor){Serial.println("(Minor)\n");}

  indxs = keySigIndxs(keySig,minor); 

  for(int i=0;i<7;i++){
    Serial.print(chromScaleS[indxs[i]]);Serial.print("\t");
    Serial.print(chromScale[indxs[i]]);Serial.print("\t\t");
    Serial.println(indxs[i]);
  }
  /*for(int i=0;i<12;i++){
    Serial.print(chromScaleS[i]);Serial.print("\t");
    Serial.println(chromScale[i]);
  }*/
}

void loop() {
   //int keySlider = analogRead(D34);

  /*testing: just play the 7 notes of selected scale
  for(int i=0;i<7;i++){
    //mywrite = map(chromScale[indxs[i]],261,495,0,255);
    //dacWrite(dacpin,mywrite);
    //delay(3000);
    dac1.outputCW(chromScale[indxs[i]]);
    Serial.println(chromScale[indxs[i]]); 
    delay(500);
  } */

}


int* keySigIndxs(int rootIndx, bool minor){
  //returns 7 incremental indexes into chromatic scale
  //we use stepping data from majmin and can just loop within chromscale
  static int indxs[7];
  static bool majmin[14] = {1,1,0,1,1,1,0,  1,0,1,1,0,1,1}; 
  int i,n, m=minor*7;
  for(i=0,n=rootIndx;n<12+rootIndx;i++,n++){
    indxs[i] = n%12; //worst case n=24. modulo overkill?
    if(majmin[i+m]){n++;}
  }
  return indxs;
}

float* genChromScale(){
  //optional todo: params/conds for tunings other than 12 tet
  //(just didnt want to hard code chromScale)
  static float scale[12];
  for(int i=0; i<12; i++){
    scale[i] = 440.0 * pow(2,((i-9)/12.0)); //start at A440
    //scale[0..11] = (261.63-493.88)hz i.e. octave C4-B4
  }
  return scale;
}

String* chromString(){
  //just for display, wasting space to be pretty
  static String chrom[12] =
  {"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"};
  return chrom;
}


