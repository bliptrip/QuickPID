/**********************************************************************************
   QuickPID Library for Arduino - Version 3.1.1
   by dlloydev https://github.com/Dlloydev/QuickPID
   Based on the Arduino PID_v1 Library. Licensed under the MIT License.
 **********************************************************************************/

#include "QuickPID.h"

/* Constructor ********************************************************************
   The parameters specified here are those for for which we can't set up
   reliable defaults, so we need to have the user set them.
 **********************************************************************************/
QuickPID::QuickPID(float* Input, float* Output, float* Setpoint,
                   float Kp = 0, float Ki = 0, float Kd = 0,
                   pMode pMode = pMode::pOnError,
                   dMode dMode = dMode::dOnMeas,
                   iAwMode iAwMode = iAwMode::iAwCondition,
                   Action Action = Action::direct,
                   tGetTimeMicros getMicros = NULL) {

  myOutput = Output;
  myInput = Input;
  mySetpoint = Setpoint;
  mode = Control::manual;
  _getMicros = getMicros;

  QuickPID::SetOutputLimits(0, 255);  // same default as Arduino PWM limit
  sampleTimeUs = 100000;              // 0.1 sec default
  QuickPID::SetControllerDirection(Action);
  QuickPID::SetTunings(Kp, Ki, Kd, pMode, dMode, iAwMode);

  if( _getMicros != NULL ) {
    lastTime = _getMicros() - sampleTimeUs;
  }
}

/* Constructor *********************************************************************
   To allow using pOnError, dOnMeas and iAwCondition without explicitly saying so.
 **********************************************************************************/
QuickPID::QuickPID(float* Input, float* Output, float* Setpoint,
                   float Kp, float Ki, float Kd, Action Action)
  : QuickPID::QuickPID(Input, Output, Setpoint, Kp, Ki, Kd,
                       pmode = pMode::pOnError,
                       dmode = dMode::dOnMeas,
                       iawmode = iAwMode::iAwCondition,
                       action = Action) {
}

/* Constructor *********************************************************************
   Simplified constructor which uses defaults for remaining parameters.
 **********************************************************************************/
QuickPID::QuickPID(float* Input, float* Output, float* Setpoint)
  : QuickPID::QuickPID(Input, Output, Setpoint, dispKp, dispKi, dispKd,
                       pmode = pMode::pOnError,
                       dmode = dMode::dOnMeas,
                       iawmode = iAwMode::iAwCondition,
                       action = Action::direct) {
}

/* Compute() ***********************************************************************
   This function should be called every time "void loop()" executes. The function
   will decide whether a new PID Output needs to be computed. Returns true
   when the output is computed, false when nothing has been done.
 **********************************************************************************/
bool QuickPID::Compute() {
  uint32_t now;
  uint32_t timeChange = 0;
  if (mode == Control::manual) return false;
  if ((mode == Control::automatic) && (_getMicros != NULL) ) {
    now = _getMicros();
    timeChange = (now - lastTime);
  }
  if (mode == Control::timer || timeChange >= sampleTimeUs) {

    float input = *myInput;
    float dInput = input - lastInput;
    if (action == Action::reverse) dInput = -dInput;

    error = *mySetpoint - input;
    if (action == Action::reverse) error = -error;
    float dError = error - lastError;

    float peTerm = kp * error;
    float pmTerm = kp * dInput;
    if (pmode == pMode::pOnError) pmTerm = 0;
    else if (pmode == pMode::pOnMeas) peTerm = 0;
    else { //pOnErrorMeas
      peTerm *= 0.5f;
      pmTerm *= 0.5f;
    }
    pTerm = peTerm - pmTerm; // used by GetDterm()
    iTerm =  ki  * error;
    if (dmode == dMode::dOnError) dTerm = kd * dError;
    else dTerm = -kd * dInput; // dOnMeas

    //condition anti-windup (default)
    if (iawmode == iAwMode::iAwCondition) {
      bool aw = false;
      float iTermOut = (peTerm - pmTerm) + ki * (iTerm + error);
      if (iTermOut > outMax && dError > 0) aw = true;
      else if (iTermOut < outMin && dError < 0) aw = true;
      if (aw && ki) iTerm = CONSTRAIN(iTermOut, -outMax, outMax);
    }

    // by default, compute output as per PID_v1
    outputSum += iTerm;                                                 // include integral amount
    if (iawmode == iAwMode::iAwOff) outputSum -= pmTerm;                // include pmTerm (no anti-windup)
    else outputSum = CONSTRAIN(outputSum - pmTerm, outMin, outMax);     // include pmTerm and clamp
    *myOutput = CONSTRAIN(outputSum + peTerm + dTerm, outMin, outMax);  // include dTerm, clamp and drive output

    lastError = error;
    lastInput = input;
    lastTime = now;
    return true;
  }
  else return false;
}

/* SetTunings(....)************************************************************
  This function allows the controller's dynamic performance to be adjusted.
  it's called automatically from the constructor, but tunings can also
  be adjusted on the fly during normal operation.
******************************************************************************/
void QuickPID::SetTunings(float Kp, float Ki, float Kd,
                          pMode pMode = pMode::pOnError,
                          dMode dMode = dMode::dOnMeas,
                          iAwMode iAwMode = iAwMode::iAwCondition) {

  if (Kp < 0 || Ki < 0 || Kd < 0) return;
  pmode = pMode; dmode = dMode; iawmode = iAwMode;
  dispKp = Kp; dispKi = Ki; dispKd = Kd;
  float SampleTimeSec = (float)sampleTimeUs / 1000000;
  kp = Kp;
  ki = Ki * SampleTimeSec;
  kd = Kd / SampleTimeSec;
}

/* SetTunings(...)************************************************************
  Set Tunings using the last remembered pMode, dMode and iAwMode settings.
******************************************************************************/
void QuickPID::SetTunings(float Kp, float Ki, float Kd) {
  SetTunings(Kp, Ki, Kd, pmode, dmode, iawmode);
}

/* SetSampleTime(.)***********************************************************
  Sets the period, in microseconds, at which the calculation is performed.
******************************************************************************/
void QuickPID::SetSampleTimeUs(uint32_t NewSampleTimeUs) {
  if (NewSampleTimeUs > 0) {
    float ratio  = (float)NewSampleTimeUs / (float)sampleTimeUs;
    ki *= ratio;
    kd /= ratio;
    sampleTimeUs = NewSampleTimeUs;
  }
}

/* SetOutputLimits(..)********************************************************
  The PID controller is designed to vary its output within a given range.
  By default this range is 0-255, the Arduino PWM range.
******************************************************************************/
void QuickPID::SetOutputLimits(float Min, float Max) {
  if (Min >= Max) return;
  outMin = Min;
  outMax = Max;

  if (mode != Control::manual) {
    *myOutput = CONSTRAIN(*myOutput, outMin, outMax);
    outputSum = CONSTRAIN(outputSum, outMin, outMax);
  }
}

/* SetMode(.)*****************************************************************
  Sets the controller mode to manual (0), automatic (1) or timer (2)
  when the transition from manual to automatic or timer occurs, the
  controller is automatically initialized.
******************************************************************************/
void QuickPID::SetMode(Control Mode, tGetTimeMicros getMicros = NULL) {
  if (mode == Control::manual && Mode != Control::manual) { // just went from manual to automatic or timer
    QuickPID::Initialize();
  }
  mode = Mode;
  _getMicros = NULL;
  if( _getMicros != NULL ) {
    lastTime = _getMicros() - sampleTimeUs;
  }
}

/* Initialize()****************************************************************
  Does all the things that need to happen to ensure a bumpless transfer
  from manual to automatic mode.
******************************************************************************/
void QuickPID::Initialize() {
  outputSum = *myOutput;
  lastInput = *myInput;
  outputSum = CONSTRAIN(outputSum, outMin, outMax);
}

/* SetControllerDirection(.)**************************************************
  The PID will either be connected to a direct acting process (+Output leads
  to +Input) or a reverse acting process(+Output leads to -Input).
******************************************************************************/
void QuickPID::SetControllerDirection(Action Action) {
  action = Action;
}

/* SetProportionalMode(.)*****************************************************
  Sets the computation method for the proportional term, to compute based
  either on error (default), on measurement, or the average of both.
******************************************************************************/
void QuickPID::SetProportionalMode(pMode pMode) {
  pmode = pMode;
}

/* SetDerivativeMode(.)*******************************************************
  Sets the computation method for the derivative term, to compute based
  either on error or on measurement (default).
******************************************************************************/
void QuickPID::SetDerivativeMode(dMode dMode) {
  dmode = dMode;
}

/* SetAntiWindupMode(.)*******************************************************
  Sets the integral anti-windup mode to one of iAwClamp, which clamps
  the output after adding integral and proportional (on measurement) terms,
  or iAwCondition (default), which provides some integral correction, prevents
  deep saturation and reduces overshoot.
  Option iAwOff disables anti-windup altogether.
******************************************************************************/
void QuickPID::SetAntiWindupMode(iAwMode iAwMode) {
  iawmode = iAwMode;
}

/* Status Functions************************************************************
  These functions query the internal state of the PID.
******************************************************************************/
float QuickPID::GetKp() {
  return dispKp;
}
float QuickPID::GetKi() {
  return dispKi;
}
float QuickPID::GetKd() {
  return dispKd;
}
float QuickPID::GetPterm() {
  return pTerm;
}
float QuickPID::GetIterm() {
  return iTerm;
}
float QuickPID::GetDterm() {
  return dTerm;
}
uint8_t QuickPID::GetMode() {
  return static_cast<uint8_t>(mode);
}
uint8_t QuickPID::GetDirection() {
  return static_cast<uint8_t>(action);
}
uint8_t QuickPID::GetPmode() {
  return static_cast<uint8_t>(pmode);
}
uint8_t QuickPID::GetDmode() {
  return static_cast<uint8_t>(dmode);
}
uint8_t QuickPID::GetAwMode() {
  return static_cast<uint8_t>(iawmode);
}
