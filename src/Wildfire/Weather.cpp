#include "Wildfire/Weather.h"

namespace Weather {
    bool IsRaining() {
        auto sky = RE::Sky::GetSingleton();
        return sky->IsRaining();
    }

	bool IsSnowing() {
		auto sky = RE::Sky::GetSingleton();
		return sky->IsSnowing();
    }

	WindData GetCurrentWind() {
		WindData data{0.0f, 0.0f};
		auto sky = RE::Sky::GetSingleton();
		if (sky) {
			data.direction = sky->windAngle;
			data.speed = sky->windSpeed;
		}
		return data;
	}

	WeatherData GetWeatherData() { WeatherData res;
        res.isRaining = IsRaining();
        res.isSnowing = IsSnowing();
        res.windData = GetCurrentWind();
        return res;
	}
}