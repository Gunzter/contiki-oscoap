/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
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
 */

/**
 * \file
 *      CoAP module for observing resources (draft-ietf-core-observe-11).
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <string.h>
#include "er-coaps-observe.h"
#include "er-coaps-engine.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]", (lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3], (lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

/*---------------------------------------------------------------------------*/
MEMB(observers_memb, coaps_observer_t, COAP_MAX_OBSERVERS);
LIST(observers_list);
/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static coaps_observer_t *
add_observer(uip_ipaddr_t *addr, uint16_t port, const uint8_t *token,
             size_t token_len, const char *uri, int uri_len)
{
  /* Remove existing observe relationship, if any. */
  coaps_remove_observer_by_uri(addr, port, uri);

  coaps_observer_t *o = memb_alloc(&observers_memb);

  if(o) {
    int max = sizeof(o->url) - 1;
    if(max > uri_len) {
      max = uri_len;
    }
    memcpy(o->url, uri, max);
    o->url[max] = 0;
    o->ctx = coaps_default_context;
    uip_ipaddr_copy(&o->addr, addr);
    o->port = port;
    o->token_len = token_len;
    memcpy(o->token, token, token_len);
    o->last_mid = 0;

    PRINTF("Adding observer (%u/%u) for /%s [0x%02X%02X]\n",
           list_length(observers_list) + 1, COAP_MAX_OBSERVERS,
           o->url, o->token[0], o->token[1]);
    list_add(observers_list, o);
  }

  return o;
}
/*---------------------------------------------------------------------------*/
/*- Removal -----------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coaps_remove_observer(coaps_observer_t *o)
{
  PRINTF("Removing observer for /%s [0x%02X%02X]\n", o->url, o->token[0],
         o->token[1]);

  memb_free(&observers_memb, o);
  list_remove(observers_list, o);
}
/*---------------------------------------------------------------------------*/
int
coaps_remove_observer_by_client(uip_ipaddr_t *addr, uint16_t port)
{
  int removed = 0;
  coaps_observer_t *obs = NULL;

  for(obs = (coaps_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    PRINTF("Remove check client ");
    PRINT6ADDR(addr);
    PRINTF(":%u\n", port);
    if(uip_ipaddr_cmp(&obs->addr, addr) && obs->port == port) {
      coaps_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*---------------------------------------------------------------------------*/
int
coaps_remove_observer_by_token(uip_ipaddr_t *addr, uint16_t port,
                              uint8_t *token, size_t token_len)
{
  int removed = 0;
  coaps_observer_t *obs = NULL;

  for(obs = (coaps_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    PRINTF("Remove check Token 0x%02X%02X\n", token[0], token[1]);
    if(uip_ipaddr_cmp(&obs->addr, addr) && obs->port == port
       && obs->token_len == token_len
       && memcmp(obs->token, token, token_len) == 0) {
      coaps_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*---------------------------------------------------------------------------*/
int
coaps_remove_observer_by_uri(uip_ipaddr_t *addr, uint16_t port,
                            const char *uri)
{
  int removed = 0;
  coaps_observer_t *obs = NULL;

  for(obs = (coaps_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    PRINTF("Remove check URL %p\n", uri);
    if((addr == NULL
        || (uip_ipaddr_cmp(&obs->addr, addr) && obs->port == port))
       && (obs->url == uri || memcmp(obs->url, uri, strlen(obs->url)) == 0)) {
      coaps_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*---------------------------------------------------------------------------*/
int
coaps_remove_observer_by_mid(uip_ipaddr_t *addr, uint16_t port, uint16_t mid)
{
  int removed = 0;
  coaps_observer_t *obs = NULL;

  for(obs = (coaps_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    PRINTF("Remove check MID %u\n", mid);
    if(uip_ipaddr_cmp(&obs->addr, addr) && obs->port == port
       && obs->last_mid == mid) {
      coaps_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*---------------------------------------------------------------------------*/
/*- Notification ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coaps_notify_observers(resource2_t *resource)
{
#if COAP_CORE_OBSERVE
  coaps_notify_observers_sub(resource, NULL);
#endif
}
void
coaps_notify_observers_sub(resource2_t *resource, const char *subpath)
{
  /* build notification */
  coaps_packet_t notification[1]; /* this way the packet can be treated as pointer as usual */
  coaps_packet_t request[1]; /* this way the packet can be treated as pointer as usual */
  coaps_observer_t *obs = NULL;
  int url_len, obs_url_len;
  char url[COAP_OBSERVER_URL_LEN];

  url_len = strlen(resource->url);
  strncpy(url, resource->url, COAP_OBSERVER_URL_LEN - 1);
  if(url_len < COAP_OBSERVER_URL_LEN - 1 && subpath != NULL) {
    strncpy(&url[url_len], subpath, COAP_OBSERVER_URL_LEN - url_len - 1);
  }
  /* Ensure url is null terminated because strncpy does not guarantee this */
  url[COAP_OBSERVER_URL_LEN - 1] = '\0';
  /* url now contains the notify URL that needs to match the observer */
  PRINTF("Observe: Notification from %s\n", url);

  coaps_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0);
  /* create a "fake" request for the URI */
  coaps_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
  coaps_set_header_uri_path(request, url);

  /* iterate over observers */
  url_len = strlen(url);
  for(obs = (coaps_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    obs_url_len = strlen(obs->url);

    /* Do a match based on the parent/sub-resource match so that it is
       possible to do parent-node observe */
    if((obs_url_len == url_len
        || (obs_url_len > url_len
            && (resource->flags & HAS_SUB_RESOURCES)
            && obs->url[url_len] == '/'))
       && strncmp(url, obs->url, url_len) == 0) {
      coaps_transaction_t *transaction = NULL;

      /*TODO implement special transaction for CON, sharing the same buffer to allow for more observers */

      if((transaction = coaps_new_transaction(coaps_get_mid(), &obs->addr, obs->port))) {
        coaps_set_transaction_context(transaction, obs->ctx);
        if(obs->obs_counter % COAP_OBSERVE_REFRESH_INTERVAL == 0) {
          PRINTF("           Force Confirmable for\n");
          notification->type = COAP_TYPE_CON;
        }

        PRINTF("           Observer ");
        PRINT6ADDR(&obs->addr);
        PRINTF(":%u\n", obs->port);

        /* update last MID for RST matching */
        obs->last_mid = transaction->mid;

        /* prepare response */
        notification->mid = transaction->mid;

        resource->get_handler(request, notification,
                              transaction->packet + COAP_MAX_HEADER_SIZE,
                              REST_MAX_CHUNK_SIZE, NULL);

        if(notification->code < BAD_REQUEST_4_00) {
          coaps_set_header_observe(notification, (obs->obs_counter)++);
        }
        coaps_set_token(notification, obs->token, obs->token_len);

        transaction->packet_len =
          coaps_serialize_message(notification, transaction->packet);

        coaps_send_transaction(transaction);
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
void
coaps_observe_handler(resource2_t *resource, void *request, void *response)
{
#if COAP_CORE_OBSERVE
  coaps_packet_t *const coaps_req = (coaps_packet_t *)request;
  coaps_packet_t *const coaps_res = (coaps_packet_t *)response;
  coaps_observer_t * obs;

  if(coaps_req->code == COAP_GET && coaps_res->code < 128) { /* GET request and response without error code */
    if(IS_OPTION(coaps_req, COAP_OPTION_OBSERVE)) {
      if(coaps_req->observe == 0) {
        obs = add_observer(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport,
                           coaps_req->token, coaps_req->token_len,
                           coaps_req->uri_path, coaps_req->uri_path_len);
       if(obs) {
          coaps_set_header_observe(coaps_res, (obs->obs_counter)++);
          /*
           * Following payload is for demonstration purposes only.
           * A subscription should return the same representation as a normal GET.
           * Uncomment if you want an information about the avaiable observers.
           */
#if 0
          static char content[16];
          coaps_set_payload(coaps_res,
                           content,
                           snprintf(content, sizeof(content), "Added %u/%u",
                                    list_length(observers_list),
                                    COAP_MAX_OBSERVERS));
#endif
        } else {
          coaps_res->code = SERVICE_UNAVAILABLE_5_03;
          coaps_set_payload(coaps_res, "TooManyObservers", 16);
        }
      } else if(coaps_req->observe == 1) {

        /* remove client if it is currently observe */
        coaps_remove_observer_by_token(&UIP_IP_BUF->srcipaddr,
                                      UIP_UDP_BUF->srcport, coaps_req->token,
                                      coaps_req->token_len);
      }
    }
  }
#endif
}
/*---------------------------------------------------------------------------*/