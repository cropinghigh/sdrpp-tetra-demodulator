/* Cryptography related helper, key management and wrapper functions */
/*
 * Copyright (C) 2023 Midnight Blue B.V.
 *
 * Author: Wouter Bokslag <w.bokslag [ ] midnightblue [ ] nl>
 *
 * SPDX-License-Identifier: AGPL-3.0+
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * See the COPYING file in the main directory for details.
 */


#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

// #include <osmocom/core/utils.h>

#include <tetra_mac_pdu.h>
#include <phy/tetra_burst.h>

#include "tetra_crypto.h"
#include "tea1.h"
#include "tea2.h"
#include "tea3.h"
#include "taa1.h"


static const struct value_string tetra_key_types[] = {
	{ KEYTYPE_UNDEFINED,		"UNDEFINED" },
	{ KEYTYPE_CCK_SCK,		"CCK/SCK" },
	{ KEYTYPE_DCK,			"DCK" },
	{ KEYTYPE_MGCK,			"MGCK" },
	{ KEYTYPE_GCK,			"GCK" },
	{ 0, NULL }
};

const char *tetra_get_key_type_name(enum tetra_key_type key_type)
{
	return get_value_string(tetra_key_types, key_type);
}

static const struct value_string tetra_ksg_types[] = {
	{ UNKNOWN,			"UNKNOWN" },
	{ KSG_TEA1,			"TEA1" },
	{ KSG_TEA2,			"TEA2" },
	{ KSG_TEA3,			"TEA3" },
	{ KSG_TEA4,			"TEA4" },
	{ KSG_TEA5,			"TEA5" },
	{ KSG_TEA6,			"TEA6" },
	{ KSG_TEA7,			"TEA7" },
	{ KSG_PROPRIETARY,		"PROPRIETARY" },
	{ 0, NULL }
};

const char *tetra_get_ksg_type_name(enum tetra_ksg_type ksg_type)
{
	if (ksg_type >= KSG_PROPRIETARY)
		return tetra_ksg_types[KSG_PROPRIETARY].str;
	else
		return get_value_string(tetra_ksg_types, ksg_type);
}

static const struct value_string tetra_security_classes[] = {
	{ NETWORK_CLASS_UNDEFINED,	"CLASS_UNDEFINED" },
	{ NETWORK_CLASS_1,		"CLASS_1" },
	{ NETWORK_CLASS_2,		"CLASS_2" },
	{ NETWORK_CLASS_3,		"CLASS_3" },
	{ 0, NULL }
};

const char *tetra_get_security_class_name(uint8_t pdut)
{
	return get_value_string(tetra_security_classes, pdut);
}

void tetra_crypto_state_init(struct tetra_crypto_state *tcs)
{
	memset(tcs, 0, sizeof(*tcs));
	tcs->mnc = UINT32_MAX;
	tcs->mcc = UINT32_MAX;
	tcs->cck_id = UINT32_MAX;
	tcs->hn = -1;
	tcs->la = -1;
	tcs->cn = -1;
	tcs->cc = -1;
}

void tetra_crypto_db_init(struct tetra_crypto_database *db)
{
	memset(db, 0, sizeof(*db));
}

void tetra_crypto_db_clear(struct tetra_crypto_database *db)
{
	if (!db)
		return;
	if (db->keys) {
		memset(db->keys, 0, sizeof(*db->keys) * db->keys_capacity);
		free(db->keys);
	}
	free(db->nets);
	memset(db, 0, sizeof(*db));
}

static void set_error(char *error, size_t error_len, const char *fmt, ...)
{
	va_list ap;
	if (!error || !error_len)
		return;
	va_start(ap, fmt);
	vsnprintf(error, error_len, fmt, ap);
	va_end(ap);
}

static bool supported_ksg(enum tetra_ksg_type type)
{
	return type == KSG_TEA1 || type == KSG_TEA2 || type == KSG_TEA3;
}

static int reserve_array(void **array, uint32_t *capacity, uint32_t count,
			 size_t element_size)
{
	void *next;
	uint32_t capacity_next;
	if (count < *capacity)
		return 0;
	capacity_next = *capacity ? *capacity + TCDB_ALLOC_BLOCK_SIZE : TCDB_ALLOC_BLOCK_SIZE;
	next = realloc(*array, element_size * capacity_next);
	if (!next)
		return -1;
	memset((char *)next + element_size * *capacity, 0,
	       element_size * (capacity_next - *capacity));
	*array = next;
	*capacity = capacity_next;
	return 0;
}

char *dump_key(struct tetra_key *k)
{
	static char pbuf[1024];

	int c = snprintf(pbuf, sizeof(pbuf), "MCC %4d MNC %4d key_type %s",
		k->mcc, k->mnc, tetra_get_key_type_name(k->key_type));

	if (k->key_type & (KEYTYPE_DCK | KEYTYPE_MGCK))
		c += snprintf(pbuf + c, sizeof(pbuf) - c, " addr: %8d", k->addr);

	if (k->key_type & (KEYTYPE_CCK_SCK))
		c += snprintf(pbuf + c, sizeof(pbuf) - c, " key_num: %4d", k->key_num);

	c += snprintf(pbuf + c, sizeof(pbuf) - c, ": ");
	for (int i = 0; i < 10; i++)
		snprintf(pbuf + c + 2*i, sizeof(pbuf)-c-2*i, "%02X", k->key[i]);

	return pbuf;
}

char *dump_network_info(struct tetra_netinfo *network)
{
	static char pbuf[1024];
	snprintf(pbuf, sizeof(pbuf), "MCC %4d MNC %4d ksg_type %d security_class %d", network->mcc, network->mnc, network->ksg_type, network->security_class);
	return pbuf;
}

uint32_t tea_build_iv(struct tetra_tdma_time *tm, uint16_t hn, uint8_t dir)
{
	assert(1 <= tm->tn  && tm->tn  <= 4);
	assert(1 <= tm->fn  && tm->fn  <= 18);
	assert(1 <= tm->mn  && tm->mn  <= 60);
	assert(dir <= 1); /* 0 = downlink, 1 = uplink */
	return ((tm->tn - 1) | (tm->fn << 2) | (tm->mn << 7) | ((hn & 0x7FFF) << 13) | (dir << 28));
}

static bool generate_keystream(struct tetra_crypto_state *tcs, struct tetra_key *key, struct tetra_tdma_time *t, int num_bits, uint8_t *ks_out)
{
	if (!key)
		return false;

	/* Construct IV and prepare buf for bytewise keystream */
	int num_bytes = (num_bits + 7) / 8;
	// uint8_t ks_bytes[num_bytes];
	//I HATE FUCKING MSVC
	uint8_t* ks_bytes = malloc(num_bytes);
	if (!ks_bytes || tcs->hn < 0) {
		free(ks_bytes);
		return false;
	}
	uint32_t iv = tea_build_iv(t, tcs->hn, 0);

	/* Compute ECK from net info and CK */
	if (tcs->cn < 0 || tcs->la < 0 || tcs->cc < 0) {
		/* Missing data for TB5 */
		free(ks_bytes);
		return false;
	}

	uint8_t eck[10];
	uint8_t cn[2] = {(tcs->cn >> 8) & 0xFF, tcs->cn & 0xFF};
	uint8_t la[2] = {(tcs->la >> 8) & 0xFF, tcs->la & 0xFF};
	uint8_t cc[1] = {tcs->cc & 0xFF};
	tb5(cn, la, cc, key->key, eck);

	/* Generate keystream with required KSG */
	switch (key->network_info->ksg_type) {
	case KSG_TEA1:
		tea1(iv, eck, num_bytes, ks_bytes);
		break;

	case KSG_TEA2:
		tea2(iv, eck, num_bytes, ks_bytes);
		break;

	case KSG_TEA3:
		tea3(iv, eck, num_bytes, ks_bytes);
			break;

	default:
		// fprintf(stderr, "tetra_crypto: KSG type %d not supported\n", key->network_info->ksg_type);
		free(ks_bytes);
		return false;
	}

	/* Expand keystream bytes into ubit format */
	for (int i = 0; i < num_bits; i++)
		ks_out[i] = (ks_bytes[i / 8] >> (7-(i % 8))) & 1;

	free(ks_bytes);
	return true;
}

bool decrypt_identity(struct tetra_crypto_state *tcs, struct tetra_addr *addr)
{
	(void)tcs;
	(void)addr;
	/* TODO FIXME implement TA61 decryption */
	return false;
}

bool decrypt_mac_element(struct tetra_crypto_state *tcs, struct tetra_tmvsap_prim *tmvp, struct tetra_key *key, int l1_len, int tmpdu_offset)
{
	if (!tcs || !tmvp || !key || tmpdu_offset < 0 || l1_len <= 0 ||
	    tmpdu_offset >= l1_len)
		return false;

	if (tcs->cn < 0 || tcs->la < 0 || tcs->cc < 0) {
		// printf("tetra_crypto: can't compute TB5 due to incomplete network info (carr %d la %d cc %d)\n",
			// tcs->cn, tcs->la, tcs->cc);
		return false;
	}

	struct tetra_tdma_time *tdma_time = &tmvp->u.unitdata.tdma_time;

	/* Compute keystream offset to apply */
	/* TODO FIXME maybe we can rework channel type setting in lower mac, and
	   avoid using TETRA_LC_UNKNOWN below */
	uint32_t ks_skip_bits = 0;
	if (tmvp->u.unitdata.blk_num == BLK_2 && (
			tmvp->u.unitdata.lchan == TETRA_LC_SCH_HD ||
			tmvp->u.unitdata.lchan == TETRA_LC_UNKNOWN)) {
		ks_skip_bits = 216;
		// printf("tetra_crypto: 2nd half slot; skipping bits\n");
	}

	/* Generate keystream */
	struct msgb *msg = tmvp->oph.msg;
	
	int ct_len = l1_len - tmpdu_offset;
	int ks_num_bits = ks_skip_bits + ct_len;
	uint8_t *ct_start = msg->l1h + tmpdu_offset;
	// uint8_t *ct_start = tmvp->msg + tmpdu_offset;
	// uint8_t ks[ks_num_bits];
	uint8_t* ks = malloc(ks_num_bits);
	if (!ks)
		return false;
	if (!generate_keystream(tcs, key, tdma_time, ks_num_bits, ks)) {
		free(ks);
		return false;
	}

	/* Apply keystream */
	for (int i = 0; i < ct_len; i++)
		ct_start[i] = ct_start[i] ^ ks[i + ks_skip_bits];

	// printf("tetra_crypto: addr %8d -> key %4d, time %5d/%s, tmpdu offset %d, decrypting %d bits\n",
		// key->addr, key->index, tcs->hn, tetra_tdma_time_dump(tdma_time), tmpdu_offset, ct_len);

	free(ks);
	return true;
}

bool decrypt_voice_timeslot(struct tetra_crypto_state *tcs, struct tetra_tdma_time *tdma_time, int16_t *type1_block)
{
	/* TODO FIXME implement proper key selection for voice blocks */
	struct tetra_key *key = tcs->cck;
	if (!key)
		return false;

	if (tcs->cn < 0 || tcs->la < 0 || tcs->cc < 0) {
		// printf("tetra_crypto: can't compute TB5 due to incomplete network info (carr %d la %d cc %d)\n",
			// tcs->cn, tcs->la, tcs->cc);
		return false;
	}

	/* Generate keystream */
	int ks_num_bits = 137*2; // two half slots of voice
	// uint8_t ks[ks_num_bits];
	uint8_t* ks = malloc(ks_num_bits);
	if (!ks)
		return false;
	if (!generate_keystream(tcs, key, tdma_time, ks_num_bits, ks)) {
		free(ks);
		return false;
	}

	/* Apply keystream */
	for (int i = 0; i < 137; i++) {
		if (ks[i]) {
			int16_t *bit = &type1_block[i + 1];
			/* Channel_Decoding may emit either hard 0/1 bits or ETSI
			 * soft decisions. XOR the former and reverse the latter. */
			*bit = (*bit == 0 || *bit == 1) ? (*bit ^ 1) : -*bit;
		}
		if (ks[i + 137]) {
			int16_t *bit = &type1_block[i + 139];
			*bit = (*bit == 0 || *bit == 1) ? (*bit ^ 1) : -*bit;
		}
	}

	// printf("tetra_crypto: addr %8d -> key %4d, time %5d/%s, decrypted voice\n",
		// key->addr, key->index, tcs->hn, tetra_tdma_time_dump(tdma_time));
	free(ks);
	return true;
}

static int parse_hex_key(const char *text, uint8_t key[16])
{
	if (strlen(text) != 20)
		return -1;
	memset(key, 0, 16);
	for (unsigned int i = 0; i < 10; i++) {
		char byte_text[3] = { text[i * 2], text[i * 2 + 1], 0 };
		char *end;
		unsigned long value;
		if (!isxdigit((unsigned char)byte_text[0]) ||
		    !isxdigit((unsigned char)byte_text[1]))
			return -1;
		value = strtoul(byte_text, &end, 16);
		if (*end || value > 0xff)
			return -1;
		key[i] = (uint8_t)value;
	}
	return 0;
}

struct tetra_netinfo *get_network_info(struct tetra_crypto_database *db,
				       uint32_t mcc, uint32_t mnc)
{
	if (!db)
		return NULL;
	for (uint32_t i = 0; i < db->num_nets; i++)
		if (db->nets[i].mcc == mcc && db->nets[i].mnc == mnc)
			return &db->nets[i];
	return NULL;
}

static int link_keys(struct tetra_crypto_database *db, char *error, size_t error_len)
{
	for (uint32_t i = 0; i < db->num_keys; i++) {
		db->keys[i].index = i;
		db->keys[i].network_info = get_network_info(db, db->keys[i].mcc,
							      db->keys[i].mnc);
		if (!db->keys[i].network_info) {
			set_error(error, error_len, "key %u has no matching network", i + 1);
			return -1;
		}
	}
	return 0;
}

int tetra_crypto_db_load(struct tetra_crypto_database *db, const char *filename,
			 char *error, size_t error_len)
{
	struct tetra_crypto_database next;
	FILE *fp;
	char buf[1000];
	unsigned int line = 0;

	if (!db || !filename || !filename[0]) {
		set_error(error, error_len, "no key-file path supplied");
		return -1;
	}
	tetra_crypto_db_init(&next);
	fp = fopen(filename, "r");
	if (!fp) {
		set_error(error, error_len, "cannot open key file: %s", strerror(errno));
		return -1;
	}
	while (fgets(buf, sizeof(buf), fp)) {
		char kind[16], hex[64], extra;
		uint32_t a, b, c, d, e;
		line++;
		if (!strchr(buf, '\n') && !feof(fp)) {
			set_error(error, error_len, "line %u is too long", line);
			goto fail;
		}
		char *cur = buf;
		while (isspace((unsigned char)*cur)) cur++;
		if (!*cur || *cur == '#') continue;
		if (sscanf(cur, "%15s", kind) != 1) continue;
		if (!strcmp(kind, "network")) {
			if (sscanf(cur, "network mcc %u mnc %u ksg_type %u security_class %u %c",
				   &a, &b, &c, &d, &extra) != 4 || !supported_ksg((enum tetra_ksg_type)c) ||
			    d < NETWORK_CLASS_1 || d > NETWORK_CLASS_3) {
				set_error(error, error_len, "invalid network definition on line %u", line);
				goto fail;
			}
			if (reserve_array((void **)&next.nets, &next.nets_capacity,
					  next.num_nets, sizeof(*next.nets))) {
				set_error(error, error_len, "out of memory on line %u", line);
				goto fail;
			}
			next.nets[next.num_nets++] = (struct tetra_netinfo){a, b, (enum tetra_ksg_type)c,
									(enum tetra_security_class)d};
		} else if (!strcmp(kind, "key")) {
			if (sscanf(cur, "key mcc %u mnc %u addr %u key_type %u key_num %u key %63s %c",
				   &a, &b, &c, &d, &e, hex, &extra) != 6 || d != KEYTYPE_CCK_SCK) {
				set_error(error, error_len, "invalid key definition on line %u", line);
				goto fail;
			}
			if (reserve_array((void **)&next.keys, &next.keys_capacity,
					  next.num_keys, sizeof(*next.keys))) {
				set_error(error, error_len, "out of memory on line %u", line);
				goto fail;
			}
			struct tetra_key *key = &next.keys[next.num_keys];
			*key = (struct tetra_key){0};
			key->mcc = a; key->mnc = b; key->addr = c;
			key->key_type = (enum tetra_key_type)d; key->key_num = e;
			if (parse_hex_key(hex, key->key)) {
				set_error(error, error_len, "key on line %u must contain exactly 20 hex digits", line);
				goto fail;
			}
			next.num_keys++;
		} else {
			set_error(error, error_len, "unknown entry on line %u", line);
			goto fail;
		}
	}
	if (ferror(fp)) {
		set_error(error, error_len, "error reading key file");
		goto fail;
	}
	if (link_keys(&next, error, error_len)) goto fail;
	fclose(fp);
	tetra_crypto_db_clear(db);
	*db = next;
	if (error && error_len) error[0] = 0;
	return 0;
fail:
	fclose(fp);
	tetra_crypto_db_clear(&next);
	return -1;
}

int tetra_crypto_db_add_or_replace(struct tetra_crypto_database *db,
				   const struct tetra_netinfo *network,
				   const struct tetra_key *key,
				   char *error, size_t error_len)
{
	uint32_t ni, ki;
	if (!db || !network || !key || !supported_ksg(network->ksg_type) ||
	    key->key_type != KEYTYPE_CCK_SCK || key->mcc != network->mcc ||
	    key->mnc != network->mnc) {
		set_error(error, error_len, "invalid or unsupported network/key candidate");
		return -1;
	}
	for (ni = 0; ni < db->num_nets; ni++)
		if (db->nets[ni].mcc == network->mcc && db->nets[ni].mnc == network->mnc) break;
	if (ni == db->num_nets) {
		if (reserve_array((void **)&db->nets, &db->nets_capacity, ni, sizeof(*db->nets))) goto oom;
		db->num_nets++;
	}
	db->nets[ni] = *network;
	for (ki = 0; ki < db->num_keys; ki++)
		if (db->keys[ki].mcc == key->mcc && db->keys[ki].mnc == key->mnc &&
		    db->keys[ki].key_type == key->key_type && db->keys[ki].key_num == key->key_num &&
		    db->keys[ki].addr == key->addr) break;
	if (ki == db->num_keys) {
		if (reserve_array((void **)&db->keys, &db->keys_capacity, ki, sizeof(*db->keys))) goto oom;
		db->num_keys++;
	}
	db->keys[ki] = *key;
	return link_keys(db, error, error_len);
oom:
	/* A successful network realloc may have moved storage before a later key
	 * allocation failed. Restore all convenience pointers before returning. */
	(void)link_keys(db, NULL, 0);
	set_error(error, error_len, "out of memory adding key candidate");
	return -1;
}

struct tetra_key *get_key_by_addr(struct tetra_crypto_state *tcs, uint32_t addr, enum tetra_key_type key_type)
{
	for (unsigned int i = 0; tcs->db && i < tcs->db->num_keys; i++) {
		struct tetra_key *key = &tcs->db->keys[i];
		if (key->mnc == tcs->mnc &&
				key->mcc == tcs->mcc &&
				key->addr == addr &&
				(key->key_type & key_type)) {
			return key;
		}
	}
	return 0;
}

struct tetra_key *get_ksg_key(struct tetra_crypto_state *tcs, int addr)
{
	(void)addr;
	/* TETRA standard part 7 Clause 6.2:
	--------------------------------------------
			Auth	Encr	GCK	DCK
	Class 1:	?	-	-	-
	Class 2:	?	+	?	-
	Class 3:	+	+	?	+
	--------------------------------------------
	*/

	if (!tcs->network)
		/* No tetra_netinfo from the db set for this network */
		return 0;

	/* FIXME: add support for ISSI/GSSI range definitions and GCK bindings */
	/* FIXME: add support for ISSI-bound DCK keys in class 3 networks */

	return tcs->cck;
}

void update_current_network(struct tetra_crypto_state *tcs, int mcc, int mnc)
{
	/* Update globals */
	tcs->mcc = mcc;
	tcs->mnc = mnc;

	/* Network changed, update reference to current network */
	tcs->network = 0;
	for (unsigned int i = 0; tcs->db && i < tcs->db->num_nets; i++) {
		struct tetra_netinfo *network = &tcs->db->nets[i];
		if (network->mnc == tcs->mnc && network->mcc == tcs->mcc) {
			tcs->network = network;
			break;
		}
	}

	/* (Try to) select new CCK/SCK */
	update_current_cck(tcs);
}

void update_current_cck(struct tetra_crypto_state *tcs)
{
	// printf("\ntetra_crypto: update_current_cck invoked cck %d mcc %d mnc %d\n", tcs->cck_id, tcs->mcc, tcs->mnc);
	tcs->cck = 0;

	for (unsigned int i = 0; tcs->db && i < tcs->db->num_keys; i++) {
		struct tetra_key *key = &tcs->db->keys[i];
		/* TODO FIXME consider selecting CCK or SCK key type based on network config */
		if (key->mcc == tcs->mcc && key->mnc == tcs->mnc && key->key_num == tcs->cck_id) {
			if (key->key_type == KEYTYPE_CCK_SCK) {
				tcs->cck = key;
				// printf("tetra_crypto: Set new current_cck %d (type: full)\n", i);
				break;
			}
		}
	}
}

void tetra_crypto_refresh(struct tetra_crypto_state *tcs)
{
	update_current_network(tcs, tcs->mcc, tcs->mnc);
}
