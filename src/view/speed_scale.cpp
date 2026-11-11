#include "view/speed_scale.h"

#include <algorithm>
#include <cmath>

#include "shared/app_settings.h"

namespace lifegame::view
{

int speedForSliderPosition(int position)
{
    if (position <= 0)
    {
        return 0;
    }

    const int clamped = std::clamp(position, 1, kSpeedSliderSteps);
    const double ratio =
        static_cast<double>(clamped - 1) / static_cast<double>(kSpeedSliderSteps - 1);
    const double value = std::exp(ratio * std::log(static_cast<double>(shared::limits::kSpeedMax)));
    return std::clamp(static_cast<int>(std::lround(value)), 1, shared::limits::kSpeedMax);
}

int sliderPositionForSpeed(int speed)
{
    const int clamped = std::clamp(speed, shared::limits::kSpeedMin, shared::limits::kSpeedMax);
    if (clamped <= 0)
    {
        return 0;
    }

    const double ratio = std::log(static_cast<double>(clamped)) /
                         std::log(static_cast<double>(shared::limits::kSpeedMax));
    return std::clamp(static_cast<int>(std::lround(1.0 + ratio * (kSpeedSliderSteps - 1))),
                      1,
                      kSpeedSliderSteps);
}

} // namespace lifegame::view
