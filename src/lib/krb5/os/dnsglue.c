/*
 * lib/krb5/os/dnsglue.c
 *
 * Copyright 2004 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 */
#ifdef KRB5_DNS_LOOKUP

#include "dnsglue.h"

/*
 * Opaque handle
 */
struct krb5int_dns_state {
    int nclass;
    int ntype;
    void *ansp;
    int anslen;
    int ansmax;
#if HAVE_RES_NSEARCH
    int cur_ans;
    ns_msg msg;
#else
    unsigned char *ptr;
    unsigned short nanswers;
#endif
};

#if !HAVE_RES_NSEARCH
static int initparse(struct krb5int_dns_state *);
#endif

/*
 * krb5int_dns_init()
 *
 * Initialize an opaue handl.  Do name lookup and initial parsing of
 * reply, skipping question section.  Prepare to iterate over answer
 * section.  Returns -1 on error, 0 on success.
 */
int
krb5int_dns_init(struct krb5int_dns_state **dsp,
		 char *host, int nclass, int ntype)
{
#if HAVE_RES_NSEARCH
    struct __res_state statbuf;
#endif
    struct krb5int_dns_state *ds;
    int len;
    size_t nextincr, maxincr;
    unsigned char *p;

    *dsp = ds = malloc(sizeof(*ds));
    if (ds == NULL)
	return -1;

    ds->nclass = nclass;
    ds->ntype = ntype;
    ds->ansp = NULL;
    ds->anslen = 0;
    ds->ansmax = 0;
    nextincr = 2048;
    maxincr = INT_MAX;

#if HAVE_RES_NSEARCH
    ds->cur_ans = 0;
    len = res_ninit(&statbuf);
    if (len < 0)
	return -1;
#endif

    do {
	p = (ds->ansp == NULL)
	    ? malloc(nextincr) : realloc(ds->ansp, nextincr);

	if (p == NULL && ds->ansp != NULL) {
	    free(ds->ansp);
	    return -1;
	}
	ds->ansp = p;
	ds->ansmax = nextincr;

#if HAVE_RES_NSEARCH
	len = res_nsearch(&statbuf, host, ds->nclass, ds->ntype,
			  ds->ansp, ds->ansmax);
#else
	len = res_search(host, ds->nclass, ds->ntype,
			 ds->ansp, ds->ansmax);
#endif
	if (len > maxincr)
	    return -1;
	while (nextincr < len)
	    nextincr *= 2;
	if (len < 0 || nextincr > maxincr) {
	    free(ds->ansp);
	    return -1;
	}
    } while (len > ds->ansmax);

    ds->anslen = len;
#if HAVE_RES_NSEARCH
    len = ns_initparse(ds->ansp, ds->anslen, &ds->msg);
#else
    len = initparse(ds);
#endif
    if (len < 0) {
	free(ds->ansp);
	return -1;
    }

    return 0;
}

#if HAVE_RES_NSEARCH
/*
 * krb5int_dns_nextans - get next matching answer record
 *
 * Sets pp to NULL if no more records.  Returns -1 on error, 0 on
 * success.
 */
int
krb5int_dns_nextans(struct krb5int_dns_state *ds,
		    const unsigned char **pp, int *lenp)
{
    int len;
    ns_rr rr;

    *pp = NULL;
    *lenp = 0;
    while (ds->cur_ans < ns_msg_count(ds->msg, ns_s_an)) {
	len = ns_parserr(&ds->msg, ns_s_an, ds->cur_ans, &rr);
	if (len < 0)
	    return -1;
	ds->cur_ans++;
	if (ds->nclass == ns_rr_class(rr)
	    && ds->ntype == ns_rr_type(rr)) {
	    *pp = ns_rr_rdata(rr);
	    *lenp = ns_rr_rdlen(rr);
	    return 0;
	}
    }
    return 0;
}
#endif

/*
 * krb5int_dns_expand - wrapper for dn_expand()
 */
int krb5int_dns_expand(struct krb5int_dns_state *ds,
		       const unsigned char *p,
		       char *buf, int len)
{

#if HAVE_NS_NAME_UNCOMPRESS
    return ns_name_uncompress(ds->ansp,
			      (unsigned char *)ds->ansp + ds->anslen,
			      p, buf, (size_t)len);
#else
    return dn_expand(ds->ansp,
		     (unsigned char *)ds->ansp + ds->anslen,
		     p, buf, len);
#endif
}

/*
 * Free stuff.
 */
void
krb5int_dns_fini(struct krb5int_dns_state *ds)
{
    if (ds->ansp != NULL)
	free(ds->ansp);
    if (ds != NULL)
	free(ds);
}

/*
 * Compat routines for BIND 4
 */
#if !HAVE_RES_NSEARCH

/*
 * initparse
 *
 * Skip header and question section of reply.  Set a pointer to the
 * beginning of the answer section, and prepare to iterate over
 * answer records.
 */
static int
initparse(struct krb5int_dns_state *ds)
{
    HEADER *hdr;
    unsigned char *p;
    unsigned short nqueries, nanswers;
    int len;
#if !HAVE_DN_SKIPNAME
    char host[MAXDNAME];
#endif

    if (ds->anslen < sizeof(HEADER))
	return -1;

    hdr = (HEADER *)ds->ansp;
    p = ds->ansp;
    nqueries = ntohs((unsigned short)hdr->qdcount);
    nanswers = ntohs((unsigned short)hdr->ancount);
    p += sizeof(HEADER);

    /*
     * Skip query records.
     */
    while (nqueries--) {
#if HAVE_DN_SKIPNAME
	len = dn_skipname(p, (unsigned char *)ds->ansp + ds->anslen);
#else
	len = dn_expand(ds->ansp, (unsigned char *)ds->ansp + ds->anslen,
			p, host, sizeof(host));
#endif
	if (len < 0 || !INCR_OK(ds->ansp, ds->anslen, p, len))
	    return -1;
	p += len;
    }
    ds->ptr = p;
    ds->nanswers = nanswers;
    return 0;
}

/*
 * krb5int_dns_nextans() - get next answer record
 *
 * Sets pp to NULL if no more records.
 */
int
krb5int_dns_nextans(struct krb5int_dns_state *ds,
		    const unsigned char **pp, int *lenp)
{
    int len;
    unsigned char *p;
    unsigned short ntype, nclass, rdlen;
#if !HAVE_DN_SKIPNAME
    char host[MAXDNAME];
#endif

    *pp = NULL;
    *lenp = 0;
    p = ds->ptr;

    while (ds->nanswers--) {
#if HAVE_DN_SKIPNAME
	len = dn_skipname(ds->ansp, (unsigned char *)ds->ansp + ds->anslen);
#else
	len = dn_expand(ds->ansp, (unsigned char *)ds->ansp + ds->anslen,
			p, host, sizeof(host));
#endif
	if (len < 0 || !INCR_OK(ds->ansp, ds->anslen, p, len))
	    return -1;
	SAFE_GETUINT16(ds->ansp, ds->anslen, p, 2, ntype, out);
	/* Also skip 4 bytes of TTL */
	SAFE_GETUINT16(ds->ansp, ds->anslen, p, 6, nclass, out);
	SAFE_GETUINT16(ds->ansp, ds->anslen, p, 2, rdlen, out);

	if (!INCR_OK(ds->ansp, ds->anslen, p, rdlen))
	    return -1;
	if (rdlen > INT_MAX)
	    return -1;
	if (nclass == ds->nclass && ntype == ds->ntype) {
	    *pp = p;
	    *lenp = rdlen;
	    ds->ptr = p + rdlen;
	    return 0;
	}
    }
    return 0;
out:
    return -1;
}

#endif

#endif /* KRB5_DNS_LOOKUP */
