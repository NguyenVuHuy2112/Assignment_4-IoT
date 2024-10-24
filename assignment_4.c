#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"

#include <stdio.h>
#include <string.h>

#define MAX_NEIGHBORS 5 // Save max neighbors

typedef struct {
  linkaddr_t addr;
  int8_t rssi;
  int rx_count;
  int tx_count;
  int prr; // Packet reception rate
  int rx_counter; // For pruning inactive nodes
} neighbor_t;

static neighbor_t neighbors[MAX_NEIGHBORS];
static int num_neighbors = 0;

static struct etimer beacon_timer, prune_timer;

static void update_neighbor(const linkaddr_t *from, int8_t rssi);
static void sort_neighbors_by_rssi();
static void prune_neighbors();
static void print_neighbors();

/*---------------------------------------------------------------------------*/
PROCESS(example_broadcast_process, "Broadcast example");
AUTOSTART_PROCESSES(&example_broadcast_process);
/*---------------------------------------------------------------------------*/

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  int8_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  printf("Broadcast message received from %d: RSSI=%d\n",
         from->u8[0], rssi);

  update_neighbor(from, rssi);
  sort_neighbors_by_rssi();
}

/* Callback broadcast */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

// Update information of node neighbor
static void update_neighbor(const linkaddr_t *from, int8_t rssi) {
  int i;
  /* Check if node is already in the table or not */
  for(i = 0; i < num_neighbors; i++) {
    if(linkaddr_cmp(&neighbors[i].addr, from)) {
      neighbors[i].rssi = rssi;
      neighbors[i].rx_count++;
      neighbors[i].rx_counter = 3; // Reset the RX_counter
      // Update PRR based on received and sent packets
      if(neighbors[i].tx_count > 0) {
        neighbors[i].prr = (neighbors[i].rx_count * 100) / neighbors[i].tx_count;
      }
      return;
    }
  }

  if(num_neighbors < MAX_NEIGHBORS) {
    linkaddr_copy(&neighbors[num_neighbors].addr, from);
    neighbors[num_neighbors].rssi = rssi;
    neighbors[num_neighbors].rx_count = 1;
    neighbors[num_neighbors].tx_count = 0; // Initialize TX count to 0
    neighbors[num_neighbors].prr = 0;
    neighbors[num_neighbors].rx_counter = 3; // Initialize RX_counter to 3
    num_neighbors++;
  }
}

/* Arrange table neighbor with RSSI decrease */
static void sort_neighbors_by_rssi() {
  int i, j;
  for(i = 0; i < num_neighbors - 1; i++) {
    for(j = i + 1; j < num_neighbors; j++) {
      if(neighbors[i].rssi < neighbors[j].rssi) {
        neighbor_t temp = neighbors[i];
        neighbors[i] = neighbors[j];
        neighbors[j] = temp;
      }
    }
  }

  // Keep only the top MAX_NEIGHBORS neighbors
  if(num_neighbors > MAX_NEIGHBORS) {
    num_neighbors = MAX_NEIGHBORS;
  }
}

/* Decrease RX_counter of nodes after 16s */
static void prune_neighbors() {
  int i;
  for(i = 0; i < num_neighbors; i++) {
    neighbors[i].rx_counter--;
    if(neighbors[i].rx_counter <= 0) {
      /* Flag node as disconnected */
      printf("Connection with node %d.%d has been disconnected\n",
             neighbors[i].addr.u8[0], neighbors[i].addr.u8[1]);
      /* Delete node if RX_counter <= 0 */
      neighbors[i] = neighbors[num_neighbors - 1];
      num_neighbors--;
      i--;
    }
  }
}

/* Print table */
static void print_neighbors() {
  int i;
  printf("Neighbor Table:\n");
  printf("Addr    RSSI      PRR   RX_Counter\n");
  for(i = 0; i < num_neighbors; i++) {
    printf("%d    %d           %d      %d\n",
           neighbors[i].addr.u8[0],
           neighbors[i].rssi,  neighbors[i].prr,
           neighbors[i].rx_counter);
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_broadcast_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_call);

  /* Cài đặt timer */
  etimer_set(&beacon_timer, CLOCK_SECOND * 8);
  etimer_set(&prune_timer, CLOCK_SECOND * 16);

  while(1) {
    PROCESS_WAIT_EVENT();

    if(etimer_expired(&beacon_timer)) {
      /* Gửi beacon */
      packetbuf_copyfrom("Hello", 6);
      broadcast_send(&broadcast);
      printf("Beacon message sent\n");

      /* Tăng TX count của mỗi node */
      int i;
      for(i = 0; i < num_neighbors; i++) {
        neighbors[i].tx_count++;
      }

      /* In ra bảng neighbor */
      print_neighbors();

      /* Đặt lại timer cho beacon */
      etimer_reset(&beacon_timer);
    }

    if(etimer_expired(&prune_timer)) {
      /* Thực hiện prune */
      prune_neighbors();
      etimer_reset(&prune_timer);
    }
  }

  PROCESS_END();
}
