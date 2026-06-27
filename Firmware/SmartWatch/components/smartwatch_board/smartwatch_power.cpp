#include "smartwatch_board.h"

#include <cmath>

#include "XPowersAXP2101.hpp"

static XPowersAXP2101 pmic;
static bool pmic_initialized;

extern "C" esp_err_t smartwatch_power_driver_init(i2c_master_bus_handle_t bus)
{
    if (pmic_initialized) {
        return ESP_OK;
    }
    if (bus == nullptr || !pmic.begin(bus, AXP2101_SLAVE_ADDRESS)) {
        return ESP_FAIL;
    }

    bool configured = pmic.enableBattDetection();
    configured &= pmic.enableBattVoltageMeasure();
    configured &= pmic.enableVbusVoltageMeasure();
    configured &= pmic.enableSystemVoltageMeasure();
    configured &= pmic.enableTemperatureMeasure();
    configured &= pmic.disableTSPinMeasure();
    if (!configured) {
        pmic.deinit();
        return ESP_FAIL;
    }

    pmic_initialized = true;
    return ESP_OK;
}

extern "C" esp_err_t smartwatch_power_driver_get_status(smartwatch_board_power_status_t *status)
{
    if (!pmic_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    status->battery_present = pmic.isBatteryConnect();
    status->vbus_present = pmic.isVbusIn();
    status->charging = pmic.isCharging();
    status->discharging = pmic.isDischarge();
    status->battery_percent = pmic.getBatteryPercent();
    status->battery_mv = pmic.getBattVoltage();
    status->vbus_mv = pmic.getVbusVoltage();
    status->system_mv = pmic.getSystemVoltage();
    status->pmic_temperature_c = pmic.getTemperature();
    return std::isfinite(status->pmic_temperature_c) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

extern "C" esp_err_t smartwatch_power_driver_shutdown(void)
{
    if (!pmic_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    pmic.shutdown();
    return ESP_OK;
}
