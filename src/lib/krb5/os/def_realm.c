/*
 * lib/krb5/os/def_realm.c
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * krb5_get_default_realm() function.
 */

#include "k5-int.h"
#include <stdio.h>

/*
 * Retrieves the default realm to be used if no user-specified realm is
 *  available.  [e.g. to interpret a user-typed principal name with the
 *  realm omitted for convenience]
 * 
 *  returns system errors, NOT_ENOUGH_SPACE, KV5M_CONTEXT
*/

/*
 * Implementation:  the default realm is stored in a configuration file,
 * named by krb5_config_file;  the first token in this file is taken as
 * the default local realm name.
 */

extern char *krb5_config_file;		/* extern so can be set at
					   load/runtime */

krb5_error_code INTERFACE
krb5_get_default_realm(context, lrealm)
    krb5_context context;
    char **lrealm;
{
#ifdef OLD_CONFIG_FILE
    FILE *config_file;
    char realmbuf[BUFSIZ];
#endif
    char *realm;
    char *cp;

    if (!context || (context->magic != KV5M_CONTEXT)) 
	    return KV5M_CONTEXT;

    if (!context->default_realm) {
#ifdef OLD_CONFIG_FILE
	    krb5_find_config_files();
	    if (!(config_file = fopen(krb5_config_file, "r")))
		    /* can't open */
		    return KRB5_CONFIG_CANTOPEN;

	    if (fgets(realmbuf, sizeof(realmbuf), config_file) == NULL) {
		    fclose(config_file);
		    return(KRB5_CONFIG_BADFORMAT);
	    }
	    fclose(config_file);
	    
	    realmbuf[BUFSIZ-1] = '0';
	    cp = strchr(realmbuf, '\n');
	    if (cp)
		    *cp = '\0';
	    cp = strchr(realmbuf, ' ');
	    if (cp)
		    *cp = '\0';

	    context->default_realm = malloc(strlen (realmbuf) + 1);
	    if (!context->default_realm)
		    return ENOMEM;

	    strcpy(context->default_realm, realmbuf);
#else
	    /*
	     * XXX should try to figure out a reasonable default based
	     * on the host's DNS domain.
	     */
	    context->default_realm = 0;
	    profile_get_string(context->profile, "libdefaults",
			       "default_realm", 0, 0,
			       &context->default_realm);
	    if (context->default_realm == 0)
		return(KRB5_CONFIG_BADFORMAT);
#endif
    }
    
    realm = context->default_realm;

    if (!(*lrealm = cp = malloc((unsigned int) strlen(realm) + 1)))
	    return ENOMEM;
    strcpy(cp, realm);
    return(0);
}

krb5_error_code INTERFACE
krb5_set_default_realm(context, lrealm)
    krb5_context context;
    const char *lrealm;
{
    if (!context || (context->magic != KV5M_CONTEXT)) 
	    return KV5M_CONTEXT;

    if (context->default_realm) {
	    free(context->default_realm);
	    context->default_realm = 0;
    }

    /* Allow the user to clear the default realm setting by passing in 
       NULL */
    if (!lrealm) return 0;

    context->default_realm = malloc(strlen (lrealm) + 1);

    if (!context->default_realm)
	    return ENOMEM;

    strcpy(context->default_realm, lrealm);
    return(0);

}
