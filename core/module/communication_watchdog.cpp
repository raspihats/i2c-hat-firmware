/*
 * communication_watchdog.cpp
 *
 *  Created on: Feb 6, 2016
 *      Author: fcos
 */
#include "communication_watchdog.h"
#include "../driver/eeprom.h"

#define TASK_DELAY_MS                   (0)
#define TASK_PERIOD_MS                  (5)

#define CWDT_DISABLED_PERIOD_MS         (0)
#define CWDT_DEFAULT_PERIOD_MS          (CWDT_DISABLED_PERIOD_MS)

namespace i2c_hat {
namespace module {

using namespace driver;

/**
  * @brief  Builds CommWatchdog Module object
  * @retval None
  */
CommunicationWatchdog::CommunicationWatchdog() :
        Module(TASK_DELAY_MS, TASK_PERIOD_MS),
        period_(CWDT_DEFAULT_PERIOD_MS),
        communication_ (false),
        state_(State::DISABLED) {

}

/**
  * @brief  Gets communication watchdog period
  * @param  None
  * @retval communication watchdog period in milliseconds
  */
uint32_t CommunicationWatchdog::period() {
    return period_;
}

/**
  * @brief  Sets communication watchdog period
  * @param  period - communication watchdog period in milliseconds
  * @retval None
  */
bool CommunicationWatchdog::SetPeriod(const uint32_t period) {
    if(Eeprom::Write(EEP_VIRT_ADR_CWDT_PERIOD, period)) {
        period_ = period;
        return true;
    }
    else {
        // TODO Error handler
    }
    return false;
}

/**
  * @brief  Feeds the communication watchdog
  * @param  None
  * @retval None
  */
void CommunicationWatchdog::Feed() {
    communication_ = true;
}

/**
  * @brief  Check if the communication watchdog has expired, timeout occurred
  * @param  None
  * @retval TRUE    Communication watchdog expired
  * @retval FALSE   Communication watchdog disabbled or running
  */
bool CommunicationWatchdog::IsExpired() {
    return (communication_ == false) and (state_ == State::TIMEOUT);
}

/**
  * @brief  CommWatchdog Module Init implementation
  * @param  None
  * @retval None
  */
void CommunicationWatchdog::Init() {
    bool success;

    success = Eeprom::Read(EEP_VIRT_ADR_CWDT_PERIOD, period_);
    if(not success) {
        // TODO Error handler
    }
}

/**
  * @brief  CommWatchdog Module cooperative task implementation
  * @param  None
  * @retval None
  */
void CommunicationWatchdog::Run() {
    static uint32_t period_cnt;

    switch(state_) {
    case State::DISABLED:
        if(communication_) {
            if(period_ > CWDT_DISABLED_PERIOD_MS) {
                period_cnt = 0;
                SendEvent(EVENT_CWDT_MONITORING);
                state_ = State::MONITORING;
            }
        }
        break;
    case State::MONITORING:
        if(communication_) {
            if(period_ == CWDT_DISABLED_PERIOD_MS) {
                SendEvent(EVENT_CWDT_DISABLED);
                state_ = State::DISABLED;
            }
            period_cnt = 0;
        }
        else {
            period_cnt += TASK_PERIOD_MS;
            if(period_cnt >= period_) {
                period_cnt = 0;
                SendEvent(EVENT_CWDT_TIMEOUT);
                state_ = State::TIMEOUT;
            }
        }
        break;
    case State::TIMEOUT:
        if(communication_) {
            if(period_ > CWDT_DISABLED_PERIOD_MS) {
                period_cnt = 0;
                SendEvent(EVENT_CWDT_MONITORING);
                state_ = State::MONITORING;
            }
            else {
                state_ = State::DISABLED;
            }
        }
        break;
    default:
        period_cnt = 0;
        state_ = State::DISABLED;
    }

    communication_ = false;
}

/**
  * @brief  CommWatchdog Module receive event implementation
  * @param  event
  * @retval None
  */
void CommunicationWatchdog::ReceiveEvent(const uint32_t event) {
    (void)event;
}

/**
  * @brief  Processes I2C request frame
  * @param  request: pointer to request frame
  * @param  response: pointer to response frame
  * @retval 1 if response
  *         0 if no response
  */
bool CommunicationWatchdog::ProcessRequest(Frame& request, Frame& response) {
    bool response_flag = false;

    Feed();

    switch((Command)request.command()) {
    case Command::CWDT_SET_PERIOD:
        if(request.payload_size() == 4) {
            uint32_t tempU32;
            const uint8_t* data = request.payload();

            BYTES_TO_UINT32(data, tempU32);
            SetPeriod(tempU32);
            response.set_payload((uint8_t*)&period_, 4);
            response_flag = true;
          }
        break;
    case Command::CWDT_GET_PERIOD:
        if(request.payload_size() == 0) {
            response.set_payload((uint8_t*)&period_, 4);
            response_flag = true;
        }
        break;
    default:
        response_flag = false;
    }

    return response_flag;
}

} /* namespace module */
} /* namespace i2c_hat */

