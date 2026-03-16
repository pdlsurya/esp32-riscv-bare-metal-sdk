if(NOT TARGET_SOC STREQUAL "esp32p4")
    message(FATAL_ERROR "drivers/ble_nimble hosted integration supports only TARGET_SOC=esp32p4")
endif()

set(BLE_NIMBLE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(BLE_NIMBLE_THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/esp-nimble)

file(GLOB BLE_NIMBLE_HOST_SOURCES CONFIGURE_DEPENDS
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/*.c)
file(GLOB BLE_NIMBLE_TINYCRYPT_SOURCES CONFIGURE_DEPENDS
    ${BLE_NIMBLE_THIRD_PARTY}/ext/tinycrypt/src/*.c)

list(REMOVE_ITEM BLE_NIMBLE_HOST_SOURCES
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_cs.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_dtm.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_ead.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_eatt.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_eddystone.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_gattc_cache.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_gattc_cache_conn.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_hs_iso.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_hs_iso_hci.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_hs_periodic_sync.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_ibeacon.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/src/ble_l2cap_coc.c
)

list(APPEND BLE_NIMBLE_SOURCES
    ${BLE_NIMBLE_DIR}/ble_nimble.c
    ${BLE_NIMBLE_DIR}/ble_npl_baremetal.c
    ${BLE_NIMBLE_DIR}/nimble_port_baremetal.c
    ${BLE_NIMBLE_DIR}/ble_hci_hosted.c
    ${BLE_NIMBLE_DIR}/ble_nimble_console.c
    ${BLE_NIMBLE_THIRD_PARTY}/porting/nimble/src/endian.c
    ${BLE_NIMBLE_THIRD_PARTY}/porting/nimble/src/mem.c
    ${BLE_NIMBLE_THIRD_PARTY}/porting/nimble/src/os_mempool.c
    ${BLE_NIMBLE_THIRD_PARTY}/porting/nimble/src/os_mbuf.c
    ${BLE_NIMBLE_THIRD_PARTY}/porting/nimble/src/os_msys_init.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/transport/src/transport.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/store/ram/src/ble_store_ram.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/services/gap/src/ble_svc_gap.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/services/gatt/src/ble_svc_gatt.c
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/util/src/addr.c
    ${BLE_NIMBLE_HOST_SOURCES}
    ${BLE_NIMBLE_TINYCRYPT_SOURCES}
)

target_sources(esp32_rv_sdk PUBLIC ${BLE_NIMBLE_SOURCES})

target_include_directories(esp32_rv_sdk PUBLIC
    ${BLE_NIMBLE_DIR}
    ${BLE_NIMBLE_DIR}/include
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/include
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/include
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/services/gap/include
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/services/gatt/include
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/store/ram/include
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/host/util/include
    ${BLE_NIMBLE_THIRD_PARTY}/nimble/transport/include
    ${BLE_NIMBLE_THIRD_PARTY}/porting/nimble/include
    ${BLE_NIMBLE_THIRD_PARTY}/ext/tinycrypt/include
)

target_compile_definitions(esp32_rv_sdk PUBLIC
    CONFIG_BT_NIMBLE_ENABLED=1
    CONFIG_BT_CONTROLLER_ENABLED=0
    CONFIG_BT_NIMBLE_GAP_SERVICE=1
    CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE=0
    CONFIG_BT_NIMBLE_MAX_EADS=0
    CONFIG_MBEDTLS_VER_4_X_SUPPORT=0
    MYNEWT_VAL_BLE_DYNAMIC_SERVICE=0
    MYNEWT_VAL_ENC_ADV_DATA=0
    MYNEWT_VAL_BLE_GATTC=0
    MYNEWT_VAL_BLE_GATTS=1
    MYNEWT_VAL_BLE_HOST_BASED_PRIVACY=0
    MYNEWT_VAL_BLE_TRANSPORT_LL__socket=0
    MYNEWT_VAL_BLE_TRANSPORT_LL__custom=1
    MYNEWT_VAL_BLE_HS_DEBUG=0
    MYNEWT_VAL_BLE_QUEUE_CONG_CHECK=0
    MYNEWT_VAL_BLE_GATT_CACHING=0
    MYNEWT_VAL_BLE_GATT_CSFC_SIZE=1
    MYNEWT_VAL_BLE_GATT_CACHING_INCLUDE_SERVICES=0
    MYNEWT_VAL_BLE_GATT_CACHING_ASSOC_ENABLE=0
    MYNEWT_VAL_BLE_GATT_CACHING_DISABLE_AUTO=0
    MYNEWT_VAL_BLE_GATT_NOTIFY_MULTIPLE=0
    MYNEWT_VAL_BLE_HS_PVCY=0
    MYNEWT_VAL_BLE_HS_REQUIRE_OS=0
    MYNEWT_VAL_BLE_STATIC_TO_DYNAMIC=0
    MYNEWT_VAL_BLE_CRYPTO_STACK_MBEDTLS=0
    MYNEWT_VAL_BLE_MAX_CONNECTIONS=1
    MYNEWT_VAL_BLE_MESH=0
    MYNEWT_VAL_BLE_MONITOR_RTT=0
    MYNEWT_VAL_BLE_MONITOR_UART=0
    MYNEWT_VAL_BLE_ISO=0
    MYNEWT_VAL_BLE_EXT_ADV=0
    MYNEWT_VAL_BLE_PERIODIC_ADV=0
    MYNEWT_VAL_BLE_PERIODIC_ADV_SYNC_TRANSFER=0
    MYNEWT_VAL_BLE_EATT=0
    MYNEWT_VAL_BLE_EATT_CHAN_NUM=0
    MYNEWT_VAL_BLE_RECONFIG_MTU=0
    MYNEWT_VAL_BLE_HS_FLOW_CTRL=0
    MYNEWT_VAL_BLE_ROLE_BROADCASTER=1
    MYNEWT_VAL_BLE_ROLE_CENTRAL=0
    MYNEWT_VAL_BLE_ROLE_OBSERVER=0
    MYNEWT_VAL_BLE_ROLE_PERIPHERAL=1
    MYNEWT_VAL_BLE_SVC_HID_SERVICE=0
    MYNEWT_VAL_BLE_STORE_MAX_CSFCS=0
    MYNEWT_VAL_BLE_CPFD_CAFD=0
)
