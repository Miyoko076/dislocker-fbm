/* -*- coding: utf-8 -*- */
/* -*- mode: c -*- */
/*
 * Dislocker -- enables to read/write on BitLocker encrypted partitions under
 * Linux
 * Copyright (C) 2012-2013  Romain Coltel, Hervé Schauer Consultants
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */


#include <errno.h>
#include <stdio.h>

#include <readline/readline.h>

#include "dislocker/accesses/rp/recovery_password.h"
#include "dislocker/metadata/vmk.h"



// Specifications of the recovery password
#define NB_RP_BLOCS   8
#define NB_DIGIT_BLOC 6

#define INTERMEDIATE_KEY_LENGTH 32

/* Length of one block plus its trailing '-' separator. */
#define RP_BLOCK_STRIDE (NB_DIGIT_BLOC + 1)

/* Length of a complete password string: 8 blocks of 6 digits + 7 separators. */
#define RP_FULL_LEN (NB_RP_BLOCS * NB_DIGIT_BLOC + (NB_RP_BLOCS - 1))

/* Terminal for the -p prompt; native Windows has no /dev/tty (READ_CONIN). */
#ifdef READ_CONIN
#  define RP_TTY_IN  "CONIN$"
#else
#  define RP_TTY_IN  "/dev/tty"
#endif

/* The real password, from rp_digit's 8th block until prompt_rp copies it. */
static char rp_captured[RP_FULL_LEN + 1];

static void rp_build_mask(char* out);


/**
 * Get the VMK datum using a recovery password
 *
 * @param dis_metadata The metadata structure
 * @param cfg The configuration structure
 * @param vmk_datum The datum_key_t found, containing the unencrypted VMK
 * @return TRUE if result can be trusted, FALSE otherwise
 */
int get_vmk_from_rp(dis_metadata_t dis_meta, dis_config_t* cfg, void** vmk_datum)
{
	return get_vmk_from_rp2(dis_meta, cfg->recovery_password, vmk_datum);
}


/**
 * Get the VMK datum using a recovery password
 *
 * @param dis_metadata The metadata structure
 * @param recovery_password The recovery password provided by the user
 * @param vmk_datum The datum_key_t found, containing the unencrypted VMK
 * @return TRUE if result can be trusted, FALSE otherwise
 */
int get_vmk_from_rp2(dis_metadata_t dis_meta, uint8_t* recovery_password,
	void** vmk_datum)
{
	// Check parameters
	if(!dis_meta)
		return FALSE;

	uint8_t* recovery_key = NULL;
	uint8_t salt[16] = {0,};

	int result = FALSE;
	void* prev_vmk_datum = NULL;

	/* If the recovery password wasn't provide, ask for it */
	if(!recovery_password)
		if(!prompt_rp(&recovery_password))
		{
			dis_printf(L_ERROR, "Cannot get valid recovery password. Abort.\n");
			return FALSE;
		}


	dis_printf(L_DEBUG, "Using the recovery password: '%s'.\n",
	                (char *)recovery_password);


	while (!result) {
		/*
		* We need a salt contained in the VMK datum associated to the recovery
		* password, so go get this salt and the VMK datum first
		* We use here the range which should be upper (or equal) than 0x800
		*/
		if(!get_vmk_datum_from_range(dis_meta, 0x800, 0xfff, (void**) vmk_datum, prev_vmk_datum))
		{
			dis_printf(
				L_ERROR,
				"Error, can't find a valid and matching VMK datum. Abort.\n"
			);
			*vmk_datum = NULL;
			return FALSE;
		}
		prev_vmk_datum = *vmk_datum;


		/*
		* We have the datum containing other data, so get in there and take the
		* nested one with type 3 (stretch key)
		*/
		void* stretch_datum = NULL;
		if(!get_nested_datumvaluetype(
				*vmk_datum,
				DATUMS_VALUE_STRETCH_KEY,
				&stretch_datum
			) ||
		!stretch_datum)
		{
			char* type_str = datumvaluetypestr(DATUMS_VALUE_STRETCH_KEY);
			dis_printf(
				L_ERROR,
				"Error looking for the nested datum of type %hd (%s) in the VMK one"
				". Internal failure, abort.\n",
				DATUMS_VALUE_STRETCH_KEY,
				type_str
			);
			dis_free(type_str);
			*vmk_datum = NULL;
			return FALSE;
		}


		/* The salt is in here, don't forget to keep it somewhere! */
		memcpy(salt, ((datum_stretch_key_t*) stretch_datum)->salt, 16);


		/* Get data which can be decrypted with this password */
		void* aesccm_datum = NULL;
		if(!get_nested_datumvaluetype(
				*vmk_datum,
				DATUMS_VALUE_AES_CCM,
				&aesccm_datum
			) ||
		!aesccm_datum)
		{
			dis_printf(
				L_ERROR,
				"Error finding the AES_CCM datum including the VMK. "
				"Internal failure, abort.\n"
			);
			*vmk_datum = NULL;
			return FALSE;
		}


		/*
		* We have all the things we need to compute the intermediate key from
		* the recovery password, so do it!
		*/
		recovery_key = dis_malloc(32 * sizeof(uint8_t));

		if(!intermediate_key(recovery_password, salt, recovery_key))
		{
			dis_printf(
				L_ERROR,
				"Error computing the recovery password to the recovery key. "
				"Abort.\n"
			);
			*vmk_datum = NULL;
			dis_free(recovery_key);
			return FALSE;
		}

		/* As the computed key length is always the same, use a direct value */
		result = get_vmk(
			(datum_aes_ccm_t*) aesccm_datum,
			recovery_key,
			32,
			(datum_key_t**) vmk_datum
		);

		dis_free(recovery_key);
	}

	return result;
}


/**
 * Validating one block of the recovery password
 *
 * @param digits The entire block to validate
 * @param block_nb The block number
 * @param short_password The block divided by 11 and converted into uint16_t
 * @return TRUE if valid, FALSE otherwise
 */
int valid_block(uint8_t* digits, int block_nb, uint16_t* short_password)
{
	// Check the parameters
	if(!digits)
		return FALSE;


	/* Convert chars into int */
	errno = 0;
	long int block = strtol((char *) digits, (char **) NULL, 10);
	if(errno == ERANGE)
	{
		dis_printf(
			L_ERROR,
			"Error converting '%s' into a number: errno=ERANGE",
			digits
		);
		return FALSE;
	}

	/* 1st check --  Checking if the bloc is divisible by eleven */
	if((block % 11) != 0)
	{
		dis_printf(
			L_ERROR,
			"Error handling the recovery password: Bloc n°%d (%d) invalid. "
			"It has to be divisible by 11.\n",
			block_nb,
			block
		);
		return FALSE;
	}

	/* 2nd check -- Checking if the bloc is less than 2**16 * 11 */
	if(block >= 720896)
	{
		dis_printf(
			L_ERROR,
			"Error handling the recovery password: Bloc n°%d (%d) invalid. "
			"It has to be less than 2**16 * 11 (720896).\n",
			block_nb,
			block
		);
		return FALSE;
	}

	/* 3rd check -- Checking if the checksum is correct */
	int8_t check_digit = (int8_t)(digits[0] - digits[1] + digits[2] - digits[3] + digits[4]
									- 48) % 11; /* Convert chars into digits */

	/* Some kind of bug the c modulo has: -2 % 11 yields -2 instead of 9 */
	while(check_digit < 0)
		check_digit = (int8_t)(check_digit + 11);

	if(check_digit != (digits[5] - 48))
	{
		dis_printf(
			L_ERROR,
			"Error handling the recovery password: Bloc n°%d (%d) has "
			"invalid checksum.\n",
			block_nb,
			block
		);
		return FALSE;
	}

	/*
	 * The bloc has a good look, store a short version of it
	 * We already have checked the size (see 2nd check), a uint16_t can contain
	 * the result
	 */
	if(short_password)
		*short_password = (uint16_t) (block / 11);

	return TRUE;
}


/**
 * Validating (or not) the recovery password.
 * If the password is valid, short_password contains the blocs divided by 11 in
 * uint16_t slot.
 *
 * @param recovery_password The recovery password the user typed (48+7 bytes)
 * @param short_password The recovery password converted in uint16_t (8 uint16_t)
 * @return TRUE if valid, FALSE otherwise
 */
int is_valid_key(const uint8_t *recovery_password, uint16_t *short_password)
{
	// Check the parameters
	if(recovery_password == NULL)
		return FALSE;

	if(short_password == NULL)
		return FALSE;

	/* Begin by checking the length of the password */
	if(strlen((char*)recovery_password) != 48+7)
	{
		dis_printf(
			L_ERROR,
			"Error handling the recovery password: "
			"Wrong length (has to be %d)\n",
			48+7);
		return FALSE;
	}

	const uint8_t *rp = recovery_password;
	uint16_t *sp = short_password;
	uint8_t digits[NB_DIGIT_BLOC + 1];

	int loop = 0;

	for(loop = 0; loop < NB_RP_BLOCS; ++loop)
	{
		memcpy(digits, rp, NB_DIGIT_BLOC);
		digits[NB_DIGIT_BLOC] = 0;

		/* Check block validity */
		if(!valid_block(digits, loop+1, sp))
			return FALSE;

		sp++;
		rp += 7;
	}

	// All of the recovery password seems to be good

	return TRUE;
} // End is_valid_key



/**
 * Calculate the intermediate key from a raw recovery password.
 *
 * @param recovery_password The raw recovery password given by the user (48+7 bytes)
 * @param result_key The intermediate key used to decrypt the associated VMK (32 bytes)
 * @return TRUE if result_key can be trusted, FALSE otherwise
 */
int intermediate_key(const uint8_t *recovery_password,
                     const uint8_t *salt,
                     uint8_t *result_key)
{
	// Check the parameters
	if(recovery_password == NULL)
	{
		dis_printf(
			L_ERROR,
			"Error: No recovery password given, aborting calculation "
			"of the intermediate key.\n"
		);
		return FALSE;
	}

	if(result_key == NULL)
	{
		dis_printf(
			L_ERROR,
			"Error: No space to store the intermediate recovery key, "
			"aborting operation.\n"
		);
		return FALSE;
	}


	uint16_t passwd[NB_RP_BLOCS];
	uint8_t *iresult = dis_malloc(INTERMEDIATE_KEY_LENGTH * sizeof(uint8_t));
	uint8_t *iresult_save = iresult;
	int loop = 0;

	memset(passwd,  0, NB_RP_BLOCS * sizeof(uint16_t));
	memset(iresult, 0, INTERMEDIATE_KEY_LENGTH * sizeof(uint8_t));

	/* Check if the recovery_password has a good smile */
	if(!is_valid_key(recovery_password, passwd))
	{
		memclean(iresult, INTERMEDIATE_KEY_LENGTH * sizeof(uint8_t));
		return FALSE;
	}

	// passwd now contains the blocs divided by 11 in a uint16_t tab
	// Convert each one of the blocs in little endian and make it one buffer
	for(loop = 0; loop < NB_RP_BLOCS; ++loop)
	{
		*iresult = (uint8_t)(passwd[loop] & 0x00ff);
		iresult++;
		*iresult = (uint8_t)((passwd[loop] & 0xff00) >> 8);
		iresult++;
	}

	iresult = iresult_save;

	/* Just print it */
	char s[NB_RP_BLOCS*2 * 5 + 1] = {0,};
	for (loop = 0; loop < NB_RP_BLOCS*2; ++loop)
		snprintf(&s[loop*5], 6, "0x%02hhx ", iresult[loop]);

	dis_printf(L_DEBUG, "Distilled password: '%s\b'\n", s);

	stretch_recovery_key(iresult, salt, result_key);

	memclean(iresult, INTERMEDIATE_KEY_LENGTH * sizeof(uint8_t));

	/* We successfully retrieve the key! */
	return TRUE;
} // End intermediate_key


/* rl_getc_function: pass digits, backspace and DEL (Delete key -> DEL) only;
 * swallow every other byte and escape sequence. */
static int rp_getc(FILE* stream)
{
	int c = rl_getc(stream);
	for(;;)
	{
		if(c < 0) return c;
		if((c >= '0' && c <= '9') || c == '\b' || c == 0x7f) return c;
		if(c != 0x1b) { c = rl_getc(stream); continue; }

		/* ESC sequence */
		c = rl_getc(stream);
		if(c == '[')                  /* CSI */
		{
			int nparam = 0, first = 0, inter = 0;
			while((c = rl_getc(stream)) >= 0x20 && c <= 0x3f)
			{
				if(c <= 0x2f) inter = 1;
				else if(inter) break;
				else if(nparam++ == 0) first = c;
			}
			if(c == '~' && !inter && nparam == 1 && first == '3')
				return 0x7f;
			if(c >= 0x40 && c <= 0x7e) c = rl_getc(stream);
		}
		else if(c == 'O')             /* SS3 */
		{
			while((c = rl_getc(stream)) >= 0x30 && c <= 0x3f) ;
			if(c == 0x1b) continue;
			if(c >= 0 && c < 0x80) c = rl_getc(stream);
		}
	}
}


/* Digit key: append it; on each full block, check it and add '-', finish after
 * the 8th block, or drop the block if it is invalid. */
static int rp_digit(int count, int key)
{
	(void) count;

	/* Append exactly one digit. Append-only model => rl_point stays at rl_end. */
	char s[2];
	s[0] = (char) key;
	s[1] = '\0';
	rl_insert_text(s);

	/* A block completes at length 6, 13, 20, ... */
	if((rl_end % RP_BLOCK_STRIDE) != NB_DIGIT_BLOC)
		return 0;

	int block_nb = rl_end / RP_BLOCK_STRIDE + 1;   /* 1 .. NB_RP_BLOCS */

	/* Copy out the six digits that just completed. */
	uint8_t blk[NB_DIGIT_BLOC + 1];
	memcpy(blk, rl_line_buffer + (rl_end - NB_DIGIT_BLOC), NB_DIGIT_BLOC);
	blk[NB_DIGIT_BLOC] = '\0';

	if(valid_block(blk, block_nb, NULL))
	{
		if(block_nb >= NB_RP_BLOCS)
		{
			/* All blocks valid: keep the password, let readline redraw the line
			 * as a mask (it handles wrapped lines; a plain '\r' would not). */
			char masked[RP_FULL_LEN + 1];
			memcpy(rp_captured, rl_line_buffer, (size_t) RP_FULL_LEN + 1);
			rp_build_mask(masked);
			rl_replace_line(masked, 1);   /* 1: also drop the undo list */
			rl_point = rl_end;
			rl_redisplay();
			rl_done = 1;
		}
		else
		{
			rl_insert_text("-");
		}
	}
	else
	{
		/* Show the 6th digit before rejecting the block, as the original did. */
		rl_redisplay();

		/* Drop the block (rl_delete_text() does not move rl_point). */
		rl_delete_text(rl_end - NB_DIGIT_BLOC, rl_end);
		rl_point = rl_end;

		rl_ding();
		rl_crlf();
		fflush(rl_outstream);          /* rl_crlf()'s newline first: stdout may be buffered */
		fputs("Invalid block.\n", stderr);   /* like the original: on stderr */
		rl_on_new_line();
		rl_forced_update_display();
	}

	return 0;
}


/* Backspace/DEL: delete the last digit; a trailing '-' goes with the digit before it. */
static int rp_backspace(int count, int key)
{
	(void) count;
	(void) key;

	if(rl_end <= 0)
		return 0;

	if(rl_line_buffer[rl_end - 1] == '-')
	{
		int from = rl_end - 2;
		if(from < 0)
			from = 0;
		rl_delete_text(from, rl_end);
	}
	else
	{
		rl_delete_text(rl_end - 1, rl_end);
	}

	rl_point = rl_end;   /* rl_delete_text() does not move point */
	return 0;
}


/* Startup hook (after inputrc, before the first key), so it wins over inputrc. */
static int override_keymap_bindings(void)
{
	Keymap km = rl_get_keymap();
	int d;

	/* Every byte rp_getc() lets through must be bound here, so no default or
	 * inputrc binding can run. Widening rp_getc() means binding the new key. */
	for(d = '0'; d <= '9'; d++)
		rl_bind_key_in_map(d, rp_digit, km);

	rl_bind_key_in_map('\b', rp_backspace, km);   /* ^H                      */
	rl_bind_key_in_map(0x7f, rp_backspace, km);   /* DEL (and the Delete key) */

	return 0;
}


/* Build the masked "XXXXXX-XXXXXX-...-XXXXXX" string into out (>= RP_FULL_LEN+1). */
static void rp_build_mask(char* out)
{
	int i, pos = 0;
	for(i = 0; i < NB_RP_BLOCS; i++)
	{
		if(i)
			out[pos++] = '-';
		memset(out + pos, 'X', NB_DIGIT_BLOC);
		pos += NB_DIGIT_BLOC;
	}
	out[pos] = '\0';
}


/**
 * Prompt for the recovery password to be entered
 *
 * @param rp The place where to put the entered recovery password
 * @return TRUE if rp can be trusted, FALSE otherwise
 */
int prompt_rp(uint8_t** rp)
{
	// Check the parameter
	if(!rp)
		return FALSE;

	*rp = NULL;

	const char* prompt = "Enter the recovery password: ";

	/* The terminal, never stdin; none (cron, daemon) -> fail, as in the original. */
	FILE* tty = fopen(RP_TTY_IN, "r");
	if(!tty)
	{
		fprintf(stderr, "Cannot open tty or conin.\n");
		return FALSE;
	}

	/* Save the readline globals we override. */
	FILE* saved_in  = rl_instream;
	FILE* saved_out = rl_outstream;

	rl_getc_func_t* saved_getc      = rl_getc_function;
	rl_hook_func_t* saved_startup   = rl_startup_hook;
	rl_hook_func_t* saved_pre_input = rl_pre_input_hook;

	/* Read from the terminal, through our key filter and bindings. */
	rl_instream  = tty;
	rl_outstream = stdout;

	rl_getc_function  = rp_getc;
	rl_startup_hook   = override_keymap_bindings;
	rl_pre_input_hook = dis_ctrlc_hook;

	/* Re-read terminal capabilities: -u's readline may have left a dumb terminal. */
	rl_reset_terminal(NULL);

	memset(rp_captured, 0, sizeof(rp_captured));   /* no stale plaintext */

	char* line = readline(prompt);

	/* Restore the readline globals, then close the terminal. */
	rl_instream  = saved_in;
	rl_outstream = saved_out;

	rl_getc_function  = saved_getc;
	rl_startup_hook   = saved_startup;
	rl_pre_input_hook = saved_pre_input;

	fclose(tty);

	if(!line)
	{
		/* EOF / aborted before a full password was entered. */
		putchar('\n');
		return FALSE;
	}

	/* 'line' holds only the mask; the password is in rp_captured. */
	free(line);

	/* Only the 8th valid block ends readline() with a line; check anyway. */
	if(strlen(rp_captured) != (size_t) RP_FULL_LEN)
	{
		memset(rp_captured, 0, sizeof(rp_captured));
		dis_printf(L_ERROR, "Unexpected recovery password length, aborting\n");
		return FALSE;
	}

	/* The masked line may have wrapped; move past it before our confirmation. */
	putchar('\n');
	printf("Valid password format, continuing.\n");
	fflush(NULL);

	uint8_t* out = malloc((size_t) RP_FULL_LEN + 1);
	if(!out)
	{
		memset(rp_captured, 0, sizeof(rp_captured));
		dis_printf(L_ERROR, "Cannot allocate memory for recovery password, abort.\n");
		return FALSE;
	}
	memcpy(out, rp_captured, (size_t) RP_FULL_LEN + 1);
	memset(rp_captured, 0, sizeof(rp_captured));   /* wipe the static plaintext */

	*rp = out;   /* malloc()ed "DDDDDD-...-DDDDDD" */
	return TRUE;
}


/**
 * Print the result key which can be used to decrypt the associated VMK
 *
 * @param result_key The key after passed to intermediate_key
 */
void print_intermediate_key(uint8_t *result_key)
{
	if(result_key == NULL)
		return;

	int loop = 0;
	char s[32*3 + 1] = {0,};
	for(loop = 0; loop < 32; ++loop)
		snprintf(&s[loop*3], 4, "%02hhx ", result_key[loop]);

	dis_printf(L_INFO, "Intermediate recovery key:\n\t%s\n", s);
}
