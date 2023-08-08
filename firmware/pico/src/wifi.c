#include "wifi.h"
#include "secret_keys.h"

extern bool WIFI_Init(void)
{
    bool success = false;
    printf("Initialising WIFI driver\n");
    if(cyw43_arch_init_with_country(CYW43_COUNTRY_UK))
    {
        printf("Failed\n");
        goto cleanup;
    }
    WIFI_ClearLed();
    cyw43_arch_enable_sta_mode();
    
    success = true;
cleanup:
    return success;
}

extern void WIFI_TryConnect(void)
{
    WIFI_ClearLed();
    if(cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_MIXED_PSK))
    {
        printf("Failed to retry\n");
    }
}

extern bool WIFI_CheckStatus(void)
{
    bool ret = false;
    int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA );
    
    if( status == CYW43_LINK_JOIN )
    {
        ret = true;
    }

    return ret;
}

extern void WIFI_SetLed(void)
{
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); 
}

extern void WIFI_ClearLed(void)
{
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0); 
}

extern void WIFI_ToggleLed(void)
{
    static bool status = true;

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, status);

    status ^= true;
}

