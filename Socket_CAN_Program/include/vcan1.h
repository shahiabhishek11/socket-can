#include<iostream>





struct VECTOR__INDEPENDENT_SIG_MSG
{
    int8_t ble_pair_delete_reset_status;
    int8_t ble_pair_pin_gen_status;
    bool follow_me_home;
    bool handle_lock_ign_off;
    bool handle_lock_ign_off;
    bool hdl_unlock_ign_on;
    int8_t hdl_unlock_status;
    bool immob_status;
    bool ping_my_bike;
    int8_t tcu_reboot_status;
    bool trunk_lock;

};




typedef enum
    {
        TRUNK_UNLOCK,
        IMMOB_STATE_CHANGE_MSG,
        TRUNK_LOCK_STATE_CHANGE_MSG,
        QUARANTINED_FLAG_MSG,
        PING_MY_BIKE_MSG,
        FOLLOW_ME_HOME_STATE_CHANGE_MSG,
        CUSTOM_RIDE_MODE_MSG,
        HANDLE_UNLOCK_MSG,
        HANDLE_UNLOCK_AND_IGN_ON_MSG,
        RESERVED,
        HANDLE_LOCK_AND_IGN_OFF_MSG,
        ALERT_RESET_MSG,
        SET_CONFIGURATION_MSG,
        GET_CONFIGURATION_MSG,
        ECU_DETAILS_READ_MSG,
        CUSTOM_COMMAND_MSG,
        ALERT_ENABLE_DISABLE_MSG,
        RESERVED,
        BLE_PAIRING_PIN_GEN_MSG,
        BLE_PAIRING_DELETION_RESET_MSG,
        TCU_REBOOT_MSG,
        OVERSPEED_LIMIT_SET_MSG,
        BATTERY_CHARGE_LIMITER_MSG,
        VEHICLE_HEALTH_CHECK_MSG,
        SET_GEOFENCE_FLAG_MSG,
        SET_PERIODIC_WAKEUP_TIMER_MSG

    } REQ;

struct Request
{
    REQ req;
};



struct 