#include "platform_rtc.h"

#include "smartwatch_board.h"

esp_err_t platform_rtc_get(platform_datetime_t *datetime)
{
    if (datetime == NULL) return ESP_ERR_INVALID_ARG;
    pcf85063a_dev_t *rtc = smartwatch_board_get_rtc();
    if (rtc == NULL) return ESP_ERR_INVALID_STATE;
    pcf85063a_datetime_t value = {0};
    esp_err_t err = pcf85063a_get_time_date(rtc, &value);
    if (err != ESP_OK) return err;
    *datetime = (platform_datetime_t) {
        .year = value.year, .month = value.month, .day = value.day, .weekday = value.dotw,
        .hour = value.hour, .minute = value.min, .second = value.sec,
    };
    return ESP_OK;
}

esp_err_t platform_rtc_set(const platform_datetime_t *datetime)
{
    if (datetime == NULL) return ESP_ERR_INVALID_ARG;
    pcf85063a_dev_t *rtc = smartwatch_board_get_rtc();
    if (rtc == NULL) return ESP_ERR_INVALID_STATE;
    const pcf85063a_datetime_t value = {
        .year = datetime->year, .month = datetime->month, .day = datetime->day,
        .dotw = datetime->weekday, .hour = datetime->hour,
        .min = datetime->minute, .sec = datetime->second,
    };
    return pcf85063a_set_time_date(rtc, value);
}
