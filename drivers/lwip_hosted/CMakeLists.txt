if(NOT TARGET_SOC STREQUAL "esp32p4")
    message(FATAL_ERROR "drivers/lwip_hosted supports only TARGET_SOC=esp32p4")
endif()

if(DEFINED SDK_PATH)
    set(LWIP_ROOT ${SDK_PATH}/third_party/lwip)
else()
    get_filename_component(LWIP_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../third_party/lwip ABSOLUTE)
endif()

set(LWIP_DIR ${LWIP_ROOT}/src)
set(LWIP_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/port)

set(LWIP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/lwip_hosted.c
    ${LWIP_DIR}/core/def.c
    ${LWIP_DIR}/core/dns.c
    ${LWIP_DIR}/core/init.c
    ${LWIP_DIR}/core/inet_chksum.c
    ${LWIP_DIR}/core/ip.c
    ${LWIP_DIR}/core/mem.c
    ${LWIP_DIR}/core/memp.c
    ${LWIP_DIR}/core/netif.c
    ${LWIP_DIR}/core/pbuf.c
    ${LWIP_DIR}/core/raw.c
    ${LWIP_DIR}/core/stats.c
    ${LWIP_DIR}/core/sys.c
    ${LWIP_DIR}/core/tcp.c
    ${LWIP_DIR}/core/tcp_in.c
    ${LWIP_DIR}/core/tcp_out.c
    ${LWIP_DIR}/core/timeouts.c
    ${LWIP_DIR}/core/udp.c
    ${LWIP_DIR}/core/ipv4/dhcp.c
    ${LWIP_DIR}/core/ipv4/acd.c
    ${LWIP_DIR}/core/ipv4/etharp.c
    ${LWIP_DIR}/core/ipv4/icmp.c
    ${LWIP_DIR}/core/ipv4/ip4.c
    ${LWIP_DIR}/core/ipv4/ip4_addr.c
    ${LWIP_DIR}/core/ipv4/ip4_frag.c
    ${LWIP_DIR}/netif/ethernet.c
)

target_sources(esp32_rv_sdk PUBLIC ${LWIP_SOURCES})

target_include_directories(esp32_rv_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LWIP_PORT_DIR}
    ${LWIP_PORT_DIR}/arch
    ${LWIP_DIR}/include
)
