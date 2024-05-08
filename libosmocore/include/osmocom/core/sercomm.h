/*! \file sercomm.h
 *  Osmocom Sercomm HDLC (de)multiplex.
 */

#pragma once

#include <osmocom/core/msgb.h>

/*! \defgroup sercomm Seriall Communications (HDLC)
 *  @{
 * \file sercomm.h */

/*! A low sercomm_dlci means high priority.  A high DLCI means low priority */
enum sercomm_dlci {
	SC_DLCI_HIGHEST = 0,
	SC_DLCI_DEBUG   = 4,
	SC_DLCI_L1A_L23 = 5,
	SC_DLCI_LOADER  = 9,
	SC_DLCI_CONSOLE = 10,
	SC_DLCI_ECHO    = 128,
	_SC_DLCI_MAX
};

struct osmo_sercomm_inst;
/*! call-back function for per-DLC receive handler
 *  \param[in] sercomm instance on which msg was received
 *  \param[in] dlci DLC Identifier of received msg
 *  \param[in] msg received message that needs to be processed */
typedef void (*dlci_cb_t)(struct osmo_sercomm_inst *sercomm, uint8_t dlci, struct msgb *msg);

/*! one instance of a sercomm multiplex/demultiplex */
struct osmo_sercomm_inst {
	/*! Has this instance been initialized? */
	int initialized;
	/*! UART Identifier */
	int uart_id;

	/*! transmit side */
	struct {
		/*! per-DLC queue of pending transmit msgbs */
		struct llist_head dlci_queues[_SC_DLCI_MAX];
		/*! msgb currently being transmitted */
		struct msgb *msg;
		/*! transmit state */
		int state;
		/*! next to-be-transmitted char in msg */
		uint8_t *next_char;
	} tx;

	/*! receive side */
	struct {
		/*! per-DLC handler call-back functions */
		dlci_cb_t dlci_handler[_SC_DLCI_MAX];
		/*! msgb allocation size for rx msgs */
		unsigned int msg_size;
		/*! currently received msgb */
		struct msgb *msg;
		/*! receive state */
		int state;
		/*! DLCI of currently received msgb */
		uint8_t dlci;
		/*! CTRL of currently received msgb */
		uint8_t ctrl;
	} rx;
};


void osmo_sercomm_init(struct osmo_sercomm_inst *sercomm);
int osmo_sercomm_initialized(struct osmo_sercomm_inst *sercomm);

/* User Interface: Tx */
void osmo_sercomm_sendmsg(struct osmo_sercomm_inst *sercomm, uint8_t dlci, struct msgb *msg);
unsigned int osmo_sercomm_tx_queue_depth(struct osmo_sercomm_inst *sercomm, uint8_t dlci);

/* User Interface: Rx */
int osmo_sercomm_register_rx_cb(struct osmo_sercomm_inst *sercomm, uint8_t dlci, dlci_cb_t cb);

int osmo_sercomm_change_speed(struct osmo_sercomm_inst *sercomm, uint32_t bdrt);

/* Driver Interface */

int osmo_sercomm_drv_pull(struct osmo_sercomm_inst *sercomm, uint8_t *ch);
int osmo_sercomm_drv_rx_char(struct osmo_sercomm_inst *sercomm, uint8_t ch);

extern void sercomm_drv_lock(unsigned long *flags);
extern void sercomm_drv_unlock(unsigned long *flags);

/*! low-level driver routine to request start of transmission
 *  The Sercomm code calls this function to inform the low-level driver
 *  that some data is pending for transmission, and the low-level driver
 *  should (if not active already) start enabling tx_empty interrupts
 *  and pull drivers out of sercomm using osmo_sercomm_drv_pull() until
 *  the latter returns 0.
 *  \param[in] sercomm Osmocom sercomm instance for which to change
 */
extern void sercomm_drv_start_tx(struct osmo_sercomm_inst *sercomm);

/*! low-level driver routine to execute baud-rate change
 *  \param[in] sercomm Osmocom sercomm instance for which to change
 *  \param[in] bdrt New Baud-Rate (integer)
 *  \returns 0 on success; negative in case of error
 */
extern int sercomm_drv_baudrate_chg(struct osmo_sercomm_inst *sercomm, uint32_t bdrt);

/*! Sercomm msgb allocator function */
static inline struct msgb *osmo_sercomm_alloc_msgb(unsigned int len)
{
	return msgb_alloc_headroom(len+4, 4, "sercomm_tx");
}

/*! @} */
