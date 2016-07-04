/* 
 * Copyright (c) 2015-2016 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "vspm_public.h"
#include "mmngr_user_public.h"

typedef void (*FDPM_CBFUNC1)(T_FDP_CB1 *);
typedef void (*FDPM_CBFUNC2)(T_FDP_CB2 *);

struct vspm_if_fdp_handle_t {
	void *handle;
	MMNGR_ID stlmsk_fd;
	pthread_mutex_t mutex;
	struct vspm_if_fdp_cb_t *cb_list;

	int id;
	struct vspm_if_fdp_handle_t *next;
};

struct vspm_if_fdp_par_t {
	struct vspm_job_t job_par;
	struct fdp_start_t start_par;
	struct vspm_if_fdp_fproc_par_t {
		struct fdp_fproc_t fproc;
		struct fdp_seq_t seq;
		struct fdp_pic_t pic;
		struct fdp_imgbuf_t out_buf;
		struct vspm_if_fdp_ref_buf_t {
			struct fdp_refbuf_t ref_buf;
			struct fdp_imgbuf_t next;
			struct fdp_imgbuf_t cur;
			struct fdp_imgbuf_t prev;
		} ref_buf;
	} fproc;
};

struct vspm_if_fdp_cb_t {
	struct vspm_if_fdp_handle_t *parent_hdl;
	struct vspm_if_fdp_par_t fdp_par;
	void *user_data;
	FDPM_CBFUNC2 cb_func;
	unsigned long job_id;
	struct vspm_if_fdp_cb_t *next;
};

pthread_mutex_t fdp_hdl_mutex = PTHREAD_MUTEX_INITIALIZER;
struct vspm_if_fdp_handle_t *hdl_list = NULL;
int seed_id = 0;


static int vspm_if_add_handle(struct vspm_if_fdp_handle_t *hdl)
{
	pthread_mutex_lock(&fdp_hdl_mutex);
	hdl->id = ++seed_id;

	hdl->next = hdl_list;
	hdl_list = hdl;
	pthread_mutex_unlock(&fdp_hdl_mutex);

	return hdl->id;
}


static struct vspm_if_fdp_handle_t *vspm_if_get_handle(int id)
{
	struct vspm_if_fdp_handle_t *hdl;

	hdl = hdl_list;
	while (hdl != NULL) {
		if (hdl->id == id) {
			break;
		}

		hdl = hdl->next;
	}

	return hdl;
}


static struct vspm_if_fdp_handle_t *vspm_if_del_handle(int id)
{
	struct vspm_if_fdp_handle_t *hdl;
	struct vspm_if_fdp_handle_t *pre_hdl;

	pthread_mutex_lock(&fdp_hdl_mutex);
	hdl = hdl_list;
	pre_hdl = NULL;
	while (hdl != NULL) {
		if (hdl->id == id) {
			if (pre_hdl == NULL) {
				hdl_list = hdl->next;
			} else {
				pre_hdl->next = hdl->next;
			}
			break;
		}

		pre_hdl = hdl;
		hdl = hdl->next;
	}
	pthread_mutex_unlock(&fdp_hdl_mutex);

	return hdl;
}


static void vspm_if_add_fdp_cb_info(
	struct vspm_if_fdp_handle_t *hdl, struct vspm_if_fdp_cb_t *set_cb)
{
	pthread_mutex_lock(&hdl->mutex);

	/* regist parent handle */
	set_cb->parent_hdl = hdl;

	/* regist callback list */
	set_cb->next = hdl->cb_list;
	hdl->cb_list = set_cb;

	pthread_mutex_unlock(&hdl->mutex);
}


static long vspm_if_del_fdp_cb_info(struct vspm_if_fdp_cb_t *target_cb)
{
	struct vspm_if_fdp_handle_t *hdl = target_cb->parent_hdl;
	struct vspm_if_fdp_cb_t *cb;
	struct vspm_if_fdp_cb_t *pre_cb;

	long ercd = R_FDPM_NG;

	pthread_mutex_lock(&hdl->mutex);

	cb = hdl->cb_list;
	pre_cb = NULL;

	while (cb != NULL) {
		if (cb == target_cb) {
			if (pre_cb == NULL) {
				hdl->cb_list = cb->next;
			} else {
				pre_cb->next = cb->next;
			}
			ercd = R_FDPM_OK;
			break;
		}

		pre_cb = cb;
		cb = cb->next;
	}

	pthread_mutex_unlock(&hdl->mutex);

	return ercd;
}


static void vspm_if_set_fdp_seq_param(
	struct fdp_seq_t *seq_par, T_FDP_SEQ *old_seq_par)
{
	if (old_seq_par->seq_mode == FDP_SEQ_PROG) {
		seq_par->seq_mode = FDP_SEQ_PROG;
	} else if ((old_seq_par->seq_mode == FDP_SEQ_INTER) ||
		(old_seq_par->seq_mode == FDP_SEQ_INTERH)) {
		seq_par->seq_mode = FDP_SEQ_INTER;
	} else if ((old_seq_par->seq_mode == FDP_SEQ_INTER_2D) ||
		(old_seq_par->seq_mode == FDP_SEQ_INTERH_2D)) {
		seq_par->seq_mode = FDP_SEQ_INTER_2D;
	} else {
		seq_par->seq_mode = old_seq_par->seq_mode;
	}

	seq_par->in_width = old_seq_par->in_width;
	seq_par->in_height = old_seq_par->in_height;
}


static void vspm_if_set_fdp_pic_param(
	struct fdp_pic_t *pic_par, T_FDP_PIC *old_pic_par)
{
	pic_par->picid = old_pic_par->picid;

	if (old_pic_par->pic_par != NULL) {
		pic_par->chroma_format = old_pic_par->pic_par->chroma_format;
		pic_par->width = old_pic_par->pic_par->width;
		pic_par->height = old_pic_par->pic_par->height;
		pic_par->progressive_sequence = old_pic_par->pic_par->progressive_sequence;
		pic_par->progressive_frame = old_pic_par->pic_par->progressive_frame;
		pic_par->picture_structure = old_pic_par->pic_par->picture_structure;
		pic_par->repeat_first_field = old_pic_par->pic_par->repeat_first_field;
		pic_par->top_field_first = old_pic_par->pic_par->top_field_first;
	}
}


static void vspm_if_set_fdp_img_param(
	struct fdp_imgbuf_t *img_par, T_FDP_IMGBUF *old_img_par)
{
	unsigned long tmp;

	tmp = (unsigned long)old_img_par->addr;
	img_par->addr = (unsigned int)(tmp & 0xffffffff);
	tmp = (unsigned long)old_img_par->addr_c0;
	img_par->addr_c0 = (unsigned int)(tmp & 0xffffffff);
	tmp = (unsigned long)old_img_par->addr_c1;
	img_par->addr_c1 = (unsigned int)(tmp & 0xffffffff);
	img_par->stride = old_img_par->stride;
	img_par->stride_c = old_img_par->stride_c;
}


static void vspm_if_set_fdp_ref_param(
	struct vspm_if_fdp_ref_buf_t *par, T_FDP_REFBUF *old_ref_buf)
{
	struct fdp_refbuf_t *ref_buf = &par->ref_buf;

	if (old_ref_buf->buf_refrd0 != NULL) {
		vspm_if_set_fdp_img_param(&par->next, old_ref_buf->buf_refrd0);
		ref_buf->next_buf = &par->next;
	}

	if (old_ref_buf->buf_refrd1 != NULL) {
		vspm_if_set_fdp_img_param(&par->cur, old_ref_buf->buf_refrd1);
		ref_buf->cur_buf = &par->cur;
	}

	if (old_ref_buf->buf_refrd2 != NULL) {
		vspm_if_set_fdp_img_param(&par->prev, old_ref_buf->buf_refrd2);
		ref_buf->prev_buf = &par->prev;
	}
}


static void vspm_if_set_fdp_fproc_param(
	struct vspm_if_fdp_fproc_par_t *par, T_FDP_FPROC *old_fproc_par)
{
	struct fdp_fproc_t *fproc_par = &par->fproc;

	if (old_fproc_par->seq_par != NULL) {
		vspm_if_set_fdp_seq_param(&par->seq, old_fproc_par->seq_par);
		if (old_fproc_par->f_decodeseq == 1) {
			par->seq.telecine_mode = FDP_TC_FORCED_PULL_DOWN;
		} else {
			par->seq.telecine_mode = FDP_TC_OFF;
		}
		fproc_par->seq_par = &par->seq;
	}

	if (old_fproc_par->in_pic != NULL) {
		vspm_if_set_fdp_pic_param(&par->pic, old_fproc_par->in_pic);
		fproc_par->in_pic = &par->pic;
	}

	if (old_fproc_par->out_buf != NULL) {
		vspm_if_set_fdp_img_param(&par->out_buf, old_fproc_par->out_buf);
		fproc_par->out_buf = &par->out_buf;
	}

	if (old_fproc_par->ref_buf != NULL) {
		vspm_if_set_fdp_ref_param(&par->ref_buf, old_fproc_par->ref_buf);
		fproc_par->ref_buf = &par->ref_buf.ref_buf;
	}

	fproc_par->last_seq_indicator = old_fproc_par->last_start;
	fproc_par->current_field = old_fproc_par->cf;
	fproc_par->interpolated_line = 0;
	fproc_par->out_format = old_fproc_par->out_format;
	fproc_par->fcp_par = NULL;
}


static void vspm_if_set_fdp_param(
	struct vspm_if_fdp_par_t *fdp_par, T_FDP_START *src)
{
	struct fdp_start_t *st_par = &fdp_par->start_par;

	st_par->fdpgo = src->fdpgo;
	if (src->fproc_par) {
		vspm_if_set_fdp_fproc_param(&fdp_par->fproc, src->fproc_par);
		st_par->fproc_par = &fdp_par->fproc.fproc;
	}

	fdp_par->job_par.type = VSPM_TYPE_FDP_AUTO;
	fdp_par->job_par.par.fdp = st_par;
}


int drv_FDPM_Open(
	void *callback2,
	void *callback3,
	void *callback4,
	T_FDP_OPEN *open_par,
	FP user_function,
	void *userdata2,
	void *userdata3,
	void *userdata4,
	int *sub_ercd)
{
	struct vspm_init_t init_par;
	struct vspm_init_fdp_t fdp_par;

	struct vspm_if_fdp_handle_t *hdl;

	size_t stlmsk_size;
	unsigned int stlmsk_hard;
	void *stlmsk_virt;

	int rtcd = R_FDPM_OK;
	long ercd;

	/* check parameter */
	if (open_par == NULL) {
		return -EINVAL;
	}

	/* check vsync mode setting */
	switch (open_par->vmode) {
	case FDP_VMODE_VBEST:
		init_par.use_ch = VSPM_EMPTY_CH;
		break;
	case FDP_VMODE_VBEST_FDP0:
		init_par.use_ch = VSPM_USE_CH0;
		break;
	case FDP_VMODE_VBEST_FDP1:
		init_par.use_ch = VSPM_USE_CH1;
		break;
	case FDP_VMODE_VBEST_FDP2:
		init_par.use_ch = VSPM_USE_CH2;
		break;
	case FDP_VMODE_NORMAL:
		if (sub_ercd != NULL) {
			*sub_ercd = E_FDP_PARA_VMODE;
		}
	default:
		return -EINVAL;
	}

	if (sub_ercd == NULL) {
		return -EINVAL;
	}

	/* check occupy mode setting */
	if (open_par->ocmode == FDP_OCMODE_OCCUPY) {
		init_par.mode = VSPM_MODE_OCCUPY;
	} else if (open_par->ocmode == FDP_OCMODE_COMMON) {
		init_par.mode = VSPM_MODE_MUTUAL;
	} else {
		return -EINVAL;
	}

	/* check size */
	if (open_par->insize == NULL) {
		return -EINVAL;
	}

	if ((open_par->insize->width < 80) ||
		(open_par->insize->width > 1920) ||
		(open_par->insize->width % 2)) {
		return -EINVAL;
	}

	if ((open_par->insize->height < 40) ||
		(open_par->insize->height > 1920)) {
		return -EINVAL;
	}

	/* allocate memory */
	hdl = malloc(sizeof(struct vspm_if_fdp_handle_t));
	if (hdl == NULL) {
		return -ENOMEM;
	}
	memset(hdl, 0, sizeof(struct vspm_if_fdp_handle_t));

	/* initialize mutex */
	(void)pthread_mutex_init(&hdl->mutex, NULL);

	/* set parameter */
	stlmsk_size = 2 * ((open_par->insize->width + 7) / 8) * open_par->insize->height;
	rtcd = mmngr_alloc_in_user_ext(
		&hdl->stlmsk_fd, stlmsk_size * 2, &stlmsk_hard, &stlmsk_virt, MMNGR_VA_SUPPORT, NULL);
	if (rtcd) {
		free(hdl);
		return -ENOMEM;
	}

	fdp_par.hard_addr[0] = stlmsk_hard;
	fdp_par.hard_addr[1] = stlmsk_hard + stlmsk_size;

	init_par.type = VSPM_TYPE_FDP_AUTO;
	init_par.par.fdp = &fdp_par;

	/* initialize VSP manager */
	ercd = vspm_init_driver(&hdl->handle, &init_par);
	switch (ercd) {
	case R_VSPM_OK:
		break;
	case R_VSPM_PARAERR:
	case R_VSPM_ALREADY_USED:
		rtcd = -EINVAL;
		goto err_exit;
	default:
		rtcd = R_FDPM_NG;
		goto err_exit;
	}

	/* add list */
	*sub_ercd = vspm_if_add_handle(hdl);

	/* call user function */
	if (user_function != NULL) {
		user_function();
	}

	return R_FDPM_OK;

err_exit:
	(void)mmngr_free_in_user_ext(hdl->stlmsk_fd);
	free(hdl);
	return rtcd;
}


int drv_FDPM_Close(
	FP user_function,
	int *sub_ercd,
	unsigned char f_release)
{
	struct vspm_if_fdp_handle_t *hdl;
	struct vspm_if_fdp_cb_t *cb;

	long ercd;

	/* check parameter */
	if (sub_ercd == NULL) {
		return -EINVAL;
	}

	/* delete handle */
	hdl = vspm_if_del_handle(*sub_ercd);
	if (hdl == NULL) {
		return -EACCES;
	}

	/* finalize VSP manager */
	ercd = vspm_quit_driver(hdl->handle);
	if (ercd) {
		return R_FDPM_NG;
	}

	/* release memory of unnotified callback */
	while (hdl->cb_list != NULL) {
		cb = hdl->cb_list;
		ercd = vspm_if_del_fdp_cb_info(cb);
		if (ercd == R_FDPM_OK) {
			free(cb);
		}
	}

	/* release memory */
	(void)mmngr_free_in_user_ext(hdl->stlmsk_fd);
	free(hdl);

	/* call user function */
	if (user_function != NULL) {
		user_function();
	}

	return R_FDPM_OK;
}


static void vspm_if_fdp_cb_func(
	unsigned long job_id, long result, void *user_data)
{
	struct vspm_if_fdp_cb_t *cb_info =
		(struct vspm_if_fdp_cb_t *)user_data;
	T_FDP_CB2 cb2;

	long ercd;

	/* check parameter */
	if (cb_info == NULL) {
		return;
	}

	/* disconnect callback information from handle */
	ercd = vspm_if_del_fdp_cb_info(cb_info);
	if (ercd != R_FDPM_OK) {
		/* already cancel? */
		return;
	}

	cb2.ercd = (int)result;
	cb2.userdata2 = cb_info->user_data;

	/* callback function */
	if (cb_info->cb_func != NULL) {
		cb_info->cb_func(&cb2);
	}

	/* release memory */
	free(cb_info);
}


int drv_FDPM_Start(
	void *callback1,
	void *callback2,
	T_FDP_START *start_par,
	void *userdata1,
	void *userdata2,
	int *sub_ercd)
{
	struct vspm_if_fdp_handle_t *hdl;
	struct vspm_if_fdp_cb_t *cb_info;

	FDPM_CBFUNC1 cb1_func;
	T_FDP_CB1 cb1;

	int rtcd;
	long ercd;

	/* check parameter */
	if (sub_ercd == NULL) {
		return -EINVAL;
	}

	if (start_par == NULL) {
		return -EINVAL;
	}

	/* allocate memory */
	cb_info = malloc(sizeof(struct vspm_if_fdp_cb_t));
	if (cb_info == NULL) {
		return -ENOMEM;
	}
	memset(cb_info, 0, sizeof(struct vspm_if_fdp_cb_t));

	/* set entry parameter */
	vspm_if_set_fdp_param(&cb_info->fdp_par, start_par);

	/* set callback parameter */
	cb_info->user_data = userdata2;
	cb_info->cb_func = callback2;

	pthread_mutex_lock(&fdp_hdl_mutex);

	/* get handle */
	hdl = vspm_if_get_handle(*sub_ercd);
	if (hdl == NULL) {
		rtcd = -EACCES;
		goto err_exit1;
	}

	/* connect callback information to handle */
	vspm_if_add_fdp_cb_info(hdl, cb_info);

	/* entry job */
	ercd = vspm_entry_job(
		hdl->handle,
		&cb_info->job_id,
		120,
		&cb_info->fdp_par.job_par,
		(void *)cb_info,
		vspm_if_fdp_cb_func);


	/* convert error code */
	switch (ercd) {
	case R_VSPM_OK:
		break;
	case R_VSPM_PARAERR:
		rtcd = -EINVAL;
		goto err_exit2;
	case R_VSPM_NG:
	case R_VSPM_QUE_FULL:
	default:
		rtcd = R_FDPM_NG;
		goto err_exit2;
	}

	pthread_mutex_unlock(&fdp_hdl_mutex);

	/* callback */
	if (callback1 != NULL) {
		cb1_func = callback1;

		memset(&cb1, 0, sizeof(T_FDP_CB1));
		cb1.userdata1 = userdata1;

		cb1_func(&cb1);
	}

	return R_FDPM_OK;

err_exit2:
	(void)vspm_if_del_fdp_cb_info(cb_info);

err_exit1:
	pthread_mutex_unlock(&fdp_hdl_mutex);
	free(cb_info);
	return rtcd;
}


int drv_FDPM_Cancel(int id, int *sub_ercd)
{
	struct vspm_if_fdp_handle_t *hdl;
	struct vspm_if_fdp_cb_t *cb;
	long ercd;

	/* check parameter */
	if (sub_ercd == NULL) {
		return -EINVAL;
	}

	pthread_mutex_lock(&fdp_hdl_mutex);

	/* get handle */
	hdl = vspm_if_get_handle(*sub_ercd);
	if (hdl == NULL) {
		pthread_mutex_unlock(&fdp_hdl_mutex);
		return -EACCES;
	}

	/* cancel all jobs connected handle */
	while (hdl->cb_list != NULL) {
		cb = hdl->cb_list;
		ercd = vspm_if_del_fdp_cb_info(cb);
		if (ercd == R_FDPM_OK) {
			(void)vspm_cancel_job(hdl->handle, cb->job_id);
			free(cb);
		}
	}

	pthread_mutex_unlock(&fdp_hdl_mutex);

	return R_FDPM_OK;
}


int drv_FDPM_Status(T_FDP_STATUS *fdp_status, int *sub_ercd)
{
	struct vspm_if_fdp_handle_t *hdl;
	struct vspm_status_t status;
	struct fdp_status_t fdp;
	long ercd;
	int rtcd;

	/* check parameter */
	if (fdp_status == NULL) {
		return -EINVAL;
	}

	/* check handle list */
	if (hdl_list == NULL) {
		memset(fdp_status, 0, sizeof(T_FDP_STATUS));
		if (sub_ercd != NULL) {
			*sub_ercd = 0;
		}
		return R_FDPM_OK;
	}

	/* check parameter */
	if (sub_ercd == NULL) {
		return -EINVAL;
	}

	pthread_mutex_lock(&fdp_hdl_mutex);

	/* get handle */
	hdl = vspm_if_get_handle(*sub_ercd);
	if (hdl == NULL) {
		rtcd = -EACCES;
		goto err_exit;
	}

	/* get status */
	status.fdp = &fdp;
	ercd = vspm_get_status(hdl->handle, &status);
	if (ercd != R_VSPM_OK) {
		rtcd = R_FDPM_NG;
		goto err_exit;
	}

	pthread_mutex_unlock(&fdp_hdl_mutex);

	/* set status information */
	fdp_status->status = FDPM_RDY;
	fdp_status->delay = 0;
	fdp_status->vcycle = (unsigned long)fdp.vcycle;
	fdp_status->vintcnt = 0;
	fdp_status->seq_lock = FDP_SEQ_UNLOCK;
	fdp_status->in_enable = FDP_IN_ENABLE;
	fdp_status->in_picid = fdp.picid;
	fdp_status->in_left = 0;
	fdp_status->out_enable = FDP_OUT_ENABLE;
	fdp_status->out_picid = fdp.picid;
	fdp_status->out_left = 0;
	fdp_status->out_req = FDP_OUT_REQ;

	return R_FDPM_OK;

err_exit:
	pthread_mutex_unlock(&fdp_hdl_mutex);
	return rtcd;
}
