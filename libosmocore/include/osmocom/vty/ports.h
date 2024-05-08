/*! \file ports.h
 * TCP port numbers used for VTY interfaces in osmocom projects. See also the
 * osmocom wiki as well as the osmo-gsm-manuals, which should all be kept in
 * sync with this file:
 * https://osmocom.org/projects/cellular-infrastructure/wiki/PortNumbers
 * https://git.osmocom.org/osmo-gsm-manuals/tree/common/chapters/port_numbers.adoc
 */

#pragma once

#define OSMO_VTY_PORT_PCAP_CLIENT	4227
#define OSMO_VTY_PORT_PCAP_SERVER	4228
/* 4236 used by control interface */
#define OSMO_VTY_PORT_TRX		4237
/* 4238 used by osmo-bts control interface */
#define OSMO_VTY_PORT_STP		4239
#define OSMO_VTY_PORT_PCU		4240
#define OSMO_VTY_PORT_BTS		4241
#define OSMO_VTY_PORT_NITB_BSC		4242
#define OSMO_VTY_PORT_BSC_MGCP		4243
#define OSMO_VTY_PORT_MGW		OSMO_VTY_PORT_BSC_MGCP
#define OSMO_VTY_PORT_BSC_NAT		4244
#define OSMO_VTY_PORT_SGSN		4245
#define OSMO_VTY_PORT_GBPROXY		4246
#define OSMO_VTY_PORT_BB		4247
/* 4249-4251 used by control interface */
#define OSMO_VTY_PORT_BTSMGR		4252
#define OSMO_VTY_PORT_GTPHUB		4253
#define OSMO_VTY_PORT_MSC		4254
/* 4255 used by control interface */
#define OSMO_VTY_PORT_MNCC_SIP		4256
/* 4257 used by control interface */
#define OSMO_VTY_PORT_HLR		4258
/* 4259 used by control interface */
#define OSMO_VTY_PORT_GGSN		4260
#define OSMO_VTY_PORT_HNBGW		4261
/* 4262-4263 used by control interface */
#define OSMO_VTY_PORT_CBC		4264
#define OSMO_VTY_PORT_UECUPS		4268
#define OSMO_VTY_PORT_E1D		4269
#define OSMO_VTY_PORT_ISDNTAP		4270
#define OSMO_VTY_PORT_SMLC		4271
/* 4272 used by control interface */
#define OSMO_VTY_PORT_HNODEB		4273
/* 4274 used by control interface */
#define OSMO_VTY_PORT_UPF		4275
/* 4276: OSMO_CTRL_PORT_UPF */
#define OSMO_VTY_PORT_PFCP_TOOL		4277
/* 4278: OSMO_CTRL_PORT_UPF */
/* When adding/changing port numbers, keep docs and wiki in sync. See above. */
