/*! \file select.h
 *  select loop abstraction.
 */

#pragma once

#include <osmocom/core/linuxlist.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

/*! \defgroup select Select loop abstraction
 *  @{
 * \file select.h */

/*! Indicate interest in reading from the file descriptor */
#define OSMO_FD_READ	0x0001
/*! Indicate interest in writing to the file descriptor */
#define OSMO_FD_WRITE	0x0002
/*! Indicate interest in exceptions from the file descriptor */
#define OSMO_FD_EXCEPT	0x0004
/*! Used as when_mask in osmo_fd_update_when() */
#define OSMO_FD_MASK	0xFFFF

/* legacy naming dating back to early OpenBSC / bsc_hack of 2008 */
#define BSC_FD_READ	OSMO_FD_READ
#define BSC_FD_WRITE	OSMO_FD_WRITE
#define BSC_FD_EXCEPT	OSMO_FD_EXCEPT

/*! Structure representing a file dsecriptor */
struct osmo_fd {
	/*! linked list for internal management */
	struct llist_head list;	
	/*! actual operating-system level file decriptor */
	int fd;
	/*! bit-mask or of \ref OSMO_FD_READ, \ref OSMO_FD_WRITE and/or
	 * \ref OSMO_FD_EXCEPT */
	unsigned int when;
	/*! call-back function to be called once file descriptor becomes
	 * available */
	int (*cb)(struct osmo_fd *fd, unsigned int what);
	/*! data pointer passed through to call-back function */
	void *data;
	/*! private number, extending \a data */
	unsigned int priv_nr;
};

void osmo_fd_setup(struct osmo_fd *ofd, int fd, unsigned int when,
		   int (*cb)(struct osmo_fd *fd, unsigned int what),
		   void *data, unsigned int priv_nr);

void osmo_fd_update_when(struct osmo_fd *ofd, unsigned int when_mask, unsigned int when);

static inline void osmo_fd_read_enable(struct osmo_fd *ofd) {
	osmo_fd_update_when(ofd, OSMO_FD_MASK, OSMO_FD_READ);
}

static inline void osmo_fd_read_disable(struct osmo_fd *ofd) {
	osmo_fd_update_when(ofd, ~OSMO_FD_READ, 0);
}

static inline void osmo_fd_write_enable(struct osmo_fd *ofd) {
	osmo_fd_update_when(ofd, OSMO_FD_MASK, OSMO_FD_WRITE);
}

static inline void osmo_fd_write_disable(struct osmo_fd *ofd) {
	osmo_fd_update_when(ofd, ~OSMO_FD_WRITE, 0);
}

bool osmo_fd_is_registered(struct osmo_fd *fd);
int osmo_fd_register(struct osmo_fd *fd);
void osmo_fd_unregister(struct osmo_fd *fd);
void osmo_fd_close(struct osmo_fd *fd);
int osmo_select_main(int polling);
int osmo_select_main_ctx(int polling);
void osmo_select_init(void);

struct osmo_fd *osmo_fd_get_by_fd(int fd);

/*
 * foreign event loop integration
 */
int osmo_fd_fill_fds(void *readset, void *writeset, void *exceptset);
int osmo_fd_disp_fds(void *readset, void *writeset, void *exceptset);

/* timerfd integration */
int osmo_timerfd_disable(struct osmo_fd *ofd);
int osmo_timerfd_schedule(struct osmo_fd *ofd, const struct timespec *first,
			  const struct timespec *interval);
int osmo_timerfd_setup(struct osmo_fd *ofd, int (*cb)(struct osmo_fd *, unsigned int), void *data);

/* signalfd integration */
struct osmo_signalfd;
struct signalfd_siginfo;

typedef void osmo_signalfd_cb(struct osmo_signalfd *osfd, const struct signalfd_siginfo *fdsi);

struct osmo_signalfd {
	struct osmo_fd ofd;
	sigset_t sigset;
	osmo_signalfd_cb *cb;
	void *data;
};

struct osmo_signalfd *
osmo_signalfd_setup(void *ctx, sigset_t set, osmo_signalfd_cb *cb, void *data);

void osmo_select_shutdown_request(void);
int osmo_select_shutdown_requested(void);
bool osmo_select_shutdown_done(void);

/*! @} */
