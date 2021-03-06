/************************************************************************
 *
 * NMF-mGPU - Non-negative Matrix Factorization on multi-GPU systems.
 *
 * Copyright (C) 2011-2015:
 *
 *	Edgardo Mejia-Roa(*), Carlos Garcia(*), Jose Ignacio Gomez(*),
 *	Manuel Prieto(*), Francisco Tirado(*) and Alberto Pascual-Montano(**).
 *
 *	(*)  ArTeCS Group, Complutense University of Madrid (UCM), Spain.
 *	(**) Functional Bioinformatics Group, Biocomputing Unit,
 *		National Center for Biotechnology-CSIC, Madrid, Spain.
 *
 *	E-mail for E. Mejia-Roa: <edgardomejia@fis.ucm.es>
 *	E-mail for A. Pascual-Montano: <pascual@cnb.csic.es>
 *
 *
 * This file is part of NMF-mGPU.
 *
 * NMF-mGPU is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NMF-mGPU is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NMF-mGPU. If not, see <http://www.gnu.org/licenses/>.
 *
 ***********************************************************************/
/**********************************************************
 * file_converter.c
 *	Program to perform binary-text file conversions.
 *
 * NOTE: The following macro constants can be defined to modify the
 *	behavior of routines, as well as some constant and data-type definitions.
 *
 *	Additional information:
 *		NMFGPU_VERBOSE: Shows some messages concerning the progress of the program, as well as some configuration parameters.
 *		NMFGPU_VERBOSE_2: Shows the parameters in some routine calls.
 *
 *	Timing:
 *		NMFGPU_PROFILING_GLOBAL: Compute total elapsed time.
 *
 *	Debug / Testing:
 *		NMFGPU_DEBUG: Shows the result of each matrix operation and data transfer.
 *
 **********************************************************
 **********************************************************
 **********************************************************
 *
 * NOTE: In order to improve performance:
 *
 *	+ The number of columns is rounded up to a multiple of <memory_alignment>.
 *	  The padded dimension is referred as "pitch".
 *
 *	  This leads to the following limits:
 *		- Maximum number of columns (padded or not): matrix_max_pitch.
 *		- Maximum number of rows: matrix_max_non_padded_dim.
 *		- Maximum number of items: matrix_max_num_items.
 *
 *	  All four GLOBAL variables must be initialized with the
 *	  set_matrix_limits() function.
 *
 **********************************************************
 *
 * Matrix tags:
 *
 * Any matrix may include the following "tag" elements:
 *
 *	+ A short description string, referred as "name".
 *	+ A list of column headers.
 *	+ A list of row labels.
 *
 * Each list is stored in a "struct tag_t" structure, which is composed by:
 *	+ All tokens stored as a (large) single string.
 *	+ An array of pointers to such tokens.
 *
 * All three elements (the "name" string, and the two tag_t structures) are
 * then stored in a "struct matrix_tags_t" structure.
 *
 * Both types of structure are defined in "matrix_io_routines.h".
 *
 **********************************************************/

#include "matrix_io/matrix_io.h"
#include "matrix_io/matrix_io_routines.h"
#include "common.h"
#include "index_type.h"
#include "real_type.h"

#if NMFGPU_PROFILING_GLOBAL
	#include <sys/time.h>
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

////////////////////////////////////////////////
////////////////////////////////////////////////

/* Global variables. */

// Information and/or error messages shown by all processes.
#if NMFGPU_DEBUG
	static bool const dbg_shown_by_all = false;	// Information or error messages on debug.
#endif
static bool const shown_by_all = false;			// Information messages in non-debug mode.
static bool const sys_error_shown_by_all = false;	// System error messages.
static bool const error_shown_by_all = false;		// Error messages on invalid arguments or I/O data.

////////////////////////////////////////////////
////////////////////////////////////////////////

/*
 * Prints all arguments to the specified file.
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE.
 */
static int print_file_converter_help( char const *restrict const execname )
{

	// Checks for NULL parameters
	if ( ! execname ) {
		print_errnum( error_shown_by_all, EFAULT, "print_file_converter_help( execname )" );
		return EXIT_FAILURE;
	}

	int status = EXIT_SUCCESS;

	// ---------------------------

	status = print_message( shown_by_all, "\t<< ASCII-Binary file converter >>\n\n"
				"Usage:\n\t%s <filename> [ -b <native_format> ] [ -cr ] [ -e <native_format> ]\n\t%s -h\n\n",
				execname, execname );


	if ( append_printed_message( shown_by_all,
				"\n<filename>\n\tInput data matrix (mandatory if 'help' is not requested).\n\n" ) != EXIT_SUCCESS )
		status = EXIT_SUCCESS;


	if ( help_file_formats() != EXIT_SUCCESS )
		status = EXIT_FAILURE;


	if ( append_printed_message( shown_by_all, "\n-h,-H\tPrints this help message.\n\n" ) != EXIT_SUCCESS )
		status = EXIT_FAILURE;

	return status;

} // print_file_converter_help

////////////////////////////////////////////////
////////////////////////////////////////////////

int main( int argc, char *argv[] )
{

	#if NMFGPU_PROFILING_GLOBAL
		// Elapsed time
		struct timeval t_tv;
	#endif

	process_id = 0;		// Global variables.
	num_act_processes = num_processes = 1;

	// Default limits for matrix dimensions.
	set_default_matrix_limits();

	int status = EXIT_SUCCESS;

	// ----------------------------------------

	#if NMFGPU_DEBUG || NMFGPU_DEBUG_READ_MATRIX || NMFGPU_DEBUG_READ_MATRIX2 \
		|| NMFGPU_DEBUG_READ_FILE || NMFGPU_DEBUG_READ_FILE2 || NMFGPU_VERBOSE_2

		// Permanently flushes the output stream in order to prevent losing messages if the program crashes.
		flush_output( true );
	#endif

	// ----------------------------------------

	/* Reads all parameters and performs error-checking. */

	bool help = false;			// Help message requested

	struct input_arguments arguments;	// Input arguments

	// Checks all arguments (shows error messages).
	if ( check_arguments( argc, (char const *restrict *restrict) argv, &help, &arguments ) != EXIT_SUCCESS )
		return EXIT_FAILURE;

	// If help was requested, just prints a help message and returns.
	if ( help )
		return print_file_converter_help( argv[0] );

	char const *restrict const filename = arguments.filename;	// Input data filename.
	bool const numeric_hdrs = arguments.numeric_hdrs;		// Has numeric columns headers.
	bool const numeric_lbls = arguments.numeric_lbls;		// Has numeric row labels.
	file_fmt_t const input_file_fmt = arguments.input_file_fmt;	// Input  file format.
	file_fmt_t const output_file_fmt = arguments.output_file_fmt;	// Output file format.

	// ----------------------------------------

	real *restrict matrix = NULL;
	index_t nrows = 0, ncols = 0, pitch = 0;
	struct matrix_tags_t mt = new_empty_matrix_tags();

	bool const transpose = false;

	// --------------------------------------------------------------------- //

	#if NMFGPU_PROFILING_GLOBAL
		// Elapsed time
		gettimeofday( &t_tv, NULL );
	#endif

	// --------------------------------------------------------------------- //

	// Loads the file.

	status = matrix_load( filename, numeric_hdrs, numeric_lbls, input_file_fmt, &matrix, &nrows, &ncols, &pitch, &mt );

	if ( status != EXIT_SUCCESS ) {
		print_error( error_shown_by_all, "Error reading '%s'\n", filename );
		return EXIT_FAILURE;
	}

	// --------------------------------------------------------------------- //

	#if NMFGPU_DEBUG
	{
		// Shows matrix content.
		bool const real_data = true;

		status = matrix_show( matrix, nrows, ncols, pitch, real_data, transpose, dbg_shown_by_all, &mt );

		if ( status != EXIT_SUCCESS ) {
			print_error( error_shown_by_all, "Error showing '%s'\n", filename );
			matrix_clean( matrix, mt );
			return EXIT_FAILURE;
		}
	}
	#endif

	// --------------------------------------------------------------------- //

	// Output filename

	// File extension:
	char const *restrict const p_file_ext = file_extension[ output_file_fmt ];

	char *restrict const filename_str = malloc( (strlen(filename) + strlen(p_file_ext)) * sizeof(char) );
	if ( ! filename_str ) {
		print_errnum( sys_error_shown_by_all, errno, "Error setting output filename: malloc(filename_str)" );
		matrix_clean( matrix, mt );
		return EXIT_FAILURE;
	}

	errno = 0;	// Resets errno.
	int const conv = sprintf( filename_str, "%s%s", filename, p_file_ext );

	if ( conv <= 0 ) {
		print_errnum( sys_error_shown_by_all, errno, "Error setting output filename (sprintf)" );
		free( filename_str ); matrix_clean( matrix, mt );
		return EXIT_FAILURE;
	}

	print_message( shown_by_all, "Output file: %s\n", filename_str );

	// --------------------------------------------------------------------- //

	// Saves the file.

	bool const verbose = true;

	status = matrix_save( filename_str, output_file_fmt, (real const *restrict) matrix, nrows, ncols, pitch, transpose, &mt, verbose );

	free( filename_str );

	matrix_clean( matrix, mt );

	// --------------------------------------------------------------------- //

	#if NMFGPU_PROFILING_GLOBAL
		// Elapsed time
		if ( status == EXIT_SUCCESS ) {
			struct timeval t_ftv, t_etv;
			gettimeofday( &t_ftv, NULL );
			timersub( &t_ftv, &t_tv, &t_etv );	// etv = ftv - tv
			float const total_time = t_etv.tv_sec + ( t_etv.tv_usec * 1e-06f );
			print_message( shown_by_all, "Done in %g seconds.\n", total_time );
		}
	#endif

	// --------------------------------------------------------------------- //

	return status;

} // main

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
