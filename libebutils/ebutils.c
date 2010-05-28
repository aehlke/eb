/*                                                            -*- C -*-
 * Copyright (c) 1997, 98, 99, 2000, 01  
 *    Motoyuki Kasahara
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#if !defined(STDC_HEADERS) && defined(HAVE_MEMORY_H)
#include <memory.h>
#endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#else /* not STDC_HEADERS and not HAVE_STRING_H */
#include <strings.h>
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#ifdef ENABLE_NLS
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <libintl.h>
#endif

#ifndef HAVE_STRCHR
#define strchr index
#define strrchr rindex
#endif /* HAVE_STRCHR */

#ifndef HAVE_STRCASECMP
#ifdef __STDC__
int strcasecmp(const char *, const char *);
int strncasecmp(const char *, const char *, size_t);
#else /* not __STDC__ */
int strcasecmp()
int strncasecmp();
#endif /* not __STDC__ */
#endif /* not HAVE_STRCASECMP */

/*
 * The maximum length of path name.
 */
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX        MAXPATHLEN
#else /* not MAXPATHLEN */
#define PATH_MAX        1024
#endif /* not MAXPATHLEN */
#endif /* not PATH_MAX */

#include "eb/eb.h"
#include "eb/error.h"
#include "ebutils.h"

/*
 * Tricks for gettext.
 */
#ifdef ENABLE_NLS
#define _(string) gettext(string)
#ifdef gettext_noop
#define N_(string) gettext_noop(string)
#else
#define N_(string) (string)
#endif
#else
#define _(string) (string)       
#define N_(string) (string)
#endif

/*
 * Character type tests and conversions.
 */
#define isdigit(c) ('0' <= (c) && (c) <= '9')
#define isupper(c) ('A' <= (c) && (c) <= 'Z')
#define islower(c) ('a' <= (c) && (c) <= 'z')
#define isalpha(c) (isupper(c) || islower(c))
#define isalnum(c) (isupper(c) || islower(c) || isdigit(c))
#define isxdigit(c) \
 (isdigit(c) || ('A' <= (c) && (c) <= 'F') || ('a' <= (c) && (c) <= 'f'))
#define toupper(c) (('a' <= (c) && (c) <= 'z') ? (c) - 0x20 : (c))
#define tolower(c) (('A' <= (c) && (c) <= 'Z') ? (c) + 0x20 : (c))

/*
 * Output ``try ...'' message to standard error.
 */
void
output_try_help(invoked_name)
    const char *invoked_name;
{
    fprintf(stderr, _("try `%s --help' for more information\n"), invoked_name);
    fflush(stderr);
}


/*
 * Output version number to stdandard out.
 */
void
output_version(program_name, program_version)
    const char *program_name;
    const char *program_version;
{
    printf("%s (EB Library) version %s\n", program_name, program_version);
    printf("Copyright (c) 1997, 98, 99, 2000, 01\n");
    printf("   Motoyuki Kasahara\n\n");
    printf("This is free software; you can redistribute it and/or modify\n");
    printf("it under the terms of the GNU General Public License as published by\n");
    printf("the Free Software Foundation; either version 2, or (at your option)\n");
    printf("any later version.\n\n");
    printf("This program is distributed in the hope that it will be useful,\n");
    printf("but WITHOUT ANY WARRANTY; without even the implied warranty\n");
    printf("of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    printf("GNU General Public License for more details.\n");
    fflush(stdout);
}


/*
 * Parse an argument to option `--subbook (-S)'.
 * If the argument is valid form, 0 is returned.
 * Otherwise -1 is returned.
 */
int
parse_subbook_name_argument(invoked_name, argument, name_list, name_count)
    const char *invoked_name;
    const char *argument;
    char name_list[][EB_MAX_DIRECTORY_NAME_LENGTH + 1];
    int *name_count;
{
    const char *argument_p = argument;
    char name[EB_MAX_DIRECTORY_NAME_LENGTH + 1];
    char *name_p;
    int i;

    while (*argument_p != '\0') {
	/*
	 * Check current `name_count'.
	 */
	if (EB_MAX_SUBBOOKS <= *name_count) {
	    fprintf(stderr, _("%s: too many subbooks\n"), invoked_name);
	    fflush(stderr);
	    return -1;
	}

	/*
	 * Take a next element in the argument.
	 */
	i = 0;
	name_p = name;
	while (*argument_p != ',' && *argument_p != '\0'
	    && i < EB_MAX_DIRECTORY_NAME_LENGTH) {
		*name_p = tolower(*argument_p);
	    i++;
	    name_p++;
	    argument_p++;
	}
	*name_p = '\0';
	if (*argument_p == ',')
	    argument_p++;
	else if (*argument_p != '\0') {
	    fprintf(stderr, _("%s: invalid subbook name `%s...'\n"),
		invoked_name, name);
	    fflush(stderr);
	    return -1;
	}

	/*
	 * If the subbook name is not found in the subbook name list,
	 * it is added to the list.
	 */
	for (i = 0; i < *name_count; i++) {
	    if (strcmp(name, name_list[i]) == 0)
		break;
	}
	if (*name_count <= i) {
	    strcpy(name_list[i], name);
	    (*name_count)++;
	}
    }

    return 0;
}


/*
 * Find a subbook-code of the subbook whose directory name is `directory'.
 * When no sub-book is matched', EB_ERR_NO_SUCH_SUB is returned.
 */
EB_Subbook_Code
find_subbook(book, directory, subbook_code)
    EB_Book *book;
    const char *directory;
    EB_Subbook_Code *subbook_code;
{
    EB_Error_Code error_code;
    EB_Subbook_Code subbook_list[EB_MAX_SUBBOOKS];
    char directory2[EB_MAX_DIRECTORY_NAME_LENGTH + 1];
    int subbook_count;
    int i;

    /*
     * Find the subbook in the current book.
     */
    error_code = eb_subbook_list(book, subbook_list, &subbook_count);
    if (error_code != EB_SUCCESS) {
	*subbook_code = EB_SUBBOOK_INVALID;
	return EB_ERR_NO_SUCH_SUB;
    }
    for (i = 0; i < subbook_count; i++) {
        error_code = eb_subbook_directory2(book, subbook_list[i], directory2);
	if (error_code != EB_SUCCESS)
	    continue;
        if (strcasecmp(directory, directory2) == 0) {
            *subbook_code = subbook_list[i];
	    return EB_SUCCESS;
	}
    }

    *subbook_code = EB_SUBBOOK_INVALID;
    return EB_ERR_NO_SUCH_SUB;
}


/*
 * Canonicalize `path'.
 * It eliminaes `/' at the tail unless `path' is not "/".
 */
void
canonicalize_path(path)
    char *path;
{
    size_t path_length;

    if (*path == '\0')
	return;
    if (*path == '/' && *(path + 1) == '\0')
	return;
    
    path_length = strlen(path);
    if (*(path + path_length - 1) == '/')
	*(path + path_length - 1) = '\0';
}


