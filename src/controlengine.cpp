#include "controlengine.h"

#include <algorithm>
#include <cmath>

namespace pid {

double Parameters::maxPower() const { return voltage * current; }

bool Parameters::valid() const
{
    return std::isfinite(target) && std::isfinite(kp) && kp >= 0.0
        && std::isfinite(ki) && ki >= 0.0 && std::isfinite(bias) && bias >= 0.0
        && std::isfinite(voltage) && voltage > 0.0 && std::isfinite(current) && current > 0.0
        && std::isfinite(maxPower()) && maxPower() > 0.0
        && codeMax > 0 && codeMax <= 65535 && adcBits >= 1 && adcBits <= 24;
}

double LeadLag::step(double input, double dt, const FeedForward &p)
{
    if (!std::isfinite(input) || !std::isfinite(dt) || dt < 0.0
        || !std::isfinite(p.gain) || !std::isfinite(p.numerator)
        || !std::isfinite(p.denominator) || p.numerator < 0.0 || p.denominator < 0.0)
        return 0.0;
    if (p.gain == 0.0) { reset(); return 0.0; }
    const double divisor = p.denominator + dt;
    if (divisor <= 1e-12) return previousOutput_;
    const double output = (p.denominator * previousOutput_
        + p.gain * (p.numerator + dt) * input
        - p.gain * p.numerator * previousInput_) / divisor;
    if (!std::isfinite(output)) { reset(); return 0.0; }
    previousInput_ = input;
    previousOutput_ = output;
    return output;
}

void LeadLag::reset() { previousOutput_ = previousInput_ = 0.0; }
void ControlEngine::reset() { integral_ = 0.0; e_.reset(); r_.reset(); }

ControlSample ControlEngine::step(double temperature, double dt, const Parameters &p,
                                 double disturbanceE, double disturbanceR,
                                 const FeedForward &e, const FeedForward &r)
{
    ControlSample sample;
    if (!p.valid() || !std::isfinite(temperature) || !std::isfinite(dt) || dt < 0.0)
        return sample;
    sample.temperature = temperature;
    if (p.quantize) {
        const double levels = (1u << p.adcBits) - 1;
        sample.temperature = std::round(std::clamp(temperature, 0.0, 100.0)
                                        / 100.0 * levels) * 100.0 / levels;
    }
    sample.error = p.target - sample.temperature;
    sample.proportional = p.kp * sample.error;
    sample.feedForwardE = e_.step(disturbanceE, dt, e);
    sample.feedForwardR = r_.step(disturbanceR, dt, r);
    const double oldIntegral = integral_;
    integral_ = p.ki == 0.0 ? 0.0 : integral_ + sample.error * dt;
    const auto demand = [&] {
        return sample.proportional + p.ki * integral_ + p.bias
            + sample.feedForwardE + sample.feedForwardR;
    };
    const double requested = demand();
    // Conditional integration: allow the integral to unwind at either limit.
    if ((requested > p.maxPower() && sample.error > 0.0)
        || (requested < 0.0 && sample.error < 0.0))
        integral_ = oldIntegral;
    sample.integral = p.ki * integral_;
    const double power = demand();
    if (!std::isfinite(power)) { reset(); return {}; }
    sample.power = std::clamp(power, 0.0, p.maxPower());
    sample.code = static_cast<std::uint16_t>(std::lround(sample.power / p.maxPower() * p.codeMax));
    return sample;
}

} // namespace pid
