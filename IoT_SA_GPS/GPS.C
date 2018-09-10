/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "dev/leds.h"

#include <string.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define MAX_PAYLOAD_LEN 120

static struct ctimer reply_ctimer;
static struct ctimer inc_ctimer;
static struct ctimer led_ctimer;
static struct uip_udp_conn *server_conn;
uint32_t localutctim = 0;
int sync_flag = 0;
int counter = 0;

//static struct uip_udp_conn *reply_server_conn;

PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&resolv_process,&udp_server_process);
/*---------------------------------------------------------------------------*/
void updateUtcTime(uint32_t utctime)
{
    localutctim = utctime;
    sync_flag = 1;
    ctimer_set(&reply_ctimer, CLOCK_SECOND*5, reply_mechanism, NULL);
    ctimer_set(&inc_ctimer, CLOCK_SECOND*1 , incUtcTime,NULL);
    ctimer_stop(&led_ctimer);
    ctimer_set(&led_ctimer, CLOCK_SECOND*0.01, led_mechanism, NULL);
} 

void incUtcTime()
{
    localutctim++;
    ctimer_reset(&inc_ctimer);
}

uint32_t getUtcTimeFromLocalTime()
{
        return localutctim;
}

void reply_mechanism()
{
    if(localutctim != 0){
        uint32_t time = getUtcTimeFromLocalTime();
        PRINTF("Responding with message: %u\r\n",(unsigned int)time);

        uint16_t replyPort = 47371;
        uip_ipaddr_t ipaddr;
        uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
        uip_udp_packet_sendto(server_conn, &time, 4,&ipaddr ,UIP_HTONS(replyPort));
    }
    ctimer_reset(&reply_ctimer);    
}

void led_mechanism()
{
    if (sync_flag == 1) {   
        if(getUtcTimeFromLocalTime() % 2 == 1){
            leds_on(LEDS_ALL);
        }else{
            leds_on(LEDS_ALL);
        }else{
            leds_off(LEDS_ALL);
        }
    } else {
        if (counter % 100 == 0) {
            leds_toggle(LEDS_ALL);
            counter = 0;
        }
        counter ++;
        
    }
    ctimer_reset(&led_ctimer);
}

static void
tcpip_handler(void)
{
  if(uip_newdata()) {
    if(sync_flag == 0){
        ((char *)uip_appdata)[uip_datalen()] = 0;
        uint32_t *time = (uint32_t *)uip_appdata;
        updateUtcTime(*time);

        PRINTF("Server received: '%d' (RSSI: %d) from ",(unsigned int)*time, (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI));
        PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
        PRINTF("\n\r");
        uip_ipaddr_copy(&server_conn->ripaddr, &UIP_IP_BUF->srcipaddr);  
    }else{

    }
    
    
    /* Restore server connection to allow data from any node */
    memset(&server_conn->ripaddr, 0, sizeof(server_conn->ripaddr));

  }
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n\r");
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
#if UIP_CONF_ROUTER
  uip_ipaddr_t ipaddr;
#endif /* UIP_CONF_ROUTER */

  PROCESS_BEGIN();
  PRINTF("UDP server started\n\r");

#if RESOLV_CONF_SUPPORTS_MDNS
  resolv_set_hostname("contiki-udp-server");
#endif

#if UIP_CONF_ROUTER
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
#endif /* UIP_CONF_ROUTER */

  print_local_addresses();

  //Create UDP socket and bind to port 3000
  //server_conn = udp_new(NULL, UIP_HTONS(3001), NULL);
  server_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  udp_bind(server_conn, UIP_HTONS(3000));
  leds_off(LEDS_ALL);
  ctimer_set(&led_ctimer, CLOCK_SECOND*0.01, led_mechanism, NULL);
  //reply_server_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  //udp_connect(reply_server_conn, UIP_HTONS(7005));
  while(1) {
    PROCESS_YIELD();

        //Wait for tcipip event to occur
    if(ev == tcpip_event) {
      tcpip_handler();
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/






