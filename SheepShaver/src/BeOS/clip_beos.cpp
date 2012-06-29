/*
 *  clip_beos.cpp - Clipboard handling, BeOS implementation
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <support/UTF8.h>

#include "clip.h"
#include "main.h"
#include "cpu_emulation.h"
#include "emul_op.h"

#define DEBUG 0
#include "debug.h"


// Global variables
static bool we_put_this_data = false;	// Flag for PutScrap(): the data was put by GetScrap(), don't bounce it back to the Be side
static BTranslatorRoster *roster;
static float input_cap = 0;
static translator_info input_info;
static float output_cap = 0;
static translator_id output_trans = 0;


/*
 *  Clipboard manager thread (for calling clipboard functions; this is not safe
 *  under R4 when running on the MacOS stack in kernel space)
 */

// Message constants
const uint32 MSG_QUIT_CLIP_MANAGER = 'quit';
const uint32 MSG_PUT_TEXT = 'ptxt';

static thread_id cm_thread = -1;
static sem_id cm_done_sem = -1;

// Argument passing
static void *cm_scrap;
static int32 cm_length;

static status_t clip_manager(void *arg)
{
	for (;;) {

		// Receive message
		thread_id sender;
		uint32 code = receive_data(&sender, NULL, 0);
		D(bug("Clipboard manager received %08lx\n", code));
		switch (code) {
			case MSG_QUIT_CLIP_MANAGER:
				return 0;

			case MSG_PUT_TEXT:
				if (be_clipboard->Lock()) {
					be_clipboard->Clear();
					BMessage *clipper = be_clipboard->Data(); 
	
					// Convert text from Mac charset to UTF-8
					int32 dest_length = cm_length * 3;
					int32 state = 0;
					char *inbuf = new char[cm_length];
					memcpy(inbuf, cm_scrap, cm_length);	// Copy to user space
					char *outbuf = new char[dest_length];
					if (convert_to_utf8(B_MAC_ROMAN_CONVERSION, inbuf, &cm_length, outbuf, &dest_length, &state) == B_OK) {
						for (int i=0; i<dest_length; i++)
							if (outbuf[i] == 13)
								outbuf[i] = 10;
		
						// Add text to Be clipboard
						clipper->AddData("text/plain", B_MIME_TYPE, outbuf, dest_length); 
						be_clipboard->Commit();
					} else {
						D(bug(" text conversion failed\n"));
					}
					delete[] outbuf;
					delete[] inbuf;
					be_clipboard->Unlock();
				}
				break;
		}

		// Acknowledge
		release_sem(cm_done_sem);
	}
}


/*
 *  Initialize clipboard
 */

void ClipInit(void)
{
	// check if there is a translator that can handle the pict datatype
	roster = BTranslatorRoster::Default(); 
	int32 num_translators, i,j; 
	translator_id *translators; 
	const char *translator_name, *trans_info; 
	int32 translator_version; 
	const translation_format *t_formats;
	long t_num;
	   
	roster->GetAllTranslators(&translators, &num_translators); 
	for (i=0;i<num_translators;i++) { 
		roster->GetTranslatorInfo(translators[i], &translator_name, 
			&trans_info, &translator_version); 
		D(bug("found translator %s: %s (%.2f)\n", translator_name, trans_info, 
			translator_version/100.)); 
		// does this translator support the pict datatype ?
		roster->GetInputFormats(translators[i], &t_formats,&t_num);
		//printf(" supports %d input formats \n",t_num);
		for (j=0;j<t_num;j++) {
			if (!strcmp (t_formats[j].MIME,"image/pict")) {
				// matching translator found
				if (t_formats[j].capability>input_cap) {
					input_info.type = t_formats[j].type;
					input_info.group = t_formats[j].group;
					input_info.quality = t_formats[j].quality;
					input_info.capability = t_formats[j].capability;
					strcpy(input_info.MIME,t_formats[j].MIME);
					strcpy(input_info.name,t_formats[j].name);
					input_info.translator=translators[i];
					input_cap = t_formats[j].capability;
				}
				D(bug("matching input translator found:type:%c%c%c%c group:%c%c%c%c quality:%f capability:%f MIME:%s name:%s\n",
					t_formats[j].type>>24,t_formats[j].type>>16,t_formats[j].type>>8,t_formats[j].type,
					t_formats[j].group>>24,t_formats[j].group>>16,t_formats[j].group>>8,t_formats[j].group,
					t_formats[j].quality,
					t_formats[j].capability,t_formats[j].MIME,
					t_formats[j].name));
			}
			
		}
		roster->GetOutputFormats(translators[i], &t_formats,&t_num);
		//printf("and %d output formats \n",t_num);
		for (j=0;j<t_num;j++) {
			if (!strcmp (t_formats[j].MIME,"image/pict")) {
				if (t_formats[j].capability>output_cap) {
					output_trans = translators[i];
					output_cap = t_formats[j].capability;
				}
				D(bug("matching output translator found:type:%c%c%c%c group:%c%c%c%c quality:%f capability:%f MIME:%s name:%s\n",
					t_formats[j].type>>24,t_formats[j].type>>16,t_formats[j].type>>8,t_formats[j].type,
					t_formats[j].group>>24,t_formats[j].group>>16,t_formats[j].group>>8,t_formats[j].group,
					t_formats[j].quality,
					t_formats[j].capability,t_formats[j].MIME,
					t_formats[j].name));
			}
		}
	} 
	delete [] translators; // clean up our droppings

	// Start clipboard manager thread
	cm_done_sem = create_sem(0, "Clipboard Manager Done");
	cm_thread = spawn_thread(clip_manager, "Clipboard Manager", B_NORMAL_PRIORITY, NULL);
	resume_thread(cm_thread);
}


/*
 *  Deinitialize clipboard
 */

void ClipExit(void)
{
	// Stop clipboard manager
	if (cm_thread > 0) {
		status_t l;
		send_data(cm_thread, MSG_QUIT_CLIP_MANAGER, NULL, 0);
		while (wait_for_thread(cm_thread, &l) == B_INTERRUPTED) ;
	}

	// Delete semaphores
	delete_sem(cm_done_sem);
}


/*
 *  Mac application wrote to clipboard
 */

void PutScrap(uint32 type, void *scrap, int32 length)
{
	D(bug("PutScrap type %08lx, data %p, length %ld\n", type, scrap, length));
	if (we_put_this_data) {
		we_put_this_data = false;
		return;
	}
	if (length <= 0)
		return;

	switch (type) {
		case 'TEXT':
			D(bug(" clipping TEXT\n"));
			cm_scrap = scrap;
			cm_length = length;
			while (send_data(cm_thread, MSG_PUT_TEXT, NULL, 0) == B_INTERRUPTED) ;
			while (acquire_sem(cm_done_sem) == B_INTERRUPTED) ;
			break;

		case 'PICT':
			D(bug(" clipping PICT\n"));
			//!! this has to be converted to use the Clipboard Manager
#if 0
			if (be_clipboard->Lock()) {
				be_clipboard->Clear();
				BMessage *clipper = be_clipboard->Data();				
	// Waaaah! This crashes!
				if (input_cap > 0) {		// if there is an converter for PICT datatype convert data to bitmap.
					BMemoryIO *in_buffer = new BMemoryIO(scrap, length);
					BMallocIO *out_buffer = new BMallocIO();
					status_t result=roster->Translate(in_buffer,&input_info,NULL,out_buffer,B_TRANSLATOR_BITMAP);
					clipper->AddData("image/x-be-bitmap", B_MIME_TYPE, out_buffer->Buffer(), out_buffer->BufferLength());
					D(bug("conversion result:%08x buffer_size:%d\n",result,out_buffer->BufferLength()));
					delete in_buffer;
					delete out_buffer;
				}
				clipper->AddData("image/pict", B_MIME_TYPE, scrap, length);
				be_clipboard->Commit();
				be_clipboard->Unlock();
			}
#endif
			break;
	}
}

/*
 * Mac application zeroes clipboard
 */

void ZeroScrap()
{

}

/*
 *  Mac application reads clipboard
 */

void GetScrap(void **handle, uint32 type, int32 offset)
{
	M68kRegisters r;
	D(bug("GetScrap handle %p, type %08lx, offset %ld\n", handle, type, offset));
	return;	//!! GetScrap is currently broken (should use Clipboard Manager)
			//!! replace with clipboard notification in BeOS R4.1

	switch (type) {
		case 'TEXT':
			D(bug(" clipping TEXT\n"));
			if (be_clipboard->Lock()) {
				BMessage *clipper = be_clipboard->Data(); 
				char *clip;
				ssize_t length;

				// Check if we already copied this data
				if (clipper->HasData("application/x-SheepShaver-cookie", B_MIME_TYPE))
					return;
				bigtime_t cookie = system_time();
				clipper->AddData("application/x-SheepShaver-cookie", B_MIME_TYPE, &cookie, sizeof(bigtime_t)); 

				// No, is there text in it?
				if (clipper->FindData("text/plain", B_MIME_TYPE, &clip, &length) == B_OK) {
					D(bug(" text/plain found\n"));

					// Convert text from UTF-8 to Mac charset
					int32 src_length = length;
					int32 dest_length = length;
					int32 state = 0;
					char *outbuf = new char[dest_length];
					if (convert_from_utf8(B_MAC_ROMAN_CONVERSION, clip, &src_length, outbuf, &dest_length, &state) == B_OK) {
						for (int i=0; i<dest_length; i++)
							if (outbuf[i] == 10)
								outbuf[i] = 13;

						// Add text to Mac clipboard
						static uint16 proc[] = {
							0x598f,					// subq.l	#4,sp
							0xa9fc,					// ZeroScrap()
							0x2f3c, 0, 0,			// move.l	#length,-(sp)
							0x2f3c, 'TE', 'XT',		// move.l	#'TEXT',-(sp)
							0x2f3c, 0, 0,			// move.l	#outbuf,-(sp)
							0xa9fe,					// PutScrap()
							0x588f,					// addq.l	#4,sp
							M68K_RTS
						};
						*(int32 *)(proc + 3) = dest_length;
						*(char **)(proc + 9) = outbuf;
						we_put_this_data = true;
						Execute68k((uint32)proc, &r);
					} else {
						D(bug(" text conversion failed\n"));
					}
					delete[] outbuf;
				}
				be_clipboard->Commit();
				be_clipboard->Unlock();
			}
			break;

		case 'PICT':
			D(bug(" clipping PICT\n"));
			if (be_clipboard->Lock()) {
				BMessage *clipper = be_clipboard->Data(); 
				char *clip;
				ssize_t length;

				// Check if we already copied this data
				if (clipper->HasData("application/x-SheepShaver-cookie", B_MIME_TYPE))
					return;
				bigtime_t cookie = system_time();
				clipper->AddData("application/x-SheepShaver-cookie", B_MIME_TYPE, &cookie, sizeof(bigtime_t)); 

				static uint16 proc2[] = {
					0x598f,					// subq.l	#4,sp
					0xa9fc,					// ZeroScrap()
					0x2f3c, 0, 0,			// move.l	#length,-(sp)
					0x2f3c, 'PI', 'CT',		// move.l	#'PICT',-(sp)
					0x2f3c, 0, 0,			// move.l	#buf,-(sp)
					0xa9fe,					// PutScrap()
					0x588f,					// addq.l	#4,sp
					M68K_RTS
				};

				// No, is there a pict ?
				if (clipper->FindData("image/pict", B_MIME_TYPE, &clip, &length) == B_OK ) {
					D(bug(" image/pict found\n"));

					// Add pict to Mac clipboard
					*(int32 *)(proc2 + 3) = length;
					*(char **)(proc2 + 9) = clip;
					we_put_this_data = true;
					Execute68k((uint32)proc2, &r);
#if 0
				// No, is there a bitmap ?
				} else if (clipper->FindData("image/x-be-bitmap", B_MIME_TYPE, &clip, &length) == B_OK || output_cap > 0) {
					D(bug(" image/x-be-bitmap found\nstarting conversion to PICT\n"));

					BMemoryIO *in_buffer = new BMemoryIO(clip, length);
					BMallocIO *out_buffer = new BMallocIO();
					status_t result=roster->Translate(output_trans,in_buffer,NULL,out_buffer,'PICT');
					D(bug("result of conversion:%08x buffer_size:%d\n",result,out_buffer->BufferLength()));

					// Add pict to Mac clipboard
					*(int32 *)(proc2 + 3) = out_buffer->BufferLength();
					*(char **)(proc2 + 9) = (char *)out_buffer->Buffer();
					we_put_this_data = true;
					Execute68k(proc2, &r);

					delete in_buffer;
					delete out_buffer;
#endif
				}
				be_clipboard->Commit();
				be_clipboard->Unlock();
			}
			break;
	}
}
