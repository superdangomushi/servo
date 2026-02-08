#define SECOND 1000000.0
void accel(float dvR,float dvL,float vR,float vL,int *state){
  float n = 0.0;
  float div = 30.0;
  float vR_ = v0R,vL_ = v0L;
  if(dvR == -1){v0R = vR;}
  if(dvL == -1){v0L = vL;}
  if(vL != v0L){dvL = fabs(dvL) * (fabs(vL - v0L) / (vL - v0L));};
  if(vR != v0R){dvR = fabs(dvR) * (fabs(vR - v0R) / (vR - v0R));};
  if(vR != v0R || vL != v0L){
    while((haveSameSign(vL - vL_,vL - v0L) || haveSameSign(vR - vR_,vR - v0R)) && (vL!=v0L) && (vR!=v0R)){
      if(haveSameSign(vL - vL_,vL - v0L)){vL_ += dvL / div;}
      if(haveSameSign(vR - vR_,vR - v0R)){vR_ += dvR / div;}
      move(vR_,vL_,SECOND / div,state);
    };
    v0R = vR;v0L = vL;
  }else{
    move(v0R,v0L,10000,state);
  }
}

bool haveSameSign(float a, float b) {
  return ((a >= 0 && b >= 0) || (a < 0 && b < 0));
}

void move(float vR,float vL,long time,int *state){
  long delay_timeR = 0,delay_timeL = 0;
  Serial.println(String(vR) + "," + String(vL));
  if(vR != 0){delay_timeR = sec / fabs(vR * 800.0 / 6.28);}
  if(vL != 0){delay_timeL = sec / fabs(vL * 800.0 / 6.28);}
  long begin = micros();
  long dt = 0;
  int stepR = 0,stepL = 0,Rn = 0,Ln = 0;
  int nextR,nextL;
  while (dt < time){
    stepR = dt - Rn * delay_timeR;
    stepL = dt - Ln * delay_timeL;
    if(stepR >= 0 && delay_timeR != 0){
      step(pinA,vR / fabs(vR), 0,state[0]);
      Rn++;
    }
    if(stepL >= 0 && delay_timeL != 0){
      step(pinB,vL / fabs(vL), 1,state[0]);
      Ln++;
    }
    dt = micros() - begin;
  }
}

void step(int *pin,int toward,int channel,int *state){
  *state = *state % 8;
  for(int k = 0;k < 4;k++){
    digitalWrite(pin[k], list[*state][k]);
  }
  *state += toward;
}