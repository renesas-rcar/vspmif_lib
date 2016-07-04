#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include "vspm_public.h"
#include "mmngr_user_public.h"

#define NUM_FRAME 6
#define NUM_PLANE 3
#define NUM_RPF   3

sem_t start_sem;

static void cb_func(
	unsigned long uwJobId, long wResult, void *uwUserData)
{
	int *tmp, *tmp2;
	int default_null = 0xAA;
	tmp = (int *)uwUserData;

	if(tmp != NULL) {
		tmp2 = (int *)(unsigned long)(*tmp);
	} else{
		tmp2 = (int *)(unsigned long)(default_null);
	}
	printf("callback2_start:%d %x\n",wResult,(int)(unsigned long)tmp2);

	sem_post(&start_sem);
}

int main(int argc, char *argv[])
{
	struct fdp_start_t *para_test_start = NULL;
	struct fdp_fproc_t *para_test_fproc = NULL;
	struct fdp_seq_t *para_test_seq = NULL;
	struct fdp_pic_t *para_test_in_pic = NULL;
	struct fdp_refbuf_t *para_test_refbuf = NULL;
	struct fdp_imgbuf_t str_refimgbuf[3];
	struct fdp_imgbuf_t str_outimgbuf;

	struct vspm_init_t init_par;
	struct vspm_job_t vspm_ip;
	struct vspm_init_fdp_t init_fdp;

	void *handle;
	unsigned long jobid;

	MMNGR_ID mmngr_fd;
	unsigned int mmngr_hw;
	void* mmngr_cpu;

	MMNGR_ID stlmsk_fd;
	unsigned int stlmsk_hw;
	void* stlmsk_cpu;

	int ercd;
	long rt_code;
	int x,y;
	int in_fsize;
	int input_hsize, input_vsize;
	int out_maxsize;
	int mmngr_size, input_area_size;
	unsigned int RefHw[NUM_FRAME][NUM_PLANE], OutHw[NUM_FRAME][NUM_PLANE];
	unsigned long *pRefCpu[NUM_FRAME][NUM_PLANE], *pOutCpu[NUM_FRAME][NUM_PLANE];
	unsigned char *dump_pt;
	int curr_frame;
	int callback2_arg;
	int i;
	FILE *fpo;
	char out_file_name[50];

	input_hsize = 80;
	input_vsize = 40;

	in_fsize = input_hsize * input_vsize;
	out_maxsize = input_hsize * input_vsize * 2;

	mmngr_size = in_fsize * NUM_PLANE * NUM_FRAME + out_maxsize * NUM_PLANE * NUM_FRAME;

	/* allocate memory for input/output image buffer */
	ercd = mmngr_alloc_in_user_ext(&mmngr_fd, mmngr_size, &mmngr_hw, &mmngr_cpu, MMNGR_VA_SUPPORT, NULL);
	if (ercd != 0) {
		printf("MMNGR alloc ERROR: code = %d\n", ercd);
		return 0;
	}

	ercd = mmngr_alloc_in_user_ext(&stlmsk_fd, (2*((input_hsize+7)/8)*input_vsize)*2, &stlmsk_hw, &stlmsk_cpu, MMNGR_VA_SUPPORT, NULL);
	if (ercd != 0) {
		printf("MMNGR alloc ERROR: code = %d\n", ercd);
		(void)mmngr_free_in_user_ext(mmngr_fd);
		return 0;
	}

	for(y=0; y<NUM_FRAME; y++) {
		for(x=0; x<3; x++) {
			RefHw  [y][x] = mmngr_hw  + (unsigned int)((3*y + x)* in_fsize);
			pRefCpu[y][x] = (unsigned long *)(mmngr_cpu + (3*y + x)* in_fsize);
		}
	}

	input_area_size = in_fsize * NUM_PLANE * NUM_FRAME;

	for(y=0; y<NUM_FRAME; y++) {
		for(x=0; x<3; x++) {
			OutHw  [y][x] = mmngr_hw  + (unsigned int)(input_area_size + (3*y + x)* out_maxsize);
			pOutCpu[y][x] = (unsigned long *)(mmngr_cpu + input_area_size + (3*y + x)* out_maxsize);
		}
	}

	/* Generate input image                           */
	/* Fill even line white and odd line black        */
	/* format:FDP_YUV420 (NV12)(no use pRefCpu[*][2]) */
	for(y=0; y<NUM_FRAME; y++) {
		for(x=0;x<in_fsize;x++) {
			*((unsigned char *)pRefCpu[y][0] + x) = (y % 2)? 0: 255;
			*((unsigned char *)pRefCpu[y][1] + x) = 0x80;
			*((unsigned char *)pRefCpu[y][2] + x) = 0x80;
		}
	}

	/* initialize output buffer */
	for(y=0; y<NUM_FRAME; y++) {
		for(x=0;x<out_maxsize;x++) {
			*((unsigned char *)pOutCpu[y][0] + x) = 0x55;
			*((unsigned char *)pOutCpu[y][1] + x) = 0xAA;
			*((unsigned char *)pOutCpu[y][2] + x) = 0xAA;
		}
	}

	sem_init(&start_sem, 0, 0);

	init_fdp.hard_addr[0] = stlmsk_hw;
	init_fdp.hard_addr[1] = stlmsk_hw + (2*((input_hsize+7)/8)*input_vsize);

	init_par.use_ch = VSPM_EMPTY_CH;
	init_par.mode = VSPM_MODE_MUTUAL;
	init_par.type = VSPM_TYPE_FDP_AUTO;
	init_par.par.fdp = &init_fdp;
	rt_code = vspm_init_driver(&handle, &init_par);
	if (rt_code) {
		printf("vspm_init_driver() Failed!! ercd=%d\n", rt_code);
		goto exit;
	}

	/* make start parameter */
	if ( (para_test_start = (struct fdp_start_t *)malloc(sizeof(struct fdp_start_t))) == NULL) {
		printf("Fail malloc (para_test_start)\n");
		goto exit;
	}
	memset(para_test_start, 0x0, sizeof(struct fdp_start_t));

	if ( (para_test_fproc = (struct fdp_fproc_t *)malloc(sizeof(struct fdp_fproc_t))) == NULL) {
		printf("Fail malloc (para_test_fproc)\n");
		goto exit;
	}
	memset(para_test_fproc, 0x0, sizeof(struct fdp_fproc_t));

	if ( (para_test_seq = (struct fdp_seq_t *)malloc(sizeof(struct fdp_seq_t))) == NULL) {
		printf("Fail malloc (para_test_seq)\n");
		goto exit;
	}
	memset(para_test_seq, 0x0, sizeof(struct fdp_seq_t));

	if ( (para_test_in_pic = (struct fdp_pic_t *)malloc(sizeof(struct fdp_pic_t))) == NULL) {
		printf("Fail malloc (para_test_in_pic)\n");
		goto exit;
	}
	memset(para_test_in_pic, 0x0, sizeof(struct fdp_pic_t));

	if ( (para_test_refbuf = (struct fdp_refbuf_t *)malloc(sizeof(struct fdp_refbuf_t))) == NULL) {
		printf("Fail malloc (poara_test_refbuf)\n");
		goto exit;
	}
	memset(para_test_refbuf, 0x0, sizeof(struct fdp_refbuf_t));

	para_test_start->fdpgo = FDP_GO;
	para_test_start->fproc_par = para_test_fproc;

	para_test_fproc->seq_par = para_test_seq;
	para_test_fproc->in_pic = para_test_in_pic;
	para_test_fproc->last_seq_indicator = 0;
	para_test_fproc->current_field = 0;
	para_test_fproc->interpolated_line = 0;
	para_test_fproc->ref_buf = para_test_refbuf;
	para_test_fproc->out_buf = &str_outimgbuf;
	para_test_fproc->out_format = FDP_YUV420;
	para_test_fproc->fcp_par = NULL;

	para_test_seq->seq_mode = FDP_SEQ_INTER;
	para_test_seq->telecine_mode = FDP_TC_OFF;
	para_test_seq->in_width = input_hsize;
	para_test_seq->in_height = input_vsize;

	para_test_in_pic->chroma_format = FDP_YUV420;
	para_test_in_pic->width = input_hsize;
	para_test_in_pic->height = input_vsize;
	para_test_in_pic->progressive_sequence = 0;
	para_test_in_pic->progressive_frame = 0;
	para_test_in_pic->picture_structure = 0;
	para_test_in_pic->repeat_first_field = 0;
	para_test_in_pic->top_field_first = 0;

	para_test_refbuf->next_buf = &str_refimgbuf[0];
	para_test_refbuf->cur_buf = &str_refimgbuf[1];
	para_test_refbuf->prev_buf = &str_refimgbuf[2];

	str_refimgbuf[1].stride = input_hsize;
	str_refimgbuf[1].stride_c = input_hsize;

	str_outimgbuf.stride = input_hsize;
	str_outimgbuf.stride_c = input_hsize;

	/* frame loop */
	for (curr_frame = 0; curr_frame<NUM_FRAME; curr_frame++) {
		/* setting input/output buffer address */
		str_refimgbuf[0].addr    = RefHw[(curr_frame+1) % NUM_FRAME][0];
		str_refimgbuf[1].addr    = RefHw[(curr_frame  ) % NUM_FRAME][0];
		str_refimgbuf[1].addr_c0 = RefHw[(curr_frame  ) % NUM_FRAME][1];
		str_refimgbuf[1].addr_c1 = RefHw[(curr_frame  ) % NUM_FRAME][2];
		str_refimgbuf[2].addr    = RefHw[(curr_frame+NUM_FRAME-1) % NUM_FRAME][0];
		if(para_test_start->fproc_par->out_buf != NULL) {
			str_outimgbuf.addr     = OutHw[curr_frame % NUM_FRAME][0];
			str_outimgbuf.addr_c0  = OutHw[curr_frame % NUM_FRAME][1];
			str_outimgbuf.addr_c1  = OutHw[curr_frame % NUM_FRAME][2];
		}

		/* set NULL to seq_par for same sequence */
		if(curr_frame != 0) {
			para_test_fproc->seq_par = NULL;
		}
		/* for last frame, set last_start to 1 */
		if(curr_frame == NUM_FRAME - 1) {
			para_test_fproc->last_seq_indicator = 1;
		}

		callback2_arg = curr_frame + 10;

		memset(&vspm_ip, 0, sizeof(struct vspm_job_t));
		vspm_ip.type = VSPM_TYPE_FDP_AUTO;
		vspm_ip.par.fdp = para_test_start;

		rt_code = vspm_entry_job(handle, &jobid, 126, &vspm_ip, (void *)&callback2_arg, cb_func);
		if (rt_code)
			printf("vspm_entry_job() Failed!! ercd=%d\n", rt_code);

		/* wait callback2 */
		sem_wait(&start_sem);

		/* dump output data */
		sprintf(out_file_name,"out_data_%03d.yuv",curr_frame);
		if((fpo = fopen(out_file_name,"w")) == NULL) {
			printf("Can not open file:%s\n",out_file_name);
		} else {
			for(i=0;i<input_vsize*2;i++){
				dump_pt = (unsigned char *)pOutCpu[curr_frame][0] + i*input_hsize;
				fwrite(dump_pt, sizeof(char),input_hsize,fpo);
			}
			for(i=0;i<input_vsize;i++){
				dump_pt = (unsigned char *)pOutCpu[curr_frame][1] + i*input_hsize;
				fwrite(dump_pt, sizeof(char),input_hsize,fpo);
			}
		    fclose(fpo);
		}

		/* current field change */
		para_test_fproc->current_field = (para_test_fproc->current_field)? 0: 1;
	}

	/* end of frame */
	rt_code = vspm_quit_driver(handle);

exit:
	if(para_test_refbuf != NULL) {
		free(para_test_refbuf);
	}
	if(para_test_in_pic != NULL) {
		free(para_test_in_pic);
	}
	if(para_test_seq != NULL) {
		free(para_test_seq);
	}
	if(para_test_fproc != NULL) {
		free(para_test_fproc);
	}
	if(para_test_start != NULL) {
		free(para_test_start);
	}

	/* free memory */
	ercd = mmngr_free_in_user_ext(stlmsk_fd);
	if (ercd != 0) {
		printf("MMNGR free ERROR: code = %d\n", ercd);
	}

	ercd = mmngr_free_in_user_ext(mmngr_fd);
	if (ercd != 0) {
		printf("MMNGR free ERROR: code = %d\n",ercd);
		return 0;
	}

	sem_destroy(&start_sem);
	return 0;
}
