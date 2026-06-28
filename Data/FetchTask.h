#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "AircraftLock.h"
#include "AircraftFetcher.h"

class FetchTask {
public:
  static volatile bool     fetching;
  static volatile uint32_t lastFetchMs;
  static volatile bool     newDataFlag;
  // -1 keeps the scheduler in radar scanning mode; any valid index switches it to detail-only updates.
  static volatile int      focusIdx;

	  static void begin(float lat, float lon, int radius) {
	    _lat = lat; _lon = lon; _radius = radius;
	    if (!gAircraftMutex) gAircraftMutex = xSemaphoreCreateMutex();
	    xTaskCreatePinnedToCore(_task, "fetch", 10240, nullptr, 1, &_handle, 0);
	  }

  static void setLocation(float lat, float lon, int radius) {
    _lat = lat; _lon = lon; _radius = radius;
  }

	  static void lock()       { aircraftLock(); }
	  static void unlock()     { aircraftUnlock(); }
  static void triggerNow() { _forceNow = true; }

  static void setFocus(int idx) {
    focusIdx     = idx;
    _resetTimers = true;
    _forceNow    = true;
  }

  static void clearFocus() {
    focusIdx     = -1;
    _resetTimers = true;
    _forceNow    = true;
  }

private:
		  static float              _lat, _lon;
		  static int                _radius;
		  static TaskHandle_t       _handle;
		  static volatile bool      _forceNow;
  // Pulsed on mode changes so the next loop fetches immediately instead of waiting a full interval.
  static volatile bool      _resetTimers;

  static constexpr uint32_t RADAR_INTERVAL  = 15000;
  static constexpr uint32_t DETAIL_INTERVAL = 2000;

  static void _task(void*) {
    uint32_t lastRadar  = 0;
    uint32_t lastDetail = 0;

    vTaskDelay(pdMS_TO_TICKS(2800));

	    for (;;) {
	      if (_resetTimers) {
        _resetTimers = false;
        lastRadar    = 0;
        lastDetail   = 0;
      }

      uint32_t now   = millis();
	      int      focus = focusIdx;

      if (Network::ok()) {

	        if (focus < 0) {
	          if (_forceNow || (now - lastRadar) >= RADAR_INTERVAL) {
            _forceNow = false;
            lastRadar = now;
            fetching  = true;
            bool ok = AircraftFetcher::fetchNearbyBounded(_lat, _lon, _radius);
            fetching  = false;
            if (ok) {
              newDataFlag = true;
              lastFetchMs = millis();
            }
          }

	        } else {
	          if (_forceNow || (now - lastDetail) >= DETAIL_INTERVAL) {
            _forceNow  = false;
            lastDetail = now;
	            aircraftLock();
	            int cnt    = AircraftFetcher::count;
	            aircraftUnlock();
            if (focus >= 0 && focus < cnt) {
              fetching = true;
	              AircraftFetcher::fetchDetail(focus);
	              fetching = false;

              // Route lookup is slower and only needed once after entering detail mode.
		              aircraftLock();
	              bool needRoute = (focus >= 0 && focus < AircraftFetcher::count &&
	                                !AircraftFetcher::list[focus].routeLoaded);
	              aircraftUnlock();
	              if (needRoute) {
                fetching = true;
                AircraftFetcher::fetchRoute(focus);
                fetching = false;
              }

              newDataFlag = true;
              lastFetchMs = millis();
            }
          }
        }
      }

      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
};
