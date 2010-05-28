/*
 * Copyright (c) 1997, 98, 2000, 01  
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

#include "build-pre.h"
#include "eb.h"
#include "error.h"
#include "build-post.h"

/*
 * Mutex for gettext function call.
 */
#if defined(ENABLE_NLS) && defined(ENABLE_PTHREAD)
pthread_mutex_t gettext_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * Error code strings.
 */
static const char * const error_strings[] = {
    /* 0 -- 4 */
    "EB_SUCCESS",
    "EB_ERR_MEMORY_EXHAUSTED",
    "EB_ERR_EMPTY_FILE_NAME",
    "EB_ERR_TOO_LONG_FILE_NAME",
    "EB_ERR_BAD_FILE_NAME",

    /* 5 -- 9 */
    "EB_ERR_BAD_DIR_NAME",
    "EB_ERR_TOO_LONG_WORD",
    "EB_ERR_BAD_WORD",
    "EB_ERR_EMPTY_WORD",
    "EB_ERR_FAIL_GETCWD",

    /* 10 -- 14 */
    "EB_ERR_FAIL_OPEN_CAT",
    "EB_ERR_FAIL_OPEN_CATAPP",
    "EB_ERR_FAIL_OPEN_TEXT",
    "EB_ERR_FAIL_OPEN_FONT",
    "EB_ERR_FAIL_OPEN_APP",

    /* 15 -- 19 */
    "EB_ERR_FAIL_OPEN_BINARY",
    "EB_ERR_FAIL_READ_CAT",
    "EB_ERR_FAIL_READ_CATAPP",
    "EB_ERR_FAIL_READ_TEXT",
    "EB_ERR_FAIL_READ_FONT",

    /* 20 -- 24 */
    "EB_ERR_FAIL_READ_APP",
    "EB_ERR_FAIL_READ_BINARY",
    "EB_ERR_FAIL_SEEK_CAT",
    "EB_ERR_FAIL_SEEK_CATAPP",
    "EB_ERR_FAIL_SEEK_TEXT",

    /* 25 -- 29 */
    "EB_ERR_FAIL_SEEK_FONT",
    "EB_ERR_FAIL_SEEK_APP",
    "EB_ERR_FAIL_SEEK_BINARY",
    "EB_ERR_UNEXP_CAT",
    "EB_ERR_UNEXP_CATAPP",

    /* 30 -- 34 */
    "EB_ERR_UNEXP_TEXT",
    "EB_ERR_UNEXP_FONT",
    "EB_ERR_UNEXP_APP",
    "EB_ERR_UNEXP_BINARY",
    "EB_ERR_UNBOUND_BOOK",

    /* 35 -- 39 */
    "EB_ERR_UNBOUND_APP",
    "EB_ERR_NO_SUB",
    "EB_ERR_NO_APPSUB",
    "EB_ERR_NO_FONT",
    "EB_ERR_NO_TEXT",

    /* 40 -- 44 */
    "EB_ERR_NO_CUR_SUB",
    "EB_ERR_NO_CUR_APPSUB",
    "EB_ERR_NO_CUR_FONT",
    "EB_ERR_NO_CUR_BINARY",
    "EB_ERR_NO_SUCH_SUB",

    /* 45 -- 49 */
    "EB_ERR_NO_SUCH_APPSUB",
    "EB_ERR_NO_SUCH_FONT",
    "EB_ERR_NO_SUCH_CHAR_BMP",
    "EB_ERR_NO_SUCH_CHAR_TEXT",
    "EB_ERR_NO_SUCH_SEARCH",

    /* 50 -- 54 */
    "EB_ERR_NO_SUCH_HOOK",
    "EB_ERR_NO_SUCH_BINARY",
    "EB_ERR_STOP_CODE",
    "EB_ERR_DIFF_CONTENT",
    "EB_ERR_NO_PREV_SEARCH",

    /* 55 -- 59 */
    "EB_ERR_NO_SUCH_MULTI_ID",
    "EB_ERR_NO_SUCH_ENTRY_ID",
    "EB_ERR_TOO_MANY_WORDS",
    "EB_ERR_NO_WORD",
    "EB_ERR_NO_CANDIDATES"
};

/*
 * Look up the error message corresponding to the error code `error_code'.
 */
const char *
eb_error_string(error_code)
    EB_Error_Code error_code;
{
    const char *string;

    if (0 <= error_code && error_code < EB_NUMBER_OF_ERRORS)
        string = error_strings[error_code];
    else
        string = "EB_ERR_UNKNOWN";

    return string;
}


/*
 * Error messages.
 */
static const char * const error_messages[] = {
    /* 0 -- 4 */
    N_("no error"),
    N_("memory exhausted"),
    N_("an empty file name"),
    N_("too long file name"),
    N_("bad file name"),

    /* 5 -- 9 */
    N_("bad directory name"),
    N_("too long word"),
    N_("a word contains bad character"),
    N_("an empty word"),
    N_("failed to get the current working directory"),

    /* 10 -- 14 */
    N_("failed to open a catalog file"),
    N_("failed to open an appendix catalog file"),
    N_("failed to open a text file"),
    N_("failed to open a font file"),
    N_("failed to open an appendix file"),

    /* 15 -- 19 */
    N_("failed to open a binary file"),
    N_("failed to read a catalog file"),
    N_("failed to read an appendix catalog file"),
    N_("failed to read a text file"),
    N_("failed to read a font file"),

    /* 20 -- 24 */
    N_("failed to read an appendix file"),
    N_("failed to read a binary file"),
    N_("failed to seek a catalog file"),
    N_("failed to seek an appendix catalog file"),
    N_("failed to seek a text file"),

    /* 25 -- 29 */
    N_("failed to seek a font file"),
    N_("failed to seek an appendix file"),
    N_("failed to seek a binary file"),
    N_("unexpected format in a catalog file"),
    N_("unexpected format in an appendix catalog file"),

    /* 30 -- 34 */
    N_("unexpected format in a text file"),
    N_("unexpected format in a font file"),
    N_("unexpected format in an appendix file"),
    N_("unexpected format in a binary file"),
    N_("book not bound"),

    /* 35 -- 39 */
    N_("appendix not bound"),
    N_("no subbook"),
    N_("no subbook in the appendix"),
    N_("no font"),
    N_("no text file"),

    /* 40 -- 44 */
    N_("no current subbook"),
    N_("no current appendix subbook"),
    N_("no current font"),
    N_("no current binary"),
    N_("no such subbook"),

    /* 45 -- 49 */
    N_("no such appendix subbook"),
    N_("no such font"),
    N_("no such character bitmap"),
    N_("no such character text"),
    N_("no such search method"),

    /* 50 -- 54 */
    N_("no such hook"),
    N_("no such binary"),
    N_("stop code found"),
    N_("different content type"),
    N_("no previous search"),

    /* 55 -- 59 */
    N_("no such multi search"),
    N_("no such multi search entry"),
    N_("too many words specified"),
    N_("no word specified"),
    N_("no candidates"),

    NULL
};

/*
 * Look up the error message corresponding to the error code `error_code'.
 */
const char *
eb_error_message(error_code)
    EB_Error_Code error_code;
{
    const char *message;

    if (0 <= error_code && error_code < EB_NUMBER_OF_ERRORS)
        message = error_messages[error_code];
    else
        message = N_("unknown error");

#ifdef ENABLE_NLS
    message = dgettext(EB_TEXT_DOMAIN_NAME, message);
#endif /* ENABLE_NLS */

    return message;
}
