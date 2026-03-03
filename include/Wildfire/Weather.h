#pragma once

#include "Types.h"

namespace Weather {
	bool IsRaining();
    bool IsSnowing();
    WindData GetCurrentWind();

    WeatherData GetWeatherData();
}