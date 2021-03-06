/* The MIT License (MIT)
 *
 * Copyright (c) 2014 Colin Wallace
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 
 * Printipi/drivers/tempcontrol.h
 * 
 * TempControl provides a way to coordinate thermistor readings with the PWM control of a hotend OR heated bed.
 * It used a PID controller to determine the ideal PWM for a given thermistor reading and temperature target.
 * Additionally, it accepts an (optional) filter applied BEFORE the PID controller, which can be used to weed out some variability in readings (use a low-pass filter for this).
 * Currently, it assumes an RC-based thermistor, but in the future it may be expanded to work with any analog IoPin too.
 */
 

#ifndef DRIVERS_TEMPCONTROL_H
#define DRIVERS_TEMPCONTROL_H

#include "drivers/iodriver.h"
#include "common/filters/nofilter.h"
#include "common/intervaltimer.h"
#include "drivers/auto/chronoclock.h" //for EventClockT
#include "common/typesettings/primitives.h" //for CelciusType

namespace drv {

//enum passed as template parameter to define the TempControl instance as either controlling a Hotend or a Heated Bed.
//Functionally, they work the same, but each type responds to different G-codes.
enum TempControlType {
    HotendType,
    HeatedBedType
};

template <TempControlType HotType, AxisIdType DeviceIdx, typename Heater, typename Thermistor, typename PID, typename Filter=NoFilter> class TempControl : public IODriver {
    static const std::chrono::microseconds _intervalThresh;
    static const std::chrono::microseconds _readInterval;
    static const std::chrono::microseconds _maxRead;
    IntervalTimer _intervalTimer;
    Heater _heater;
    Thermistor _therm;
    PID _pid;
    Filter _filter;
    float _destTemp;
    float _lastTemp;
    bool _isReading;
    EventClockT::time_point _nextReadTime;
    public:
        TempControl() : IODriver(), _destTemp(-300), _lastTemp(-300), _isReading(false),
         _nextReadTime(EventClockT::now()) {
            _heater.makeDigitalOutput(IoLow);
        }
        //register as the correct device type:
        bool isHotend() const {
            return HotType == HotendType;
        }
        bool isHeatedBed() const {
            return HotType == HeatedBedType;
        }
        //route output commands to the heater:
        void stepForward() {
            _heater.digitalWrite(IoHigh);
        }
        void stepBackward() {
            _heater.digitalWrite(IoLow);
        }
        void setTargetTemperature(CelciusType t) {
            _destTemp = t;
        }
        CelciusType getMeasuredTemperature() const {
            return _lastTemp;
        }
        Heater& getPwmPin() { //Note: will be able to handle PWMing multiple pins, too, if one were just to use a wrapper and pass it as the Driver type.
            return _heater;
        }
        template <typename Sched> bool onIdleCpu(Sched &sched) {
            //LOGV("TempControl::onIdleCpu()\n");
            if (_isReading) {
                if (_therm.isReady()) {
                    _isReading = false;
                    if (_intervalTimer.clockCmp(_intervalThresh) > 0) { //too much latency in reading sample; restart.
                        LOGV("Thermistor sample dropped\n");
                        return true; //restart read.
                    } else {
                        _lastTemp = _therm.value();
                        updatePwm(sched);
                        return false; //no more cpu needed.
                    }
                } else {
                    _intervalTimer.clock();
                    if (_therm.timeSinceStartRead() > _maxRead) {
                        LOG("Thermistor read error\n");
                        _isReading = false;
                        return false;
                    } else {
                        return true; //need more cpu time.
                    }
                }
            } else {
                auto now = _intervalTimer.clock();
                if (_nextReadTime < now) {
                    _nextReadTime += _readInterval;
                    _therm.startRead();
                    _isReading = true;
                    return true; //more cpu time needed.
                } else { //wait until it's time for another read.
                    return false;
                }
            }
        }
    private:
        template <typename Sched> void updatePwm(Sched &sched) {
	        // Make this actually pass both the setpoint and process value
	        // into the the controller
            float filtered = _filter.feed(_lastTemp);
            float pwm = _pid.feed(_destTemp, filtered);
            LOG("tempcontrol: pwm=%f, temp=%f *C\n", pwm, filtered);
            sched.schedPwm(DeviceIdx, pwm, heaterPwmPeriod());
        }
};

#if RUNNING_IN_VM
    template <TempControlType HotType, AxisIdType DeviceIdx, typename Heater, typename Thermistor, typename PID, typename Filter> const std::chrono::microseconds TempControl<HotType, DeviceIdx, Heater, Thermistor, PID, Filter>::_intervalThresh(2000000); //high latency for valgrind
#else
    template <TempControlType HotType, AxisIdType DeviceIdx, typename Heater, typename Thermistor, typename PID, typename Filter> const std::chrono::microseconds TempControl<HotType, DeviceIdx, Heater, Thermistor, PID, Filter>::_intervalThresh(40000);
#endif

template <TempControlType HotType, AxisIdType DeviceIdx, typename Heater, typename Thermistor, typename PID, typename Filter> const std::chrono::microseconds TempControl<HotType, DeviceIdx, Heater, Thermistor, PID, Filter>::_readInterval(3000000);
template <TempControlType HotType, AxisIdType DeviceIdx, typename Heater, typename Thermistor, typename PID, typename Filter> const std::chrono::microseconds TempControl<HotType, DeviceIdx, Heater, Thermistor, PID, Filter>::_maxRead(1000000);

}
#endif
