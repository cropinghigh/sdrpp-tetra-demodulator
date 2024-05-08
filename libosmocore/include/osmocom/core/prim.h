#pragma once

/*! \defgroup prim Osmocom primitives
 *  @{
 *
 *  Osmocom Primitives are a method to express inter-layer primitives as
 *  used often in ITU/ETSI/3GPP specifications in a generic way. They
 *  are based on \ref msgb and encapsulate any (optional) user payload
 *  data with a primitive header.  The header contains information on
 *  - which SAP this primitive is used on
 *  - what is the name of the primitive
 *  - is it REQUEST, RESPONSE, INDICATION or CONFIRMATION
 *
 *  For more information on the inter-layer primitives concept, see
 *  ITU-T X.21@ as found at https://www.itu.int/rec/T-REC-X.212-199511-I/en
 *
 * \file prim.h */

#include <stdint.h>
#include <osmocom/core/msgb.h>

#define OSMO_PRIM(prim, op)	((prim << 8) | (op & 0xFF))
#define OSMO_PRIM_HDR(oph)	OSMO_PRIM((oph)->primitive, (oph)->operation)

/*! primitive operation */
enum osmo_prim_operation {
	PRIM_OP_REQUEST,	/*!< request */
	PRIM_OP_RESPONSE,	/*!< response */
	PRIM_OP_INDICATION,	/*!< indication */
	PRIM_OP_CONFIRM,	/*!< confirm */
};

extern const struct value_string osmo_prim_op_names[];
static inline const char *osmo_prim_operation_name(enum osmo_prim_operation val)
{
	return get_value_string(osmo_prim_op_names, val);
}

/*!< The upper 8 byte of the technology, the lower 24 bits for the SAP */
#define _SAP_GSM_SHIFT	24

#define _SAP_GSM_BASE	(0x01 << _SAP_GSM_SHIFT)
#define _SAP_TETRA_BASE	(0x02 << _SAP_GSM_SHIFT)
#define _SAP_SS7_BASE	(0x03 << _SAP_GSM_SHIFT)

/*! Osmocom primitive header */
struct osmo_prim_hdr {
	unsigned int sap;	/*!< Service Access Point Identifier */
	unsigned int primitive;	/*!< Primitive number */
	enum osmo_prim_operation operation; /*! Primitive Operation */
	struct msgb *msg;	/*!< \ref msgb containing associated data.
       * Note this can be slightly confusing, as the \ref osmo_prim_hdr
       * is stored inside a \ref msgb, but then it contains a pointer
       * back to the msgb.  This is to simplify development: You can
       * pass around a \ref osmo_prim_hdr by itself, and any function
       * can autonomously resolve the underlying msgb, if needed (e.g.
       * for \ref msgb_free. */
};

/*! Convenience function to initialize a primitive header
 *  \param[in,out] oph primitive header
 *  \param[in] sap Service Access Point
 *  \param[in] primitive Primitive Number
 *  \param[in] operation Primitive Operation (REQ/RESP/IND/CONF)
 *  \param[in] msg Message
 */
static inline void
osmo_prim_init(struct osmo_prim_hdr *oph, unsigned int sap,
		unsigned int primitive, enum osmo_prim_operation operation,
		struct msgb *msg)
{
	oph->sap = sap;
	oph->primitive = primitive;
	oph->operation = operation;
	oph->msg = msg;
}

/*! primitive handler callback type */
typedef int (*osmo_prim_cb)(struct osmo_prim_hdr *oph, void *ctx);

/*! magic value to be used as final record of \ref
 * osmo_prim_event_map */
#define OSMO_NO_EVENT	0xFFFFFFFF

/*! single entry in a SAP/PRIM/OP -> EVENT map */
struct osmo_prim_event_map {
	unsigned int sap;	/*!< SAP to match */
	unsigned int primitive;	/*!< primtiive to match */
	enum osmo_prim_operation operation; /*!< operation to match */
	uint32_t event;		/*!< event as result if above match */
};

uint32_t osmo_event_for_prim(const struct osmo_prim_hdr *oph,
			     const struct osmo_prim_event_map *maps);
/*! @} */
