#include "controlengine.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>
#include <cstdlib>

namespace {
int checks = 0;
void check(bool condition, const char *message)
{
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-8; }
}

int main()
{
    pid::Parameters p;
    pid::ControlEngine engine;
    check(p.valid(), "default parameters");
    check(near(p.maxPower(), 24.0 * 2.0), "generic power limit");
    auto sample = engine.step(17, 0, p);
    check(near(sample.power, 12), "bias at setpoint");
    check(sample.code <= p.codeMax, "output bounds");
    for (int i = 0; i < 1000; ++i) sample = engine.step(0, 1, p);
    check(near(sample.power, p.maxPower()), "upper saturation");
    check(near(sample.integral, 0), "integral does not wind up under saturation");
    sample = engine.step(100, 1, p);
    check(sample.code == 0 && near(sample.power, 0), "lower saturation");
    sample = engine.step(17, 1, p);
    check(near(sample.power, 12), "recovery after saturation");
    engine.reset(); p.kp = 0; p.ki = 1;
    sample = engine.step(16, 1, p);
    check(near(sample.integral, 1), "time-scaled integration");
    sample = engine.step(16, 0.5, p);
    check(near(sample.integral, 1.5), "variable sample interval");
    p.ki = 0;
    sample = engine.step(16, 1, p);
    check(near(sample.integral, 0), "zero Ki clears integral");
    p.voltage = 0;
    check(!p.valid() && engine.step(16, 1, p).code == 0, "invalid supply fails closed");
    p = {}; p.codeMax = 65536;
    check(!p.valid(), "16-bit code storage limit");
    p = {}; p.adcBits = 32;
    check(!p.valid(), "quantizer shift bounds");
    p = {};
    check(engine.step(std::numeric_limits<double>::quiet_NaN(), 1, p).code == 0, "NaN reading rejected");
    check(engine.step(17, -1, p).code == 0, "negative interval rejected");
    p.quantize = true; p.adcBits = 2;
    sample = engine.step(34.0, 0, p);
    check(near(sample.temperature, 100.0 / 3.0), "generic 0-100 quantization");
    pid::LeadLag lag;
    check(near(lag.step(2, 1, {3, 0, 0}), 6), "static feed-forward gain");
    lag.reset();
    check(near(lag.step(2, 1, {3, 0, 1}), 3), "first lead-lag step");
    check(near(lag.step(2, 1, {3, 0, 1}), 4.5), "lead-lag state");
    lag.reset();
    check(near(lag.step(2, 0, {3, 0, 0}), 0), "zero divisor");
    check(near(lag.step(2, 1, {3, 0, -1}), 0), "invalid lag constant");
    p = {}; engine.reset();
    sample = engine.step(17, 1, p, -5, 2, {0.8, 0, 0}, {1, 0, 0});
    check(near(sample.power, 10.0), "independent feed-forward sum");
    // Closed-loop model, exercising feedback, limits, integration, and variable dt.
    engine.reset(); double temperature = 16.35;
    for (int i = 0; i < 2400; ++i) {
        const double dt = (i % 2 == 0) ? 0.2 : 0.3;
        sample = engine.step(temperature, dt, p);
        temperature += (sample.power - 12 * (temperature - 16)) / 240 * dt;
    }
    check(std::abs(temperature - p.target) < 0.005, "closed-loop convergence");
    // A load pulse must be rejected by the synthetic closed loop; feed-forward
    // should reduce its maximum temperature error relative to PI alone.
    const auto loadPulseError = [&](bool compensation) {
        pid::ControlEngine controller;
        double modelTemperature = 16.35;
        double peakError = 0.0;
        for (int i = 0; i < 1200; ++i) {
            const double seconds = i * 0.25;
            const double load = seconds >= 120.0 && seconds < 150.0 ? 5.0 : 0.0;
            const auto output = controller.step(modelTemperature, 0.25, p, -load, 0.0,
                compensation ? pid::FeedForward{0.8, 0.5, 2.0} : pid::FeedForward{});
            modelTemperature += (output.power + load - 12.0 * (modelTemperature - 16.0)) / 240.0 * 0.25;
            if (seconds >= 120.0 && seconds < 210.0)
                peakError = std::max(peakError, std::abs(modelTemperature - p.target));
        }
        check(std::abs(modelTemperature - p.target) < 0.005, "recovery after synthetic load pulse");
        return peakError;
    };
    const double piError = loadPulseError(false);
    const double compensatedError = loadPulseError(true);
    check(piError < 0.08, "demo PI disturbance envelope");
    check(compensatedError < piError * 0.5, "feed-forward improves synthetic load rejection");
    std::cout << checks << " control checks passed\n";
}
