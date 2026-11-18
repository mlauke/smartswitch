#pragma once
#include <ETH.h>

#include "SmartSwitch.h"

#define ETH_SPI_HOST SPI3_HOST // vspi, its pins are routed via the gpio matrix

// the netif only exists after begin(), so the hostname can only be applied on the start event
inline const char *&ethernetHostname()
{
  static const char *hostname = NULL;
  return hostname;
}

inline void onEthernetEvent(arduino_event_id_t event)
{
  if (event == ARDUINO_EVENT_ETH_START)
  {
    ETH.setHostname(ethernetHostname());
  }
}

// starts the w5500, dhcp runs in the background and reconnects on its own after a link loss
inline bool startEthernet(const char *hostname)
{
  ethernetHostname() = hostname; // must outlive the start event, the config buffer does
  Network.onEvent(onEthernetEvent);
  return ETH.begin(ETH_PHY_W5500, ETH_PHY_ADDR_W5500, PIN_ETH_CS, PIN_ETH_INT, PIN_ETH_RST,
                   ETH_SPI_HOST, PIN_ETH_SCK, PIN_ETH_MISO, PIN_ETH_MOSI);
}

inline bool ethernetConnected()
{
  return ETH.linkUp() && ETH.hasIP();
}

// starts ethernet and waits for link and dhcp, false if that does not happen in time
inline bool connectEthernet(const char *hostname)
{
  bool started = startEthernet(hostname);
  for (uint16_t waited = 0; started && waited < NET_CONNECT_TIMEOUT_MS && !ethernetConnected(); waited += NET_CONNECT_POLL_MS)
  {
    delay(NET_CONNECT_POLL_MS);
  }
  return ethernetConnected();
}

// names the ethernet state for the event log
inline const __FlashStringHelper *ethernetStatusLabel()
{
  if (!ETH.linkUp())
  {
    return F("no link");
  }
  return ETH.hasIP() ? F("connected") : F("no ip address");
}
