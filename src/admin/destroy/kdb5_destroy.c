/*
 * $Source$
 * $Author$
 *
 * Copyright 1990 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <krb5/mit-copyright.h>.
 *
 * kdb_dest(roy): destroy the named database.
 *
 * This version knows about DBM format databases.
 */

#if !defined(lint) && !defined(SABER)
static char rcsid_kdb_dest_c[] =
"$Id$";
#endif	/* !lint & !SABER */

#include <krb5/copyright.h>
#include <krb5/krb5.h>
#include <krb5/kdb.h>
#include <krb5/kdb_dbm.h>
#include <stdio.h>

#include <com_err.h>
#include <krb5/krb5_err.h>
#include <krb5/kdb5_err.h>
#include <krb5/isode_err.h>

#include <krb5/ext-proto.h>
#include <sys/file.h>			/* for unlink() */

#include <sys/param.h>			/* XXX for MAXPATHLEN */

extern int errno;

char *yes = "yes\n";			/* \n to compare against result of
					   fgets */

static void
usage(who, status)
char *who;
int status;
{
    fprintf(stderr, "usage: %s [-d dbpathname]\n", who);
    exit(status);
}

void
main(argc, argv)
int argc;
char *argv[];
{
    extern char *optarg;	
    int optchar;
    char *dbname = 0;
    char buf[5];
    char dbfilename[MAXPATHLEN];
    krb5_error_code retval;

    initialize_krb5_error_table();
    initialize_kdb5_error_table();
    initialize_isod_error_table();

    if (rindex(argv[0], '/'))
	argv[0] = rindex(argv[0], '/')+1;

    while ((optchar = getopt(argc, argv, "d:")) != EOF) {
	switch(optchar) {
	case 'd':			/* set db name */
	    dbname = optarg;
	    break;
	case '?':
	default:
	    usage(argv[0], 1);
	    /*NOTREACHED*/
	}
    }
    if (!dbname)
	dbname = DEFAULT_DBM_FILE;	/* XXX? */

    printf("Deleting KDC database stored in '%s', are you sure?\n", dbname);
    printf("(type 'yes' to confirm)? ");
    if ((fgets(buf, sizeof(buf), stdin) != NULL) && /* typed something */
	!strcmp(buf,yes)) {		/* it matches yes */
	printf("OK, deleting database '%s'...\n", dbname);
	(void) strcpy(dbfilename, dbname);
	(void) strcat(dbfilename, ".dir");
	if (unlink(dbfilename) == -1) {
	    retval = errno;
	    com_err(argv[0], retval, "deleting database file '%s'",dbfilename);
	    goto aborted;
	}
	(void) strcpy(dbfilename, dbname);
	(void) strcat(dbfilename, ".pag");
	if (unlink(dbfilename) == -1) {
	    retval = errno;
	    com_err(argv[0], retval, "deleting database file '%s'",dbfilename);
	    fprintf(stderr,
		    "Database partially deleted--inspect files manually!\n");
	    exit(1);
	}
	(void) strcpy(dbfilename, dbname);
	(void) strcat(dbfilename, ".ok");
	if (unlink(dbfilename) == -1) {
	    retval = errno;
	    com_err(argv[0], retval, "deleting database file '%s'",dbfilename);
	    fprintf(stderr,
		    "Database partially deleted--inspect files manually!\n");
	    exit(1);
	}
	printf("** Database '%s' destroyed.\n", dbname);
	exit(0);
    }
 aborted:
    printf("** Destruction aborted--database left intact.\n");
    exit(1);
}
