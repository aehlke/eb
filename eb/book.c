/*
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

#include "build-pre.h"
#include "eb.h"
#include "error.h"
#include "font.h"
#include "build-post.h"

/*
 * Book ID counter.
 */
static EB_Book_Code book_counter = 0;

/*
 * Mutex for `book_counter'.
 */
#ifdef ENABLE_PTHREAD
static pthread_mutex_t book_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * Unexported functions.
 */
static void eb_fix_misleaded_book EB_P((EB_Book *));
static EB_Error_Code eb_load_catalog EB_P((EB_Book *));
static void eb_load_language EB_P((EB_Book *));

/*
 * Initialize `book'.
 */
void
eb_initialize_book(book)
    EB_Book *book;
{
    LOG(("in: eb_initialize_book()"));

    book->code = EB_BOOK_NONE;
    book->disc_code = EB_DISC_INVALID;
    book->version = 0;
    book->character_code = EB_CHARCODE_INVALID;
    book->path = NULL;
    book->path_length = 0;
    book->subbooks = NULL;
    book->subbook_current = NULL;
    eb_initialize_text_context(book);
    eb_initialize_binary_context(book);
    eb_initialize_search_contexts(book);
    eb_initialize_binary_context(book);
    eb_initialize_lock(&book->lock);

    LOG(("out: eb_initialize_book()"));
}


/*
 * Bind `book' to `path'.
 */
EB_Error_Code
eb_bind(book, path)
    EB_Book *book;
    const char *path;
{
    EB_Error_Code error_code;
    char temporary_path[PATH_MAX + 1];

    eb_lock(&book->lock);
    LOG(("in: eb_bind(path=%s)", path));

    /*
     * Clear the book if the book has already been bound.
     */
    if (book->path != NULL) {
	eb_finalize_book(book);
	eb_initialize_book(book);
    }

    /*
     * Assign a book code.
     */
    pthread_mutex_lock(&book_counter_mutex);
    book->code = book_counter++;
    pthread_mutex_unlock(&book_counter_mutex);
    
    /*
     * Set the path of the book.
     * The length of the file name "<path>/subdir/subsubdir/file.ebz;1" must
     * be PATH_MAX maximum.
     */
    if (PATH_MAX < strlen(path)) {
	error_code = EB_ERR_TOO_LONG_FILE_NAME;
	goto failed;
    }
    strcpy(temporary_path, path);
    error_code = eb_canonicalize_path_name(temporary_path);
    if (error_code != EB_SUCCESS)
	goto failed;

    book->path_length = strlen(temporary_path);
    if (PATH_MAX < book->path_length + 1 + EB_MAX_DIRECTORY_NAME_LENGTH + 1
	+ EB_MAX_DIRECTORY_NAME_LENGTH + 1 + EB_MAX_FILE_NAME_LENGTH) {
	error_code = EB_ERR_TOO_LONG_FILE_NAME;
	goto failed;
    }

    book->path = (char *)malloc(book->path_length + 1);
    if (book->path == NULL) {
	error_code = EB_ERR_MEMORY_EXHAUSTED;
	goto failed;
    }
    strcpy(book->path, temporary_path);

    /*
     * Read information from the `LANGUAGE' file.
     * If failed to initialize, JIS X 0208 is assumed.
     */
    eb_load_language(book);

    /*
     * Read information from the `CATALOG(S)' file.
     */
    error_code = eb_load_catalog(book);
    if (error_code != EB_SUCCESS)
	goto failed;

    LOG(("out: eb_bind(book=%d) = %s", (int)book->code,
	eb_error_string(EB_SUCCESS)));
    eb_unlock(&book->lock);
    return EB_SUCCESS;

    /*
     * An error occurs...
     */
  failed:
    eb_finalize_book(book);
    LOG(("out: eb_bind() = %s", eb_error_string(error_code)));
    eb_unlock(&book->lock);
    return error_code;
}


/*
 * Suspend using `book'.
 */
void
eb_suspend(book)
    EB_Book *book;
{
    eb_lock(&book->lock);
    LOG(("in: eb_suspend(book=%d)", (int)book->code));

    eb_unset_subbook(book);

    LOG(("out: eb_suspend()"));
    eb_unlock(&book->lock);
}


/*
 * Finish using `book'.
 */
void
eb_finalize_book(book)
    EB_Book *book;
{
    LOG(("in: eb_finalize_book(book=%d)", (int)book->code));

    eb_unset_subbook(book);

    if (book->path != NULL)
	free(book->path);

    book->code = EB_BOOK_NONE;
    book->disc_code = EB_DISC_INVALID;
    book->version = 0;
    book->character_code = EB_CHARCODE_INVALID;
    book->path = NULL;
    book->path_length = 0;

    if (book->subbooks != NULL) {
	eb_finalize_subbooks(book);
	free(book->subbooks);
	book->subbooks = NULL;
    }

    book->subbook_current = NULL;
    eb_finalize_text_context(book);
    eb_finalize_binary_context(book);
    eb_finalize_search_contexts(book);
    eb_finalize_binary_context(book);
    eb_finalize_lock(&book->lock);
    eb_finalize_lock(&book->lock);

    LOG(("out: eb_finalize_book()"));
}


/*
 * There are some books that EB Library sets wrong character code of
 * the book.  They are written in JIS X 0208, but the library sets
 * ISO 8859-1.
 *
 * We fix the character of the books.  The following table lists
 * titles of the first subbook in those books.
 */
static const char * const misleaded_book_table[] = {
    /* SONY DataDiskMan (DD-DR1) accessories. */
    "%;%s%A%e%j!\\%S%8%M%9!\\%/%i%&%s",

    /* Shin Eiwa Waei Chujiten (earliest edition) */
    "8&5f<R!!?71QOBCf<-E5",

    /* EB Kagakugijutsu Yougo Daijiten (YRRS-048) */
    "#E#B2J3X5;=QMQ8lBg<-E5",

    NULL
};

/*
 * Fix chachacter-code of the book if misleaded.
 */
static void
eb_fix_misleaded_book(book)
    EB_Book *book;
{
    const char * const * misleaded;
    EB_Subbook *subbook;
    int i;

    LOG(("in: eb_fix_misleaded_book(book=%d)", (int)book->code));

    for (misleaded = misleaded_book_table; *misleaded != NULL; misleaded++) {
	if (strcmp(book->subbooks->title, *misleaded) == 0) {
	    book->character_code = EB_CHARCODE_JISX0208;
	    for (i = 0, subbook = book->subbooks; i < book->subbook_count;
		 i++, subbook++) {
		eb_jisx0208_to_euc(subbook->title, subbook->title);
	    }
	    break;
	}
    }

    LOG(("out: eb_fix_misleaded_book()"));
}

/*
 * Read information from the `CATALOG(S)' file in 'book'.
 * Return EB_SUCCESS if it succeeds, error-code otherwise.
 */
static EB_Error_Code
eb_load_catalog(book)
    EB_Book *book;
{
    EB_Error_Code error_code;
    char buffer[EB_SIZE_PAGE];
    char catalog_file_name[EB_MAX_FILE_NAME_LENGTH + 1];
    char catalog_path_name[PATH_MAX + 1];
    char *space;
    EB_Subbook *subbook;
    size_t catalog_size;
    size_t title_size;
    Zio zio;
    Zio_Code zio_code;
    int i;

    LOG(("in: eb_load_catalog(book=%d)", (int)book->code));

    zio_initialize(&zio);

    /*
     * Find a catalog file.
     */
    if (eb_find_file_name(book->path, "catalog", catalog_file_name)
	== EB_SUCCESS) {
	book->disc_code = EB_DISC_EB;
	catalog_size = EB_SIZE_EB_CATALOG;
	title_size = EB_MAX_EB_TITLE_LENGTH;
    } else if (eb_find_file_name(book->path, "catalogs", catalog_file_name)
	== EB_SUCCESS) {
	book->disc_code = EB_DISC_EPWING;
	catalog_size = EB_SIZE_EPWING_CATALOG;
	title_size = EB_MAX_EPWING_TITLE_LENGTH;
    } else {
	error_code = EB_ERR_FAIL_OPEN_CAT;
	goto failed;
    }

    eb_compose_path_name(book->path, catalog_file_name, catalog_path_name);
    eb_path_name_zio_code(catalog_path_name, ZIO_PLAIN, &zio_code);

    /*
     * Open a catalog file.
     */
    if (zio_open(&zio, catalog_path_name, zio_code) < 0) {
	error_code = EB_ERR_FAIL_OPEN_CAT;
	goto failed;
    }

    /*
     * Get the number of subbooks in this book.
     */
    if (zio_read(&zio, buffer, 16) != 16) {
	error_code = EB_ERR_FAIL_READ_CAT;
	goto failed;
    }

    book->subbook_count = eb_uint2(buffer);
    LOG(("aux: eb_load_catalog: subbook_count=%d", book->subbook_count));
    if (EB_MAX_SUBBOOKS < book->subbook_count)
	book->subbook_count = EB_MAX_SUBBOOKS;
    if (book->subbook_count == 0) {
	error_code = EB_ERR_UNEXP_CAT;
	goto failed;
    }

    /*
     * Get EPWING format version.
     */
    if (book->disc_code == EB_DISC_EPWING)
	book->version = eb_uint1(buffer + 3);

    /*
     * Allocate memories for subbook entries.
     */
    book->subbooks = (EB_Subbook *) malloc(sizeof(EB_Subbook)
	* book->subbook_count);
    if (book->subbooks == NULL) {
	error_code = EB_ERR_MEMORY_EXHAUSTED;
	goto failed;
    }
    eb_initialize_subbooks(book);

    /*
     * Read information about subbook.
     */
    for (i = 0, subbook = book->subbooks; i < book->subbook_count;
	 i++, subbook++) {
	/*
	 * Read data from the catalog file.
	 */
	if (zio_read(&zio, buffer, catalog_size) != catalog_size) {
	    error_code = EB_ERR_FAIL_READ_CAT;
	    goto failed;
	}

	/*
	 * Set a directory name.
	 */
	strncpy(subbook->directory_name, buffer + 2 + title_size,
	    EB_MAX_DIRECTORY_NAME_LENGTH);
	subbook->directory_name[EB_MAX_DIRECTORY_NAME_LENGTH] = '\0';
	space = strchr(subbook->directory_name, ' ');
	if (space != NULL)
	    *space = '\0';
	eb_fix_directory_name(book->path, subbook->directory_name);

	/*
	 * Set an index page.
	 */
	if (book->disc_code == EB_DISC_EB)
	    subbook->index_page = 1;
	else {
	    subbook->index_page = eb_uint2(buffer + 2
		+ EB_MAX_EPWING_TITLE_LENGTH + EB_MAX_DIRECTORY_NAME_LENGTH
		+ 4);
	}

	/*
	 * Set a title.  (Convert from JISX0208 to EUC JP)
	 */
	strncpy(subbook->title, buffer + 2, title_size);
	subbook->title[title_size] = '\0';
	if (book->character_code != EB_CHARCODE_ISO8859_1)
	    eb_jisx0208_to_euc(subbook->title, subbook->title);

	/*
	 * If the book is EPWING, get font file names.
	 */
	if (book->disc_code == EB_DISC_EPWING) {
	    EB_Font *font;
	    char *buffer_p;
	    int j;

	    /*
	     * Narrow font file names.
	     */
	    buffer_p = buffer + 2 + title_size + 50;
	    for (font = subbook->narrow_fonts, j = 0; j < EB_MAX_FONTS;
		 j++, font++) {
		/*
		 * Skip this entry if the first character of the file name
		 * is not valid.
		 */
		if (*buffer_p == '\0' || 0x80 <= *((unsigned char *)buffer_p))
		    continue;
		strncpy(font->file_name, buffer_p,
		    EB_MAX_DIRECTORY_NAME_LENGTH);
		font->file_name[EB_MAX_DIRECTORY_NAME_LENGTH] = '\0';
		font->font_code = j;
		font->page = 1;
		space = strchr(font->file_name, ' ');
		if (space != NULL)
		    *space = '\0';
		buffer_p += EB_MAX_DIRECTORY_NAME_LENGTH;
	    }

	    /*
	     * Wide font file names.
	     */
	    buffer_p = buffer + 2 + title_size + 18;
	    for (font = subbook->wide_fonts, j = 0; j < EB_MAX_FONTS;
		 j++, font++) {
		/*
		 * Skip this entry if the first character of the file name
		 * is not valid.
		 */
		if (*buffer_p == '\0' || 0x80 <= *((unsigned char *)buffer_p))
		    continue;
		strncpy(font->file_name, buffer_p,
		    EB_MAX_DIRECTORY_NAME_LENGTH);
		font->file_name[EB_MAX_DIRECTORY_NAME_LENGTH] = '\0';
		font->font_code = j;
		font->page = 1;
		space = strchr(font->file_name, ' ');
		if (space != NULL)
		    *space = '\0';
		buffer_p += EB_MAX_DIRECTORY_NAME_LENGTH;
	    }
	}

	subbook->initialized = 0;
	subbook->code = i;
    }

    /*
     * Close the catalog file.
     */
    zio_close(&zio);
    zio_finalize(&zio);

    /*
     * Fix chachacter-code of the book.
     */
    eb_fix_misleaded_book(book);
    LOG(("out: eb_load_catalog() = %s", eb_error_string(EB_SUCCESS)));

    return EB_SUCCESS;

    /*
     * An error occurs...
     */
  failed:
    zio_close(&zio);
    zio_finalize(&zio);
    if (book->subbooks != NULL) {
	free(book->subbooks);
	book->subbooks = NULL;
    }
    LOG(("out: eb_load_catalog() = %s", eb_error_string(error_code)));
    return error_code;
}


/*
 * Read information from the `LANGUAGE' file in `book'.
 */
static void
eb_load_language(book)
    EB_Book *book;
{
    Zio zio;
    Zio_Code zio_code;
    char language_path_name[PATH_MAX + 1];
    char language_file_name[EB_MAX_FILE_NAME_LENGTH + 1];
    char buffer[16];

    LOG(("in: eb_load_language(book=%d)", (int)book->code));

    zio_initialize(&zio);
    book->character_code = EB_CHARCODE_JISX0208;

    /*
     * Open the language file.
     */
    if (eb_find_file_name(book->path, "language", language_file_name)
	!= EB_SUCCESS)
	goto failed;

    eb_compose_path_name(book->path, language_file_name, language_path_name);
    eb_path_name_zio_code(language_path_name, ZIO_PLAIN, &zio_code);

    if (zio_open(&zio, language_path_name, zio_code) < 0)
	goto failed;

    /*
     * Get a character code of the book, and get the number of langueages
     * in the file.
     */
    if (zio_read(&zio, buffer, 16) != 16)
	goto failed;

    book->character_code = eb_uint2(buffer);
    if (book->character_code != EB_CHARCODE_ISO8859_1
	&& book->character_code != EB_CHARCODE_JISX0208
	&& book->character_code != EB_CHARCODE_JISX0208_GB2312) {
	goto failed;
    }

    zio_close(&zio);
    LOG(("out: eb_load_language()"));

    return;

    /*
     * An error occurs...
     */
  failed:
    zio_close(&zio);
    LOG(("out: eb_load_language()"));
}


/*
 * Test whether `book' is bound.
 */
int
eb_is_bound(book)
    EB_Book *book;
{
    int is_bound;

    eb_lock(&book->lock);
    LOG(("in: eb_is_bound(book=%d)", (int)book->code));

    /*
     * Check for the current status.
     */
    is_bound = (book->path != NULL);

    LOG(("out: eb_is_bound() = %d", is_bound));
    eb_unlock(&book->lock);

    return is_bound;
}


/*
 * Return the bound path of `book'.
 */
EB_Error_Code
eb_path(book, path)
    EB_Book *book;
    char *path;
{
    EB_Error_Code error_code;

    eb_lock(&book->lock);
    LOG(("in: eb_path(book=%d)", (int)book->code));

    /*
     * Check for the current status.
     */
    if (book->path == NULL) {
	error_code = EB_ERR_UNBOUND_BOOK;
	goto failed;
    }

    /*
     * Copy the path to `path'.
     */
    strcpy(path, book->path);

    LOG(("out: eb_path(path=%s) = %s", path, eb_error_string(EB_SUCCESS)));
    eb_unlock(&book->lock);

    return EB_SUCCESS;

    /*
     * An error occurs...
     */
  failed:
    *path = '\0';
    LOG(("out: eb_path() = %s", eb_error_string(error_code)));
    eb_unlock(&book->lock);
    return error_code;
}


/*
 * Inspect a disc type.
 */
EB_Error_Code
eb_disc_type(book, disc_code)
    EB_Book *book;
    EB_Disc_Code *disc_code;
{
    EB_Error_Code error_code;

    eb_lock(&book->lock);
    LOG(("in: eb_disc_type(book=%d)", (int)book->code));

    /*
     * Check for the current status.
     */
    if (book->path == NULL) {
	error_code = EB_ERR_UNBOUND_BOOK;
	goto failed;
    }

    /*
     * Copy the disc code to `disc_code'.
     */
    *disc_code = book->disc_code;

    LOG(("out: eb_disc_type(disc_code=%d) = %s", (int)*disc_code,
	eb_error_string(EB_SUCCESS)));
    eb_unlock(&book->lock);

    return EB_SUCCESS;

    /*
     * An error occurs...
     */
  failed:
    *disc_code = EB_DISC_INVALID;
    LOG(("out: eb_disc_type() = %s", eb_error_string(error_code)));
    eb_unlock(&book->lock);
    return error_code;
}


/*
 * Inspect a character code used in the book.
 */
EB_Error_Code
eb_character_code(book, character_code)
    EB_Book *book;
    EB_Character_Code *character_code;
{
    EB_Error_Code error_code;

    eb_lock(&book->lock);
    LOG(("in: eb_character_code(book=%d)", (int)book->code));

    /*
     * Check for the current status.
     */
    if (book->path == NULL) {
	error_code = EB_ERR_UNBOUND_BOOK;
	goto failed;
    }

    /*
     * Copy the character code to `character_code'.
     */
    *character_code = book->character_code;

    LOG(("out: eb_character_code(character_code=%d) = %s",
	(int)*character_code, eb_error_string(EB_SUCCESS)));
    eb_unlock(&book->lock);

    return EB_SUCCESS;

    /*
     * An error occurs...
     */
  failed:
    *character_code = EB_CHARCODE_INVALID;
    LOG(("out: eb_character_code() = %s", eb_error_string(error_code)));
    eb_unlock(&book->lock);
    return error_code;
}


