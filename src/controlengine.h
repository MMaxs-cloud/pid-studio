#pragma once

#include <cstdint>

namespace pid {

// Synthetic example settings; not a calibration for any physical device.
struct Parameters {
    double target = 17.0;
    double kp = 80.0;
    double ki = 2.0;
    double bias = 12.0;
    double voltage = 24.0;
    double current = 2.0;
    int codeMax = 1000;
    int adcBits = 10;
    bool quantize = false;
    double maxPower() const;
    bool valid() const;
};

struct FeedForward {
    double gain = 0.0;
    double numerator = 0.0;
    double denominator = 0.0;
};

struct ControlSample {
    double temperature = 0.0;
    double error = 0.0;
    double proportional = 0.0;
    double integral = 0.0;
    double feedForwardE = 0.0;
    double feedForwardR = 0.0;
    double power = 0.0;
    std::uint16_t code = 0;
};

class LeadLag {
public:
    double step(double input, double dt, const FeedForward &parameters);
    void reset();
private:
    double previousOutput_ = 0.0;
    double previousInput_ = 0.0;
};

// PI with two independent feed-forward channels. No Qt or hardware dependencies.
class ControlEngine {
public:
    void reset();
    ControlSample step(double temperature, double dt, const Parameters &parameters,
                       double disturbanceE = 0.0, double disturbanceR = 0.0,
                       const FeedForward &e = {}, const FeedForward &r = {});
private:
    double integral_ = 0.0;
    LeadLag e_, r_;
};

} // namespace pid
