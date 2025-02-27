#pragma once
#ifndef QuickPID_h
#define QuickPID_h

typedef unsigned long (*tGetTimeMicros)(void);

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define CONSTRAIN(x,a,b) MIN(MAX((x),(a)),(b))

class QuickPID {

  public:

    enum class Control : uint8_t {manual, automatic, timer};        // controller mode
    enum class Action : uint8_t {direct, reverse};                  // controller action
    enum class pMode : uint8_t {pOnError, pOnMeas, pOnErrorMeas};   // proportional mode
    enum class dMode : uint8_t {dOnError, dOnMeas};                 // derivative mode
    enum class iAwMode : uint8_t {iAwCondition, iAwClamp, iAwOff};  // integral anti-windup mode

    // commonly used functions ************************************************************************************

    // Constructor. Links the PID to Input, Output, Setpoint, initial tuning parameters and control modes.
    QuickPID(float *Input, float *Output, float *Setpoint, float Kp, float Ki, float Kd,
             pMode pMode, dMode dMode, iAwMode iAwMode, Action Action);

    // Overload constructor links the PID to Input, Output, Setpoint, tuning parameters and control Action.
    // Uses defaults for remaining parameters.
    QuickPID(float *Input, float *Output, float *Setpoint, float Kp, float Ki, float Kd, Action Action);

    // Simplified constructor which uses defaults for remaining parameters.
    QuickPID(float *Input, float *Output, float *Setpoint);

    // Sets PID mode to manual (0), automatic (1) or timer (2).
    void SetMode(Control Mode, tGetTimeMicros* getTimeMicros);

    // Performs the PID calculation. It should be called every time loop() cycles ON/OFF and calculation frequency
    // can be set using SetMode and SetSampleTime respectively.
    bool Compute();

    // Sets and clamps the output to a specific range (0-255 by default).
    void SetOutputLimits(float Min, float Max);

    // available but not commonly used functions ******************************************************************

    // While most users will set the tunings once in the constructor, this function gives the user the option of
    // changing tunings during runtime for Adaptive control.
    void SetTunings(float Kp, float Ki, float Kd);

    // Overload for specifying proportional ratio.
    void SetTunings(float Kp, float Ki, float Kd, pMode pMode, dMode dMode, iAwMode iAwMode);

    // Sets the controller direction or action. Direct means the output will increase when the error is positive.
    // Reverse means the output will decrease when the error is positive.
    void SetControllerDirection(Action Action);

    // Sets the sample time in microseconds with which each PID calculation is performed. Default is 100000 µs.
    void SetSampleTimeUs(uint32_t NewSampleTimeUs);

    // Sets the computation method for the proportional term, to compute based either on error (default),
    // on measurement, or the average of both.
    void SetProportionalMode(pMode pMode);

    // Sets the computation method for the derivative term, to compute based either on error or measurement (default).
    void SetDerivativeMode(dMode dMode);

    // Sets the integral anti-windup mode to one of iAwClamp, which clamps the output after
    // adding integral and proportional (on measurement) terms, or iAwCondition (default), which
    // provides some integral correction, prevents deep saturation and reduces overshoot.
    // Option iAwOff disables anti-windup altogether.
    void SetAntiWindupMode(iAwMode iAwMode);

    // PID Query functions ****************************************************************************************
    float GetKp();            // proportional gain
    float GetKi();            // integral gain
    float GetKd();            // derivative gain
    float GetPterm();         // proportional component of output
    float GetIterm();         // integral component of output
    float GetDterm();         // derivative component of output
    uint8_t GetMode();        // manual (0), automatic (1) or timer (2)
    uint8_t GetDirection();   // direct (0), reverse (1)
    uint8_t GetPmode();       // pOnError (0), pOnMeas (1), pOnErrorMeas (2)
    uint8_t GetDmode();       // dOnError (0), dOnMeas (1)
    uint8_t GetAwMode();      // iAwCondition (0, iAwClamp (1), iAwOff (2)

  private:

    void Initialize();

    float dispKp = 0;   // for defaults and display
    float dispKi = 0;
    float dispKd = 0;
    float pTerm;
    float iTerm;
    float dTerm;

    float kp;           // (P)roportional Tuning Parameter
    float ki;           // (I)ntegral Tuning Parameter
    float kd;           // (D)erivative Tuning Parameter

    float *myInput;     // Pointers to the Input, Output, and Setpoint variables. This creates a
    float *myOutput;    // hard link between the variables and the PID, freeing the user from having
    float *mySetpoint;  // to constantly tell us what these values are. With pointers we'll just know.

    tGetTimeMicros _getMicros; // Function to use in 'automatic' mode that allows polling of time since wakeup in microseconds

    Control mode = Control::manual;
    Action action = Action::direct;
    pMode pmode = pMode::pOnError;
    dMode dmode = dMode::dOnMeas;
    iAwMode iawmode = iAwMode::iAwCondition;

    uint32_t sampleTimeUs, lastTime;
    float outputSum, outMin, outMax, error, lastError, lastInput;

}; // class QuickPID
#endif // QuickPID.h
