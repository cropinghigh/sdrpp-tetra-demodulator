/*! \file netns.h
 *  Network namespace convenience functions. */

#pragma once
#if (!EMBEDDED)

#if defined(__linux__)

#include <signal.h>

struct osmo_netns_switch_state {
	sigset_t prev_sigmask;
	int prev_nsfd;
};

int osmo_netns_open_fd(const char *name);
int osmo_netns_switch_enter(int nsfd, struct osmo_netns_switch_state *state);
int osmo_netns_switch_exit(struct osmo_netns_switch_state *state);


#endif /* defined(__linux__) */

#endif /* (!EMBEDDED) */
/*! @} */
